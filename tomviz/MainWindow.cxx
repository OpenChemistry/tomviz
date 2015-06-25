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

#include "tomvizConfig.h"
#include "ActiveObjects.h"
#include "AddAlignReaction.h"
#include "AddExpressionReaction.h"
#include "AddPythonTransformReaction.h"
#include "AddResampleReaction.h"
#include "Behaviors.h"
#include "CropReaction.h"
#include "CloneDataReaction.h"
#include "DeleteDataReaction.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "ModuleMenu.h"
#include "RecentFilesMenu.h"
#include "ResetReaction.h"
#include "SaveDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "SetScaleReaction.h"
#include "ViewMenuManager.h"

#include "MisalignImgs_Uniform.h"
#include "Align_Images.h"
#include "Recon_DFT.h"
#include "Crop_Data.h"
#include "FFT_AbsLog.h"
#include "Shift_Stack_Uniformly.h"
#include "Square_Root_Data.h"
#include "Subtract_TiltSer_Background.h"
#include "MisalignImgs_Gaussian.h"

#include <QFileInfo>

//we are building with dax, so we have plugins to import
#ifdef DAX_DEVICE_ADAPTER
  // Adds required forward declarations.
  PV_PLUGIN_IMPORT_INIT(tomvizThreshold);
  PV_PLUGIN_IMPORT_INIT(tomvizStreaming);
#endif

namespace tomviz
{

class MainWindow::MWInternals
{
public:
  MWInternals() : AboutDialog(NULL) { ; }

