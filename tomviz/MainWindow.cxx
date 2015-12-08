/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ui_AboutDialog.h"

#include "pqFiltersMenuReaction.h"
#include "pqMacroReaction.h"
#include "pqProxyGroupMenuManager.h"
#include "pqProxyGroupMenuManager.h"
#include "pqPVApplicationCore.h"
#include "pqPythonShellReaction.h"
#include "pqSaveAnimationReaction.h"
#include "pqSaveScreenshotReaction.h"
#include "pqSaveStateReaction.h"
#include "vtkPVPlugin.h"
#include "vtkSMSettings.h"

#include "tomvizConfig.h"
#include "ActiveObjects.h"
#include "AddAlignReaction.h"
#include "AddRotateAlignReaction.h"
#include "AddExpressionReaction.h"
#include "AddPythonTransformReaction.h"
#include "AddResampleReaction.h"
#include "AddRotateAlignReaction.h"
#include "Behaviors.h"
#include "CropReaction.h"
#include "CloneDataReaction.h"
#include "DataPropertiesPanel.h"
#include "DeleteDataReaction.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "ModuleMenu.h"
#include "ModulePropertiesPanel.h"
#include "PythonGeneratedDatasetReaction.h"
#include "RecentFilesMenu.h"
#include "ReconstructionReaction.h"
#include "ResetReaction.h"
#include "SaveDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "SetScaleReaction.h"
#include "SetTiltAnglesReaction.h"
#include "ToggleDataTypeReaction.h"
#include "Utilities.h"
#include "ViewMenuManager.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QTimer>

#if QT_VERSION >= 0x050000
  #include <QStandardPaths>
#else
  #include <QDesktopServices>
#endif

//we are building with dax, so we have plugins to import
#ifdef DAX_DEVICE_ADAPTER
  // Adds required forward declarations.
  PV_PLUGIN_IMPORT_INIT(tomvizThreshold);
  PV_PLUGIN_IMPORT_INIT(tomvizStreaming);
#endif

namespace
{
QString getAutosaveFile()
{
  // workaround to get user config location
  QString dataPath;
#if QT_VERSION >= 0x050000
  dataPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
  dataPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
  QDir dataDir(dataPath);
  if (!dataDir.exists())
  {
    dataDir.mkpath(dataPath);
  }
  return dataDir.absoluteFilePath(".tomviz_autosave.tvsm");
}
}

namespace tomviz
{

class MainWindow::MWInternals
{
public:
  MWInternals() : AboutDialog(nullptr) { ; }

