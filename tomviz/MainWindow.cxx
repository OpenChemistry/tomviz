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
#include "ui_AboutDialog.h"
#include "ui_MainWindow.h"

#include <pqMacroReaction.h>
#include <pqObjectBuilder.h>
#include <pqPythonShellReaction.h>
#include <pqSaveAnimationReaction.h>
#include <pqSaveStateReaction.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkPVPlugin.h>
#include <vtkPVRenderView.h>
#include <vtkSMSettings.h>
#include <vtkSMViewProxy.h>

#include "ActiveObjects.h"
#include "AddAlignReaction.h"
#include "AddPythonTransformReaction.h"
#include "AddRotateAlignReaction.h"
#include "AddRotateAlignReaction.h"
#include "Behaviors.h"
#include "DataPropertiesPanel.h"
#include "DataTransformMenu.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "ModuleMenu.h"
#include "ModulePropertiesPanel.h"
#include "ProgressDialogManager.h"
#include "PythonGeneratedDatasetReaction.h"
#include "RecentFilesMenu.h"
#include "ReconstructionReaction.h"
#include "ResetReaction.h"
#include "SaveDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "SaveScreenshotReaction.h"
#include "SetScaleReaction.h"
#include "SetTiltAnglesReaction.h"
#include "ToggleDataTypeReaction.h"
#include "Utilities.h"
#include "ViewMenuManager.h"
#include "WelcomeDialog.h"
#include "tomvizConfig.h"

#include "PipelineModel.h"

#include <QAction>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>
#include <QUrl>

#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif

// we are building with dax, so we have plugins to import
#ifdef DAX_DEVICE_ADAPTER
// Adds required forward declarations.
PV_PLUGIN_IMPORT_INIT(tomvizThreshold);
PV_PLUGIN_IMPORT_INIT(tomvizStreaming);
#endif

namespace {
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
  if (!dataDir.exists()) {
    dataDir.mkpath(dataPath);
  }
  return dataDir.absoluteFilePath(".tomviz_autosave.tvsm");
}
}