  Ui::MainWindow Ui;
  Ui::AboutDialog AboutUi;
  QDialog *AboutDialog;
};

//-----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* _parent, Qt::WindowFlags _flags)
  : Superclass(_parent, _flags),
  Internals(new MainWindow::MWInternals())
{
  Ui::MainWindow& ui = this->Internals->Ui;
  ui.setupUi(this);

  QString version(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0)
    version.append("-").append(TOMVIZ_VERSION_EXTRA);
  setWindowTitle("tomviz " + version);

  QIcon icon(":/icons/tomviz.png");
  setWindowIcon(icon);

  // Link the histogram in the central widget to the active data source.
  ui.centralWidget->connect(&ActiveObjects::instance(),
                            SIGNAL(dataSourceChanged(DataSource*)),
                            SLOT(setDataSource(DataSource*)));

  // connect quit.
  pqApplicationCore::instance()->connect(ui.actionExit, SIGNAL(triggered()),
                                         SLOT(quit()));

  // Connect the about dialog up too.
  connect(ui.actionAbout, SIGNAL(triggered()), SLOT(showAbout()));

  new pqPythonShellReaction(ui.actionPythonConsole);
  new pqMacroReaction(ui.actionMacros);

  // Instantiate tomviz application behavior.
  new Behaviors(this);

  new LoadDataReaction(ui.actionOpen);
  new DeleteDataReaction(ui.actionDeleteData);

  new AddAlignReaction(ui.actionAlign);
  new CloneDataReaction(ui.actionClone);

  /*
   * Data Transforms
   *
   * Crop - Crop_Data.py
   * Background subtraction - Subtract_TiltSer_Background.py
   * ---
   * Manual Align
   * Auto Align (XCORR) - Align_Images.py
   * Shift Uniformly - Shift_Stack_Uniformly.py
   * Misalign (Uniform) - MisalignImgs_Uniform.py
   * Misalign (Gaussian) - MisalignImgs_Gaussian.py
   * ---
   * Reconstruct (Direct Fourier) - Recon_DFT.py
   * ---
   * Square Root Data - Square_Root_Data.py
   * FFT (ABS LOG) - FFT_AbsLog.py
   * ---
   * Clone
   * Delete
   *
   */

  QAction *setScaleAction = new QAction("Set Data Size", this);
  ui.menuTools->addAction(setScaleAction);
  new SetScaleReaction(setScaleAction);

  QAction *customPythonAction = new QAction("Custom Transform", this);
  QAction *cropDataAction = new QAction("Crop", this);
  //QAction *backgroundSubtractAction = new QAction("Background Subtraction", this);
  QAction *autoAlignAction = new QAction("Auto Align (xcorr)", this);
  QAction *shiftUniformAction = new QAction("Shift Uniformly", this);
  //QAction *misalignUniformAction = new QAction("Misalign (Uniform)", this);
  //QAction *misalignGaussianAction = new QAction("Misalign (Gaussian)", this);
  QAction *squareRootAction = new QAction("Square Root Data", this);
  QAction *fftAbsLogAction = new QAction("FFT (abs log)", this);
  //QAction *resampleDataAction = new QAction("Clone && Downsample", this);

  ui.menuData->insertAction(ui.actionAlign, customPythonAction);
  ui.menuData->insertAction(ui.actionAlign, cropDataAction);
  ui.menuData->insertSeparator(ui.actionAlign);
  //ui.menuData->insertAction(ui.actionAlign, backgroundSubtractAction);
  ui.menuData->insertSeparator(ui.actionAlign);
  ui.menuData->insertAction(ui.actionReconstruct, autoAlignAction);
  ui.menuData->insertAction(ui.actionReconstruct, shiftUniformAction);
  //ui.menuData->insertAction(ui.actionReconstruct, misalignUniformAction);
  //ui.menuData->insertAction(ui.actionReconstruct, misalignGaussianAction);
  ui.menuData->insertSeparator(ui.actionReconstruct);
  ui.menuData->insertSeparator(ui.actionClone);
  ui.menuData->insertAction(ui.actionClone, squareRootAction);
  ui.menuData->insertAction(ui.actionClone, fftAbsLogAction);
  ui.menuData->insertSeparator(ui.actionClone);
  //ui.menuData->insertAction(ui.actionClone, resampleDataAction);

  // Add our Python script reactions, these compose Python into menu entries.
  new AddExpressionReaction(customPythonAction);
  new CropReaction(cropDataAction, this);
  //new AddResampleReaction(resampleDataAction);
  //new AddPythonTransformReaction(backgroundSubtractAction,
  //                               "Background Subtraction",
  //                               Subtract_TiltSer_Background);
  ui.actionAlign->setText("Manual Align");
  new AddPythonTransformReaction(autoAlignAction,
                                 "Auto Align (XCORR)", Align_Images);
  new AddPythonTransformReaction(shiftUniformAction,
                                 "Shift Uniformly", Shift_Stack_Uniformly);
  //new AddPythonTransformReaction(misalignUniformAction,
  //                               "Misalign (Uniform)", MisalignImgs_Uniform);
  //new AddPythonTransformReaction(misalignGaussianAction,
  //                               "Misalign (Gaussian)", MisalignImgs_Uniform);
  ui.actionReconstruct->setText("Reconstruct (Direct Fourier)");
  new AddPythonTransformReaction(ui.actionReconstruct,
                                 "Reconstruct (Direct Fourier)",
                                 Recon_DFT);
  new AddPythonTransformReaction(squareRootAction,
                                 "Square Root Data", Square_Root_Data);
  new AddPythonTransformReaction(fftAbsLogAction,
                                 "FFT (ABS LOG)", FFT_AbsLog);

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

#ifdef TOMVIZ_DATA
  QMenu *sampleDataMenu = new QMenu("Sample Data", this);
  ui.menubar->insertMenu(ui.menuHelp->menuAction(), sampleDataMenu);
  QAction *reconAction = sampleDataMenu->addAction("Reconstruction");
  QAction *tiltAction = sampleDataMenu->addAction("Tilt Series");
  connect(reconAction, SIGNAL(triggered()), SLOT(openRecon()));
  connect(tiltAction, SIGNAL(triggered()), SLOT(openTilt()));
#endif

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

}