  Ui::MainWindow Ui;
  Ui::AboutDialog AboutUi;
  QDialog *AboutDialog;
  QTimer *Timer;
};

//-----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* _parent, Qt::WindowFlags _flags)
  : Superclass(_parent, _flags),
  Internals(new MainWindow::MWInternals()),
  DataPropertiesWidget(new DataPropertiesPanel(this)),
  ModulePropertiesWidget(new ModulePropertiesPanel(this))
{
  Ui::MainWindow& ui = this->Internals->Ui;
  ui.setupUi(this);
  this->Internals->Timer = new QTimer(this);
  this->connect(this->Internals->Timer, SIGNAL(timeout()), SLOT(autosave()));
  this->Internals->Timer->start(
      5 /*minutes*/ * 60 /*seconds per minute*/ * 1000 /*msec per second*/);
    
  QString version(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0)
    version.append("-").append(TOMVIZ_VERSION_EXTRA);
  setWindowTitle("tomviz " + version);

  QIcon icon(":/icons/tomviz.png");
  setWindowIcon(icon);

  // Link the histogram in the central widget to the active data source.
  ui.centralWidget->connect(&ActiveObjects::instance(),
                            SIGNAL(dataSourceActivated(DataSource*)),
                            SLOT(setActiveDataSource(DataSource*)));
  ui.centralWidget->connect(&ActiveObjects::instance(),
                            SIGNAL(moduleActivated(Module*)),
                            SLOT(setActiveModule(Module*)));
  ui.centralWidget->connect(this->DataPropertiesWidget,
                            SIGNAL(colorMapUpdated()),
                            SLOT(onColorMapUpdated()));

  // connect quit.
  pqApplicationCore::instance()->connect(ui.actionExit, SIGNAL(triggered()),
                                         SLOT(quit()));

  // Connect up the module/data changed to the appropriate slots.
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceActivated(DataSource*)),
          SLOT(dataSourceChanged(DataSource*)));
  connect(&ActiveObjects::instance(), SIGNAL(moduleActivated(Module*)),
          SLOT(moduleChanged(Module*)));
  DataPropertiesWidget->hide();
  ModulePropertiesWidget->hide();

  // Connect the about dialog up too.
  connect(ui.actionAbout, SIGNAL(triggered()), SLOT(showAbout()));

  new pqPythonShellReaction(ui.actionPythonConsole);
  new pqMacroReaction(ui.actionMacros);

  // Instantiate tomviz application behavior.
  new Behaviors(this);

  new LoadDataReaction(ui.actionOpen);

  QAction *setScaleAction = new QAction("Set Data Size", this);
  ui.menuTools->addAction(setScaleAction);
  new SetScaleReaction(setScaleAction);

  // Build Data Transforms menu
  // ################################################################
  QAction *customPythonAction = ui.menuData->addAction("Custom Transform");
  QAction *customPythonITKAction = ui.menuData->addAction("Custom ITK Transform");
  QAction *cropDataAction = ui.menuData->addAction("Crop");
  ui.menuData->addSeparator();
  
  QAction *shiftUniformAction = ui.menuData->addAction("Shift Uniformly");
  QAction *deleteSliceAction = ui.menuData->addAction("Delete Slices");
  QAction *downsampleByTwoAction = ui.menuData->addAction("Downsample x2");
  QAction *resampleAction = ui.menuData->addAction("Resample");
  QAction *rotateAction = ui.menuData->addAction("Rotate");
  QAction *clearAction = ui.menuData->addAction("Clear Volume");
  ui.menuData->addSeparator();

  QAction *squareRootAction = ui.menuData->addAction("Square Root Data");
  QAction *hannWindowAction = ui.menuData->addAction("Hann Window");
  QAction *fftAbsLogAction = ui.menuData->addAction("FFT (abs log)");
  QAction *sobelFilterAction = ui.menuData->addAction("Sobel Filter");
  QAction *laplaceFilterAction = ui.menuData->addAction("Laplace Filter");
  QAction *gaussianFilterAction = ui.menuData->addAction("Gaussian Filter");
  QAction *medianFilterAction = ui.menuData->addAction("Median Filter");
    

  ui.menuData->addSeparator();

  QAction *cloneAction = ui.menuData->addAction("Clone");
  QAction *deleteDataAction = ui.menuData->addAction(
      QIcon(":/QtWidgets/Icons/pqDelete32.png"), "Delete Data & Modules");
  deleteDataAction->setToolTip("Delete Data");

  // Build Tomography menu
  // ################################################################
  QAction *toggleDataTypeAction = ui.menuTomography->addAction("Toggle Data Type");
  ui.menuTomography->addSeparator();

  QAction *setTiltAnglesAction = ui.menuTomography->addAction("Set Tilt Angles");
  ui.menuTomography->addSeparator();

  QAction *dataProcessingLabel = ui.menuTomography->addAction("Pre-reconstruction Processing:");
  dataProcessingLabel->setEnabled(false);
  QAction *autoSubtractBackgroundAction = ui.menuTomography->addAction("Background Subtraction (Auto)");
  QAction *subtractBackgroundAction = ui.menuTomography->addAction("Background Subtraction (Manual)");
  QAction *sobelFilter2DAction = ui.menuTomography->addAction("Sobel Filter");
  QAction *autoAlignAction = ui.menuTomography->addAction("Image Alignment (Auto)");
  QAction *alignAction = ui.menuTomography->addAction("Image Alignment (Manual)");
  QAction *rotateAlignAction = ui.menuTomography->addAction("Tilt Axis Alignment (Manual)");
  ui.menuTomography->addSeparator();

  QAction *reconLabel = ui.menuTomography->addAction("Reconstruction:");
  reconLabel->setEnabled(false);
  QAction *reconDFMAction = ui.menuTomography->addAction("Direct Fourier Method");
  QAction *reconWBPAction = ui.menuTomography->addAction("Weighted Back Projection");
  //QAction *reconWBP_CAction = ui.menuTomography->addAction("Simple Back Projection (C++)");
  QAction *reconARTAction = ui.menuTomography->addAction("Algebraic Reconstruction Technique (ART)");

  ui.menuTomography->addSeparator();
  QAction *generateTiltSeriesAction = ui.menuTomography->addAction("Generate Tilt Series");

  // Set up reactions for Data Transforms Menu
  //#################################################################

  // Add our Python script reactions, these compose Python into menu entries.
  new AddExpressionReaction(customPythonAction);
  new AddExpressionReaction(customPythonITKAction);
  new CropReaction(cropDataAction, this);
  new AddPythonTransformReaction(shiftUniformAction,
                                 "Shift Uniformly", readInPythonScript("Shift_Stack_Uniformly"));
  new AddPythonTransformReaction(deleteSliceAction,
                                   "Delete Slices", readInPythonScript("deleteSlices"));
  new AddPythonTransformReaction(downsampleByTwoAction,
                                   "Downsample x2", readInPythonScript("DownsampleByTwo"));
  new AddPythonTransformReaction(resampleAction,
                                   "Resample", readInPythonScript("Resample"));
  new AddPythonTransformReaction(rotateAction,
                                 "Rotate", readInPythonScript("Rotate3D"));
  new AddPythonTransformReaction(clearAction, "Clear Volume", readInPythonScript("ClearVolume"));

  new AddPythonTransformReaction(squareRootAction,
                                 "Square Root Data", readInPythonScript("Square_Root_Data"));
  new AddPythonTransformReaction(hannWindowAction,
                                 "Hann Window", readInPythonScript("HannWindow3D"));
  new AddPythonTransformReaction(fftAbsLogAction,
                                 "FFT (ABS LOG)", readInPythonScript("FFT_AbsLog"));
  new AddPythonTransformReaction(sobelFilterAction,
                                   "Sobel Filter", readInPythonScript("SobelFilter"));
  new AddPythonTransformReaction(laplaceFilterAction,
                                   "Laplace Filter", readInPythonScript("LaplaceFilter"));
  new AddPythonTransformReaction(gaussianFilterAction,
                                   "Gaussian Filter", readInPythonScript("GaussianFilter"));
  new AddPythonTransformReaction(medianFilterAction,
                                   "Median Filter", readInPythonScript("MedianFilter"));
    
  new CloneDataReaction(cloneAction);
  new DeleteDataReaction(deleteDataAction);
  // Set up reactions for Tomography Menu
  //#################################################################
  new ToggleDataTypeReaction(toggleDataTypeAction, this);
  new SetTiltAnglesReaction(setTiltAnglesAction, this);
  new AddPythonTransformReaction(generateTiltSeriesAction,
                                 "Generate Tilt Series", readInPythonScript("TiltSeries"),
                                 false, true);
  new AddAlignReaction(alignAction);
  new AddPythonTransformReaction(autoSubtractBackgroundAction, "Background Subtraction (Auto)",
                                 readInPythonScript("Subtract_TiltSer_Background_Auto"), true);
  new AddPythonTransformReaction(subtractBackgroundAction, "Background Subtraction (Manual)",
                                 readInPythonScript("Subtract_TiltSer_Background"), true);
  new AddPythonTransformReaction(sobelFilter2DAction, "Sobel Filter 2D",
                                 readInPythonScript("SobelFilter2D"), true);
  new AddRotateAlignReaction(rotateAlignAction);
  new AddPythonTransformReaction(autoAlignAction,
                                 "Auto Align (XCORR)", readInPythonScript("Align_Images"), true);
  new AddPythonTransformReaction(reconDFMAction,
                                 "Reconstruct (Direct Fourier)",
                                 readInPythonScript("Recon_DFT"), true);
  new AddPythonTransformReaction(reconWBPAction,
                                 "Reconstruct (Back Projection)",
                                 readInPythonScript("Recon_WBP"), true);
  new AddPythonTransformReaction(reconARTAction,
                                 "Reconstruct (ART)",
                                 readInPythonScript("Recon_ART"), true);
  
  //new ReconstructionReaction(reconWBP_CAction);
  //#################################################################
  new ModuleMenu(ui.modulesToolbar, ui.menuModules, this);
  new RecentFilesMenu(*ui.menuRecentlyOpened, ui.menuRecentlyOpened);
  new pqSaveStateReaction(ui.actionSaveDebuggingState);

  new SaveDataReaction(ui.actionSaveData);
  new pqSaveScreenshotReaction(ui.actionSaveScreenshot);
  new pqSaveAnimationReaction(ui.actionSaveMovie);

  new SaveLoadStateReaction(ui.actionSaveState);
  new SaveLoadStateReaction(ui.actionLoadState, /*load*/ true);

  new ResetReaction(ui.actionReset);

  new ViewMenuManager(this, ui.menuView);

  QMenu *sampleDataMenu = new QMenu("Sample Data", this);
  ui.menubar->insertMenu(ui.menuHelp->menuAction(), sampleDataMenu);