namespace tomviz {

class MainWindow::MWInternals
{
public:
  Ui::MainWindow Ui;
  Ui::AboutDialog AboutUi;
  QDialog* AboutDialog = nullptr;
  QTimer* Timer = nullptr;
  bool isFirstShow = true;
};

MainWindow::MainWindow(QWidget* _parent, Qt::WindowFlags _flags)
  : Superclass(_parent, _flags), Internals(new MainWindow::MWInternals())
{
  Ui::MainWindow& ui = this->Internals->Ui;
  ui.setupUi(this);
  this->Internals->Timer = new QTimer(this);
  this->connect(this->Internals->Timer, SIGNAL(timeout()), SLOT(autosave()));
  this->Internals->Timer->start(5 /*minutes*/ * 60 /*seconds per minute*/ *
                                1000 /*msec per second*/);

  QString version(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0)
    version.append("-").append(TOMVIZ_VERSION_EXTRA);
  setWindowTitle("tomviz " + version);

  QIcon icon(":/icons/tomviz.png");
  setWindowIcon(icon);

  // Tweak the initial sizes of the dock widgets.
  QList<QDockWidget*> docks;
  docks << this->Internals->Ui.dockWidget << this->Internals->Ui.dockWidget_5;
  QList<int> dockSizes;
  dockSizes << 250 << 250;
  this->resizeDocks(docks, dockSizes, Qt::Horizontal);
  docks.clear();
  dockSizes.clear();
  docks << this->Internals->Ui.dockWidget_3;
  dockSizes << 200;
  this->resizeDocks(docks, dockSizes, Qt::Vertical);

  // Link the histogram in the central widget to the active data source.
  ui.centralWidget->connect(&ActiveObjects::instance(),
                            SIGNAL(dataSourceActivated(DataSource*)),
                            SLOT(setActiveDataSource(DataSource*)));
  ui.centralWidget->connect(&ActiveObjects::instance(),
                            SIGNAL(moduleActivated(Module*)),
                            SLOT(setActiveModule(Module*)));
  ui.centralWidget->connect(ui.dataPropertiesPanel, SIGNAL(colorMapUpdated()),
                            SLOT(onColorMapUpdated()));

  // When a new renderview is created ensure that the orientation axes labels
  // are set to off white.
  connect(pqApplicationCore::instance()->getObjectBuilder(),
          &pqObjectBuilder::viewCreated, [=](pqView* view) {
            auto renderView = vtkPVRenderView::SafeDownCast(
              view->getViewProxy()->GetClientSideView());
            if (renderView) {
              renderView->SetOrientationAxesLabelColor(offWhite[0], offWhite[1],
                                                       offWhite[2]);
            }
          });

  ui.treeWidget->setModel(new PipelineModel(this));
  ui.treeWidget->header()->setStretchLastSection(false);
  ui.treeWidget->header()->setVisible(false);
  ui.treeWidget->header()->setSectionResizeMode(0, QHeaderView::Fixed);
  ui.treeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  ui.treeWidget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
  ui.treeWidget->header()->resizeSection(0, 70);
  ui.treeWidget->header()->resizeSection(2, 30);

  // Ensure that items are expanded by default, can be collapsed at will.
  connect(ui.treeWidget->model(), SIGNAL(rowsInserted(QModelIndex, int, int)),
          ui.treeWidget, SLOT(expandAll()));
  connect(ui.treeWidget->model(), SIGNAL(modelReset()), ui.treeWidget,
          SLOT(expandAll()));

  // connect quit.
  pqApplicationCore::instance()->connect(ui.actionExit, SIGNAL(triggered()),
                                         SLOT(quit()));

  // Connect up the module/data changed to the appropriate slots.
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceActivated(DataSource*)),
          SLOT(dataSourceChanged(DataSource*)));
  connect(&ActiveObjects::instance(), SIGNAL(moduleActivated(Module*)),
          SLOT(moduleChanged(Module*)));
  connect(&ActiveObjects::instance(), SIGNAL(operatorActivated(Operator*)),
          SLOT(operatorChanged(Operator*)));

  // Connect the about dialog up too.
  connect(ui.actionAbout, SIGNAL(triggered()), SLOT(showAbout()));

  new pqPythonShellReaction(ui.actionPythonConsole);
  new pqMacroReaction(ui.actionMacros);

  // Instantiate tomviz application behavior.
  new Behaviors(this);

  new LoadDataReaction(ui.actionOpen);

  QAction* setScaleAction = new QAction("Set Data Size", this);
  ui.menuTools->addAction(setScaleAction);
  new SetScaleReaction(setScaleAction);

  // Build Data Transforms menu
  new DataTransformMenu(this, ui.menuData);

  // Build Tomography menu
  // ################################################################
  QAction* toggleDataTypeAction =
    ui.menuTomography->addAction("Toggle Data Type");
  ui.menuTomography->addSeparator();

  QAction* setTiltAnglesAction =
    ui.menuTomography->addAction("Set Tilt Angles");
  ui.menuTomography->addSeparator();

  QAction* dataProcessingLabel =
    ui.menuTomography->addAction("Pre-reconstruction Processing:");
  dataProcessingLabel->setEnabled(false);
  QAction* downsampleByTwoAction =
    ui.menuTomography->addAction("Bin Tilt Images x2");
  QAction* removeBadPixelsAction =
    ui.menuTomography->addAction("Remove Bad Pixels");
  QAction* gaussianFilterAction =
    ui.menuTomography->addAction("Gaussian Filter");
  QAction* autoSubtractBackgroundAction =
    ui.menuTomography->addAction("Background Subtraction (Auto)");
  QAction* subtractBackgroundAction =
    ui.menuTomography->addAction("Background Subtraction (Manual)");
  QAction* normalizationAction =
    ui.menuTomography->addAction("Normalize Average Image Intensity");
  QAction* gradientMagnitude2DSobelAction =
    ui.menuTomography->addAction("2D Gradient Magnitude");
  QAction* autoAlignAction =
    ui.menuTomography->addAction("Image Alignment (Auto)");
  QAction* alignAction =
    ui.menuTomography->addAction("Image Alignment (Manual)");
  QAction* autoRotateAlignAction =
    ui.menuTomography->addAction("Tilt Axis Alignment (Auto)");
  QAction* rotateAlignAction =
    ui.menuTomography->addAction("Tilt Axis Alignment (Manual)");
  ui.menuTomography->addSeparator();

  QAction* reconLabel = ui.menuTomography->addAction("Reconstruction:");
  reconLabel->setEnabled(false);
  QAction* reconDFMAction =
    ui.menuTomography->addAction("Direct Fourier Method");
  QAction* reconWBPAction =
    ui.menuTomography->addAction("Weighted Back Projection");
  QAction* reconWBP_CAction =
    ui.menuTomography->addAction("Simple Back Projection (C++)");
  QAction* reconARTAction =
    ui.menuTomography->addAction("Algebraic Reconstruction Technique (ART)");
  QAction* reconSIRTAction = ui.menuTomography->addAction(
    "Simultaneous Iterative Reconstruction Technique (SIRT)");
  QAction* reconDFMConstraintAction =
    ui.menuTomography->addAction("Constraint-based Direct Fourier Method");
  QAction* reconTVMinimizationAction =
    ui.menuTomography->addAction("TV Minimization Method");
  ui.menuTomography->addSeparator();
  QAction* generateTiltSeriesAction =
    ui.menuTomography->addAction("Generate Tilt Series");

  // Set up reactions for Tomography Menu
  //#################################################################
  new ToggleDataTypeReaction(toggleDataTypeAction, this);
  new SetTiltAnglesReaction(setTiltAnglesAction, this);

  new AddPythonTransformReaction(
    generateTiltSeriesAction, "Generate Tilt Series",
    readInPythonScript("GenerateTiltSeries"), false, true,
    readInJSONDescription("GenerateTiltSeries"));

  new AddAlignReaction(alignAction);
  new AddPythonTransformReaction(downsampleByTwoAction, "Bin Tilt Image x2",
                                 readInPythonScript("BinTiltSeriesByTwo"),
                                 true);
  new AddPythonTransformReaction(
    removeBadPixelsAction, "Remove Bad Pixels",
    readInPythonScript("RemoveBadPixelsTiltSeries"), true, false,
    readInJSONDescription("RemoveBadPixelsTiltSeries"));
  new AddPythonTransformReaction(
    gaussianFilterAction, "Gaussian Filter Tilt Series",
    readInPythonScript("GaussianFilterTiltSeries"), true, false,
    readInJSONDescription("GaussianFilterTiltSeries"));
  new AddPythonTransformReaction(
    autoSubtractBackgroundAction, "Background Subtraction (Auto)",
    readInPythonScript("Subtract_TiltSer_Background_Auto"), true);
  new AddPythonTransformReaction(
    subtractBackgroundAction, "Background Subtraction (Manual)",
    readInPythonScript("Subtract_TiltSer_Background"), true);
  new AddPythonTransformReaction(normalizationAction, "Normalize Tilt Series",
                                 readInPythonScript("NormalizeTiltSeries"),
                                 true);
  new AddPythonTransformReaction(
    gradientMagnitude2DSobelAction, "Gradient Magnitude 2D",
    readInPythonScript("GradientMagnitude2D_Sobel"), true);
  new AddRotateAlignReaction(rotateAlignAction);
  new AddPythonTransformReaction(autoRotateAlignAction, "Auto Tilt Axis Align",
                                 readInPythonScript("AutoTiltAxisAlignment"),
                                 true);
  new AddPythonTransformReaction(
    autoAlignAction, "Auto Tilt Image Align (XCORR)",
    readInPythonScript("AutoTiltImageAlignment"), true);
  new AddPythonTransformReaction(reconDFMAction, "Reconstruct (Direct Fourier)",
                                 readInPythonScript("Recon_DFT"), true);
  new AddPythonTransformReaction(reconWBPAction,
                                 "Reconstruct (Back Projection)",
                                 readInPythonScript("Recon_WBP"), true);
  new AddPythonTransformReaction(reconARTAction, "Reconstruct (ART)",
                                 readInPythonScript("Recon_ART"), true, false,
                                 readInJSONDescription("Recon_ART"));
  new AddPythonTransformReaction(reconSIRTAction, "Reconstruct (SIRT)",
                                 readInPythonScript("Recon_SIRT"), true);
  new AddPythonTransformReaction(
    reconDFMConstraintAction, "Reconstruct (Constraint-based Direct Fourier)",
    readInPythonScript("Recon_DFT_constraint"), true);
  new AddPythonTransformReaction(
    reconTVMinimizationAction, "Reconstruct (TV Minimization)",
    readInPythonScript("Recon_TV_minimization"), true, false,
    readInJSONDescription("Recon_TV_minimization"));

  new ReconstructionReaction(reconWBP_CAction);
  //#################################################################
  new ModuleMenu(ui.modulesToolbar, ui.menuModules, this);
  new RecentFilesMenu(*ui.menuRecentlyOpened, ui.menuRecentlyOpened);
  new pqSaveStateReaction(ui.actionSaveDebuggingState);

  new SaveDataReaction(ui.actionSaveData);
  new SaveScreenshotReaction(ui.actionSaveScreenshot, this);
  new pqSaveAnimationReaction(ui.actionSaveMovie);

  new SaveLoadStateReaction(ui.actionSaveState);
  new SaveLoadStateReaction(ui.actionLoadState, /*load*/ true);

  new ResetReaction(ui.actionReset);

  new ViewMenuManager(this, ui.menuView);

  QMenu* sampleDataMenu = new QMenu("Sample Data", this);
  ui.menubar->insertMenu(ui.menuHelp->menuAction(), sampleDataMenu);