#ifdef TOMVIZ_DATA
  QAction *reconAction = sampleDataMenu->addAction("Reconstruction");
  QAction *tiltAction = sampleDataMenu->addAction("Tilt Series");
  connect(reconAction, SIGNAL(triggered()), SLOT(openRecon()));
  connect(tiltAction, SIGNAL(triggered()), SLOT(openTilt()));
  sampleDataMenu->addSeparator();
#endif
  QAction* constantDataAction = sampleDataMenu->addAction("Constant Dataset");
  new PythonGeneratedDatasetReaction(constantDataAction, "Constant Dataset",
      readInPythonScript("ConstantDataset"));
  sampleDataMenu->addSeparator();
  QAction* randomParticlesAction = sampleDataMenu->addAction("Random Particles");
  new PythonGeneratedDatasetReaction(randomParticlesAction, "Random Particles",
      readInPythonScript("RandomParticles"));

  

  //now init the optional dax plugins
#ifdef DAX_DEVICE_ADAPTER
  PV_PLUGIN_IMPORT(tomvizThreshold);
  PV_PLUGIN_IMPORT(tomvizStreaming);
#endif

  ResetReaction::reset();
}

//-----------------------------------------------------------------------------
MainWindow::~MainWindow()
{
  ModuleManager::instance().reset();
  QString autosaveFile = getAutosaveFile();
  if (QFile::exists(autosaveFile) && !QFile::remove(autosaveFile))
  {
    std::cerr << "Failed to remove autosave file." << std::endl;
  }
  delete this->Internals;
}