#ifdef TOMVIZ_DATA
  QAction* reconAction =
    sampleDataMenu->addAction("Star Nanoparticle (Reconstruction)");
  QAction* tiltAction =
    sampleDataMenu->addAction("Star Nanoparticle (Tilt Series)");
  connect(reconAction, SIGNAL(triggered()), SLOT(openRecon()));
  connect(tiltAction, SIGNAL(triggered()), SLOT(openTilt()));
  sampleDataMenu->addSeparator();
#endif
  QAction* constantDataAction =
    sampleDataMenu->addAction("Generate Constant Dataset");
  new PythonGeneratedDatasetReaction(constantDataAction, "Constant Dataset",
                                     readInPythonScript("ConstantDataset"));
  QAction* randomParticlesAction =
    sampleDataMenu->addAction("Generate Random Particles");
  new PythonGeneratedDatasetReaction(randomParticlesAction, "Random Particles",
                                     readInPythonScript("RandomParticles"));
  QAction* probeShapeAction =
    sampleDataMenu->addAction("Generate Electron Beam Shape");
  new PythonGeneratedDatasetReaction(probeShapeAction, "Electron Beam Shape",
                                     readInPythonScript("STEM_probe"));
  sampleDataMenu->addSeparator();
  QAction* sampleDataLinkAction =
    sampleDataMenu->addAction("Download More Datasets");
  connect(sampleDataLinkAction, SIGNAL(triggered()), SLOT(openDataLink()));

  QAction* moveObjects =
    ui.toolBar->addAction(QIcon(":/icons/move_objects"), "MoveObjects");
  moveObjects->setToolTip(
    "Enable to allow moving of the selected dataset in the scene");
  moveObjects->setCheckable(true);

  QObject::connect(moveObjects, SIGNAL(triggered(bool)),
                   &ActiveObjects::instance(), SLOT(setMoveObjectsMode(bool)));

// now init the optional dax plugins
#ifdef DAX_DEVICE_ADAPTER
  PV_PLUGIN_IMPORT(tomvizThreshold);
  PV_PLUGIN_IMPORT(tomvizStreaming);
#endif

  ResetReaction::reset();
  // Initialize worker manager
  new ProgressDialogManager(this);
}

MainWindow::~MainWindow()
{
  ModuleManager::instance().reset();
  QString autosaveFile = getAutosaveFile();
  if (QFile::exists(autosaveFile) && !QFile::remove(autosaveFile)) {
    std::cerr << "Failed to remove autosave file." << std::endl;
  }
  delete this->Internals;
}

void MainWindow::showAbout()
{
  if (!this->Internals->AboutDialog) {
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
  if (info.exists()) {
    LoadDataReaction::loadData(info.canonicalFilePath());
  }
}

void MainWindow::openRecon()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/Recon_NanoParticle_doi_10.1021-nl103400a.tif";
  QFileInfo info(path);
  if (info.exists()) {
    LoadDataReaction::loadData(info.canonicalFilePath());
  } else {
    QMessageBox::warning(
      this, "Sample Data not found",
      QString("The data file \"%1\" was not found.").arg(path));
  }
}

void MainWindow::openDataLink()
{
  QString link = "http://www.nature.com/articles/sdata201641";
  QDesktopServices::openUrl(QUrl(link));
}