//-----------------------------------------------------------------------------
void MainWindow::showAbout()
{
  if (!this->Internals->AboutDialog)
  {
    this->Internals->AboutDialog = new QDialog(this);
    this->Internals->AboutUi.setupUi(this->Internals->AboutDialog);
    QString version(TOMVIZ_VERSION);
    if (QString(TOMVIZ_VERSION_EXTRA).size() > 0)
      version.append("-").append(TOMVIZ_VERSION_EXTRA);
    QString versionString =
        this->Internals->AboutUi.version->text().replace("#VERSION", version);
    this->Internals->AboutUi.version->setText(versionString);
  }
  this->Internals->AboutDialog->show();
}

void MainWindow::openTilt()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/TiltSeries_NanoParticle_doi_10.1021-nl103400a.tif";
  QFileInfo info(path);
  if (info.exists())
  {
    LoadDataReaction::loadData(info.canonicalFilePath());
  }
}

void MainWindow::openRecon()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/Recon_NanoParticle_doi_10.1021-nl103400a.tif";
  QFileInfo info(path);
  if (info.exists())
  {
    LoadDataReaction::loadData(info.canonicalFilePath());
  }
}

void MainWindow::dataSourceChanged(DataSource*)
{
  QVBoxLayout *propsLayout =
      qobject_cast<QVBoxLayout *>(this->Internals->Ui.propertiesPanel->layout());
  if (!propsLayout)
  {
    propsLayout = new QVBoxLayout;
    this->Internals->Ui.propertiesPanel->setLayout(propsLayout);
  }
  propsLayout->removeWidget(this->ModulePropertiesWidget);
  this->ModulePropertiesWidget->hide();
  propsLayout->addWidget(this->DataPropertiesWidget);
  this->DataPropertiesWidget->show();
}

void MainWindow::moduleChanged(Module*)
{
  QVBoxLayout *propsLayout =
      qobject_cast<QVBoxLayout *>(this->Internals->Ui.propertiesPanel->layout());
  if (!propsLayout)
  {
    propsLayout = new QVBoxLayout;
    this->Internals->Ui.propertiesPanel->setLayout(propsLayout);
  }
  propsLayout->removeWidget(this->DataPropertiesWidget);
  this->DataPropertiesWidget->hide();
  propsLayout->addWidget(this->ModulePropertiesWidget);
  this->ModulePropertiesWidget->show();
}

void MainWindow::showEvent(QShowEvent *e)
{
  Superclass::showEvent(e);
  QTimer::singleShot(1,this,SLOT(checkForAutosaveFile()));
}

void MainWindow::checkForAutosaveFile()
{
  QFile file(getAutosaveFile());
  if (!file.exists())
  {
    return;
  }
  QMessageBox::StandardButton response = QMessageBox::question(
      this, "Load autosave?", "There is a tomviz autosave file present.  Load it?",
      QMessageBox::Yes|QMessageBox::No);
  if (response == QMessageBox::Yes)
  {
    SaveLoadStateReaction::loadState(getAutosaveFile());
  }
}

void MainWindow::autosave()
{
  SaveLoadStateReaction::saveState(getAutosaveFile());
}

}