void MainWindow::dataSourceChanged(DataSource*)
{
  this->Internals->Ui.propertiesPanelStackedWidget->setCurrentWidget(
    this->Internals->Ui.dataPropertiesScrollArea);
}

void MainWindow::moduleChanged(Module*)
{
  this->Internals->Ui.propertiesPanelStackedWidget->setCurrentWidget(
    this->Internals->Ui.modulePropertiesScrollArea);
}

void MainWindow::operatorChanged(Operator*)
{
  this->Internals->Ui.propertiesPanelStackedWidget->setCurrentWidget(
    this->Internals->Ui.operatorPropertiesScrollArea);
}

void MainWindow::showEvent(QShowEvent* e)
{
  Superclass::showEvent(e);
  if (this->Internals->isFirstShow) {
    this->Internals->isFirstShow = false;
    QTimer::singleShot(1, this, SLOT(onFirstWindowShow()));
  }

  // Add a delayed render after the main window is shown.
  // This is an attempt to work around a problem with Mac render
  // windows not repainting appropriately.
  //
  // NOTE: remove this once painting events in QVTKWidget or
  // it successor work better with Qt 5.
  QTimer::singleShot(250, &ActiveObjects::instance(), SLOT(renderAllViews()));
}

void MainWindow::onFirstWindowShow()
{
  QFile file(getAutosaveFile());
  if (!file.exists()) {
    QSettings* settings = pqApplicationCore::instance()->settings();
    int showWelcome =
      settings->value("GeneralSettings.ShowWelcomeDialog", 1).toInt();
    if (showWelcome) {
      QString path =
        QApplication::applicationDirPath() + "/../share/tomviz/Data";
      path += "/Recon_NanoParticle_doi_10.1021-nl103400a.tif";
      QFileInfo info(path);
      if (info.exists()) {
        WelcomeDialog welcomeDialog(this);
        welcomeDialog.setModal(true);
        welcomeDialog.exec();
      }
    }
    return;
  }
  QMessageBox::StandardButton response =
    QMessageBox::question(this, "Load autosave?",
                          "There is a tomviz autosave file present.  Load it?",
                          QMessageBox::Yes | QMessageBox::No);
  if (response == QMessageBox::Yes) {
    SaveLoadStateReaction::loadState(getAutosaveFile());
  }
}

void MainWindow::autosave()
{
  SaveLoadStateReaction::saveState(getAutosaveFile());
}
}
