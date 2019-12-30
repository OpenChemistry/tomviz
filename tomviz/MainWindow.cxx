/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqOutputWidget.h>
#include <pqSaveAnimationReaction.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkPVPlugin.h>
#include <vtkPVRenderView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSettings.h>
#include <vtkSMViewProxy.h>

#include "AboutDialog.h"
#include "AcquisitionWidget.h"
#include "ActiveObjects.h"
#include "AddAlignReaction.h"
#include "AddPythonTransformReaction.h"
#include "AxesReaction.h"
#include "Behaviors.h"
#include "CameraReaction.h"
#include "Connection.h"
#include "DataPropertiesPanel.h"
#include "DataTransformMenu.h"
#include "FileFormatManager.h"
#include "LoadDataReaction.h"
#include "LoadPaletteReaction.h"
#include "LoadStackReaction.h"
#include "ModuleManager.h"
#include "ModuleMenu.h"
#include "ModulePropertiesPanel.h"
#include "OperatorFactory.h"
#include "PassiveAcquisitionWidget.h"
#include "Pipeline.h"
#include "PipelineManager.h"
#include "PipelineSettingsDialog.h"
#include "ProgressDialogManager.h"
#include "PythonGeneratedDatasetReaction.h"
#include "PythonUtilities.h"
#include "RecentFilesMenu.h"
#include "ReconstructionReaction.h"
#include "RegexGroupSubstitution.h"
#include "ResetReaction.h"
#include "SaveDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "SaveScreenshotReaction.h"
#include "SaveWebReaction.h"
#include "SetDataTypeReaction.h"
#include "SetTiltAnglesOperator.h"
#include "SetTiltAnglesReaction.h"
#include "Utilities.h"
#include "ViewMenuManager.h"
#include "WelcomeDialog.h"
#include "tomvizConfig.h"

#include "PipelineModel.h"

#include <QAction>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QMessageBox>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QtConcurrent>

namespace {
QString getAutosaveFile()
{
  // workaround to get user config location
  QString dataPath;
  dataPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
  QDir dataDir(dataPath);
  if (!dataDir.exists()) {
    dataDir.mkpath(dataPath);
  }
  return dataDir.absoluteFilePath(".tomviz_autosave.tvsm");
}
} // namespace
class Connection;

namespace tomviz {

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
  : QMainWindow(parent, flags), m_ui(new Ui::MainWindow)
{
  // Override the default setting for showing full messages. This needs to be
  // done prior to calling m_ui->setupUi(this) which sets the default to false.
  pqSettings* qtSettings = pqApplicationCore::instance()->settings();
  if (!qtSettings->contains("pqOutputWidget.ShowFullMessages")) {
    qtSettings->setValue("pqOutputWidget.ShowFullMessages", true);
  }

  connect(&ModuleManager::instance(), &ModuleManager::enablePythonConsole, this,
          &MainWindow::setEnabledPythonConsole);

  // Update back light azimuth default on view.
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          &pqServerManagerModel::viewAdded, [](pqView* view) {
            vtkSMPropertyHelper helper(view->getProxy(), "BackLightAzimuth");
            // See https://github.com/OpenChemistry/tomviz/issues/1525
            helper.Set(60);
          });

  // checkOpenGL();
  m_ui->setupUi(this);
  m_timer = new QTimer(this);
  connect(m_timer, SIGNAL(timeout()), SLOT(autosave()));
  m_timer->start(5 /*minutes*/ * 60 /*seconds per minute*/ *
                 1000 /*msec per second*/);

  QString version(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0)
    version.append("-").append(TOMVIZ_VERSION_EXTRA);
  setWindowTitle("tomviz " + version);

  QIcon icon(":/icons/tomviz.png");
  setWindowIcon(icon);

  // tabify output messages widget.
  tabifyDockWidget(m_ui->dockWidgetAnimation, m_ui->dockWidgetMessages);
  tabifyDockWidget(m_ui->dockWidgetAnimation, m_ui->dockWidgetPythonConsole);

  // don't think tomviz should import ParaView modules by default in Python
  // shell.
  pqPythonShell::setPreamble(QStringList());

  connect(m_ui->pythonConsole, &pqPythonShell::executing,
          [this](bool executing) {
            if (!executing) {
              this->syncPythonToApp();
            }
          });

  // Hide these dock widgets when tomviz is first opened. If they are later
  // opened and remain open while tomviz is shut down, their visibility and
  // geometry state will be saved out to the settings file. The dock widgets
  // will then be restored when the Behaviors (in particular the
  // pqPersistentMainWindowStateBehavior) are instantiated further down.
  m_ui->dockWidgetMessages->hide();
  m_ui->dockWidgetPythonConsole->hide();
  m_ui->dockWidgetAnimation->hide();
  m_ui->dockWidgetLightsInspector->hide();

  // Tweak the initial sizes of the dock widgets.
  QList<QDockWidget*> docks;
  docks << m_ui->dockWidget << m_ui->dockWidget_5 << m_ui->dockWidgetMessages;
  QList<int> dockSizes;
  dockSizes << 250 << 250 << 250;
  resizeDocks(docks, dockSizes, Qt::Horizontal);
  docks.clear();
  dockSizes.clear();
  docks << m_ui->dockWidgetAnimation;
  dockSizes << 200;
  resizeDocks(docks, dockSizes, Qt::Vertical);

  // raise dockWidgetMessages on error.
  connect(m_ui->outputWidget, SIGNAL(messageDisplayed(const QString&, int)),
          SLOT(handleMessage(const QString&, int)));

  // Link the histogram in the central widget to the active data source.
  m_ui->centralWidget->connect(
    &ActiveObjects::instance(), &ActiveObjects::transformedDataSourceActivated,
    m_ui->centralWidget, &CentralWidget::setActiveColorMapDataSource);
  m_ui->centralWidget->connect(&ActiveObjects::instance(),
                               SIGNAL(moduleActivated(Module*)),
                               SLOT(setActiveModule(Module*)));
  m_ui->centralWidget->connect(&ActiveObjects::instance(),
                               SIGNAL(colorMapChanged(DataSource*)),
                               SLOT(setActiveColorMapDataSource(DataSource*)));
  m_ui->centralWidget->connect(m_ui->dataPropertiesPanel,
                               SIGNAL(colorMapUpdated()),
                               SLOT(onColorMapUpdated()));
  m_ui->centralWidget->connect(&ActiveObjects::instance(),
                               SIGNAL(operatorActivated(Operator*)),
                               SLOT(setActiveOperator(Operator*)));

  m_ui->treeWidget->setModel(new PipelineModel(this));
  m_ui->treeWidget->initLayout();

  // Ensure that items are expanded by default, can be collapsed at will.
  connect(m_ui->treeWidget->model(),
          SIGNAL(rowsInserted(QModelIndex, int, int)), m_ui->treeWidget,
          SLOT(expandAll()));
  connect(m_ui->treeWidget->model(), SIGNAL(modelReset()), m_ui->treeWidget,
          SLOT(expandAll()));

  // connect quit.
  connect(m_ui->actionExit, SIGNAL(triggered()), SLOT(close()));

  // Connect up the module/data changed to the appropriate slots.
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceActivated(DataSource*)),
          SLOT(dataSourceChanged(DataSource*)));
  connect(&ActiveObjects::instance(),
          SIGNAL(moleculeSourceActivated(MoleculeSource*)),
          SLOT(moleculeSourceChanged(MoleculeSource*)));
  connect(&ActiveObjects::instance(), SIGNAL(moduleActivated(Module*)),
          SLOT(moduleChanged(Module*)));
  connect(&ActiveObjects::instance(), SIGNAL(operatorActivated(Operator*)),
          SLOT(operatorChanged(Operator*)));
  connect(&ActiveObjects::instance(), SIGNAL(resultActivated(OperatorResult*)),
          SLOT(operatorResultChanged(OperatorResult*)));

  // Connect the about dialog up too.
  connect(m_ui->actionAbout, &QAction::triggered, this,
          [this]() { openDialog<AboutDialog>(&m_aboutDialog); });

  // And connect the read the docs page
  connect(m_ui->actionReadTheDocs, &QAction::triggered, this,
          &MainWindow::openReadTheDocs);

  // Instantiate tomviz application behavior.
  new Behaviors(this);

  new LoadDataReaction(m_ui->actionOpen);

  new LoadStackReaction(m_ui->actionStack);

  // Build Data Transforms menu
  new DataTransformMenu(this, m_ui->menuData, m_ui->menuSegmentation);

  // Create the custom transforms menu
  m_customTransformsMenu = new QMenu("Custom Transforms", this);
  m_customTransformsMenu->setEnabled(false);
  m_ui->menubar->insertMenu(m_ui->menuModules->menuAction(),
                            m_customTransformsMenu);

  // Build Tomography menu
  // ################################################################
  QAction* setVolumeDataTypeAction =
    m_ui->menuTomography->addAction("Set Data Type");
  QAction* setTiltDataTypeAction =
    m_ui->menuTomography->addAction("Set Data Type");
  // QAction* setFibDataTypeAction =
  //   m_ui->menuTomography->addAction("Set Data Type");
  m_ui->menuTomography->addSeparator();

  QAction* setTiltAnglesAction =
    m_ui->menuTomography->addAction("Set Tilt Angles");
  m_ui->menuTomography->addSeparator();

  QAction* dataProcessingLabel =
    m_ui->menuTomography->addAction("Pre-processing:");
  dataProcessingLabel->setEnabled(false);
  QAction* downsampleByTwoAction =
    m_ui->menuTomography->addAction("Bin Tilt Images x2");
  QAction* removeBadPixelsAction =
    m_ui->menuTomography->addAction("Remove Bad Pixels");
  QAction* gaussianFilterAction =
    m_ui->menuTomography->addAction("Gaussian Filter");
  QAction* autoSubtractBackgroundAction =
    m_ui->menuTomography->addAction("Background Subtraction (Auto)");
  QAction* subtractBackgroundAction =
    m_ui->menuTomography->addAction("Background Subtraction (Manual)");
  QAction* normalizationAction =
    m_ui->menuTomography->addAction("Normalize Average Image Intensity");
  QAction* gradientMagnitude2DSobelAction =
    m_ui->menuTomography->addAction("2D Gradient Magnitude");

  m_ui->menuTomography->addSeparator();
  QAction* alignmentLabel = m_ui->menuTomography->addAction("Alignment:");
  alignmentLabel->setEnabled(false);
  QAction* autoAlignCCAction = m_ui->menuTomography->addAction(
    "Image Alignment (Auto: Cross Correlation)");
  QAction* autoAlignCOMAction =
    m_ui->menuTomography->addAction("Image Alignment (Auto: Center of Mass)");
  QAction* alignAction =
    m_ui->menuTomography->addAction("Image Alignment (Manual)");
  QAction* autoRotateAlignAction =
    m_ui->menuTomography->addAction("Tilt Axis Rotation Alignment (Auto)");
  QAction* autoRotateAlignShiftAction =
    m_ui->menuTomography->addAction("Tilt Axis Shift Alignment (Auto)");
  QAction* rotateAlignAction =
    m_ui->menuTomography->addAction("Tilt Axis Alignment (Manual)");
  m_ui->menuTomography->addSeparator();

  QAction* reconLabel = m_ui->menuTomography->addAction("Reconstruction:");
  reconLabel->setEnabled(false);
  QAction* reconDFMAction =
    m_ui->menuTomography->addAction("Direct Fourier Method");
  QAction* reconWBPAction =
    m_ui->menuTomography->addAction("Weighted Back Projection");
  QAction* reconWBP_CAction =
    m_ui->menuTomography->addAction("Simple Back Projection (C++)");
  QAction* reconARTAction =
    m_ui->menuTomography->addAction("Algebraic Reconstruction Technique (ART)");
  QAction* reconSIRTAction = m_ui->menuTomography->addAction(
    "Simultaneous Iterative Recon. Technique (SIRT)");
  QAction* reconDFMConstraintAction =
    m_ui->menuTomography->addAction("Constraint-based Direct Fourier Method");
  QAction* reconTVMinimizationAction =
    m_ui->menuTomography->addAction("TV Minimization Method");
  QAction* reconTomoPyGridRecAction =
    m_ui->menuTomography->addAction("TomoPy Gridrec Method");
  m_ui->menuTomography->addSeparator();

  QAction* simulationLabel = m_ui->menuTomography->addAction("Simulation:");
  simulationLabel->setEnabled(false);
  QAction* generateTiltSeriesAction =
    m_ui->menuTomography->addAction("Project Tilt Series from Volume");

  QAction* randomShiftsAction =
    m_ui->menuTomography->addAction("Shift Tilt Series Randomly");
  QAction* addPoissonNoiseAction =
    m_ui->menuTomography->addAction("Add Poisson Noise");

  // Set up reactions for Tomography Menu
  //#################################################################
  new SetDataTypeReaction(setVolumeDataTypeAction, this, DataSource::Volume);
  new SetDataTypeReaction(setTiltDataTypeAction, this, DataSource::TiltSeries);
  // new SetDataTypeReaction(setFibDataTypeAction, this, DataSource::FIB);
  new SetTiltAnglesReaction(setTiltAnglesAction, this);

  new AddPythonTransformReaction(
    generateTiltSeriesAction, "Generate Tilt Series",
    readInPythonScript("GenerateTiltSeries"), false, true, false,
    readInJSONDescription("GenerateTiltSeries"));

  new AddAlignReaction(alignAction);
  new AddPythonTransformReaction(downsampleByTwoAction, "Bin Tilt Image x2",
                                 readInPythonScript("BinTiltSeriesByTwo"),
                                 false, false, false);
  new AddPythonTransformReaction(
    removeBadPixelsAction, "Remove Bad Pixels",
    readInPythonScript("RemoveBadPixelsTiltSeries"), false, false, false);
  new AddPythonTransformReaction(
    gaussianFilterAction, "Gaussian Filter Tilt Series",
    readInPythonScript("GaussianFilterTiltSeries"), false, false, false,
    readInJSONDescription("GaussianFilterTiltSeries"));
  new AddPythonTransformReaction(
    autoSubtractBackgroundAction, "Background Subtraction (Auto)",
    readInPythonScript("Subtract_TiltSer_Background_Auto"), false, false,
    false);
  new AddPythonTransformReaction(
    subtractBackgroundAction, "Background Subtraction (Manual)",
    readInPythonScript("Subtract_TiltSer_Background"), false, false, false);
  new AddPythonTransformReaction(normalizationAction, "Normalize Tilt Series",
                                 readInPythonScript("NormalizeTiltSeries"),
                                 false, false, false);
  new AddPythonTransformReaction(
    gradientMagnitude2DSobelAction, "Gradient Magnitude 2D",
    readInPythonScript("GradientMagnitude2D_Sobel"), false, false, false);
  new AddPythonTransformReaction(
    rotateAlignAction, "Tilt Axis Alignment (manual)",
    readInPythonScript("RotationAlign"), true, false, false,
    readInJSONDescription("RotationAlign"));
  // new AddRotateAlignReaction(rotateAlignAction);
  new AddPythonTransformReaction(
    autoRotateAlignAction, "Auto Tilt Axis Align",
    readInPythonScript("AutoTiltAxisRotationAlignment"), true);
  new AddPythonTransformReaction(
    autoRotateAlignShiftAction, "Auto Tilt Axis Shift Align",
    readInPythonScript("AutoTiltAxisShiftAlignment"), true);

  new AddPythonTransformReaction(
    autoAlignCCAction, "Auto Tilt Image Align (XCORR)",
    readInPythonScript("AutoCrossCorrelationTiltImageAlignment"), false, false,
    false, readInJSONDescription("AutoCrossCorrelationTiltImageAlignment"));
  new AddPythonTransformReaction(
    autoAlignCOMAction, "Auto Tilt Image Align (CoM)",
    readInPythonScript("AutoCenterOfMassTiltImageAlignment"), false, false,
    false, readInJSONDescription("AutoCenterOfMassTiltImageAlignment"));
  new AddPythonTransformReaction(reconDFMAction, "Reconstruct (Direct Fourier)",
                                 readInPythonScript("Recon_DFT"), true, false,
                                 false, readInJSONDescription("Recon_DFT"));
  new AddPythonTransformReaction(reconWBPAction,
                                 "Reconstruct (Back Projection)",
                                 readInPythonScript("Recon_WBP"), true, false,
                                 false, readInJSONDescription("Recon_WBP"));
  new AddPythonTransformReaction(reconARTAction, "Reconstruct (ART)",
                                 readInPythonScript("Recon_ART"), true, false,
                                 false, readInJSONDescription("Recon_ART"));
  new AddPythonTransformReaction(reconSIRTAction, "Reconstruct (SIRT)",
                                 readInPythonScript("Recon_SIRT"), true, false,
                                 false, readInJSONDescription("Recon_SIRT"));
  new AddPythonTransformReaction(
    reconDFMConstraintAction, "Reconstruct (Constraint-based Direct Fourier)",
    readInPythonScript("Recon_DFT_constraint"), true, false, false,
    readInJSONDescription("Recon_DFT_constraint"));
  new AddPythonTransformReaction(
    reconTVMinimizationAction, "Reconstruct (TV Minimization)",
    readInPythonScript("Recon_TV_minimization"), true, false, false,
    readInJSONDescription("Recon_TV_minimization"));
  new AddPythonTransformReaction(
    reconTomoPyGridRecAction, "Reconstruct (TomoPy Gridrec)",
    readInPythonScript("Recon_tomopy_gridrec"), true, false, false,
    readInJSONDescription("Recon_tomopy_gridrec"));

  new ReconstructionReaction(reconWBP_CAction);

  new AddPythonTransformReaction(
    randomShiftsAction, "Shift Tilt Series Randomly",
    readInPythonScript("ShiftTiltSeriesRandomly"), true, false, false,
    readInJSONDescription("ShiftTiltSeriesRandomly"));
  new AddPythonTransformReaction(addPoissonNoiseAction, "Add Poisson Noise",
                                 readInPythonScript("AddPoissonNoise"), true,
                                 false, false,
                                 readInJSONDescription("AddPoissonNoise"));

  //#################################################################
  new ModuleMenu(m_ui->modulesToolbar, m_ui->menuModules, this);
  new RecentFilesMenu(*m_ui->menuRecentlyOpened, m_ui->menuRecentlyOpened);

  new SaveDataReaction(m_ui->actionSaveData);
  new SaveScreenshotReaction(m_ui->actionSaveScreenshot, this);
  new pqSaveAnimationReaction(m_ui->actionSaveMovie);
  new SaveWebReaction(m_ui->actionSaveWeb, this);

  new SaveLoadStateReaction(m_ui->actionSaveState);
  new SaveLoadStateReaction(m_ui->actionLoadState, /*load*/ true);

  auto reaction = new ResetReaction(m_ui->actionReset);
  connect(m_ui->menu_File, &QMenu::aboutToShow, reaction,
          &ResetReaction::updateEnableState);

  new ViewMenuManager(this, m_ui->menuView);

  QMenu* sampleDataMenu = new QMenu("Sample Data", this);
  m_ui->menubar->insertMenu(m_ui->menuHelp->menuAction(), sampleDataMenu);
  QAction* userGuideAction = m_ui->menuHelp->addAction("User Guide");
  connect(userGuideAction, SIGNAL(triggered()), SLOT(openUserGuide()));
  QAction* introAction = m_ui->menuHelp->addAction("Intro to 3D Visualization");
  connect(introAction, SIGNAL(triggered()), SLOT(openVisIntro()));
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

  QAction* moveObjects = m_ui->utilitiesToolbar->addAction(
    QIcon(":/icons/move_objects.png"), "MoveObjects");
  moveObjects->setToolTip(
    "Enable to allow moving of the selected dataset in the scene");
  moveObjects->setCheckable(true);

  connect(moveObjects, SIGNAL(triggered(bool)), &ActiveObjects::instance(),
          SLOT(setMoveObjectsMode(bool)));

  QAction* loadPaletteAction = m_ui->utilitiesToolbar->addAction(
    QIcon(":/icons/pqPalette.png"), "LoadPalette");
  new LoadPaletteReaction(loadPaletteAction);

  QToolButton* tb = qobject_cast<QToolButton*>(
    m_ui->utilitiesToolbar->widgetForAction(loadPaletteAction));
  if (tb) {
    tb->setPopupMode(QToolButton::InstantPopup);
  }

  CameraReaction::addAllActionsToToolBar(m_ui->utilitiesToolbar);
  AxesReaction::addAllActionsToToolBar(m_ui->utilitiesToolbar);

  ResetReaction::reset();
  // Initialize worker manager
  new ProgressDialogManager(this);

  // Add the acquisition client experimentally.
  m_ui->actionAcquisition->setEnabled(false);
  m_ui->actionPassiveAcquisition->setEnabled(false);

  connect(m_ui->actionAcquisition, &QAction::triggered, this,
          [this]() { openDialog<AcquisitionWidget>(&m_acquisitionWidget); });

  connect(m_ui->actionPassiveAcquisition, &QAction::triggered, this, [this]() {
    openDialog<PassiveAcquisitionWidget>(&m_passiveAcquisitionDialog);
  });

  auto pipelineSettingsDialog = new PipelineSettingsDialog(this);
  connect(m_ui->actionPipelineSettings, &QAction::triggered,
          pipelineSettingsDialog, &QWidget::show);

  // Prepopulate the previously seen python readers/writers
  // This operation is fast since it fetches the readers description
  // from the settings, without really invoking python
  FileFormatManager::instance().prepopulatePythonReaders();
  FileFormatManager::instance().prepopulatePythonWriters();

  // Async initialize python
  statusBar()->showMessage("Initializing python...");
  auto pythonWatcher = new QFutureWatcher<std::vector<OperatorDescription>>;
  connect(pythonWatcher, &QFutureWatcherBase::finished, this,
          [this, pythonWatcher]() {
            m_ui->actionAcquisition->setEnabled(true);
            m_ui->actionPassiveAcquisition->setEnabled(true);
            registerCustomOperators(pythonWatcher->result());
            delete pythonWatcher;
            statusBar()->showMessage("Initialization complete", 1500);
          });

  auto pythonFuture = QtConcurrent::run(initPython);
  pythonWatcher->setFuture(pythonFuture);
}

MainWindow::~MainWindow()
{
  ModuleManager::instance().reset();
  QString autosaveFile = getAutosaveFile();
  if (QFile::exists(autosaveFile) && !QFile::remove(autosaveFile)) {
    std::cerr << "Failed to remove autosave file." << std::endl;
  }
}

std::vector<OperatorDescription> MainWindow::initPython()
{
  Python::initialize();
  Connection::registerType();
  RegexGroupSubstitution::registerType();
  auto operators = findCustomOperators();
  FileFormatManager::instance().registerPythonReaders();
  FileFormatManager::instance().registerPythonWriters();

  return operators;
}

template <class T>
void MainWindow::openDialog(QWidget** dialog)
{
  if (*dialog == nullptr) {
    *dialog = new T(this);
  }
  (*dialog)->show();
}

void MainWindow::openFiles(int argc, char** argv)
{
  if (argc < 2) {
    return;
  }

  QString path(argv[argc - 1]);
  QFileInfo info(path);
  if (!info.exists()) {
    return;
  }

  if (info.isFile()) {
    if (info.suffix() == "tvsm") {
      SaveLoadStateReaction::loadState(info.canonicalFilePath());
    } else {
      LoadDataReaction::loadData(info.canonicalFilePath());
    }
  } else if (info.isDir()) {
    LoadStackReaction::loadData(info.canonicalFilePath());
  }
}

void MainWindow::openTilt()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/TiltSeries_NanoParticle_doi_10.1021-nl103400a.emd";
  QFileInfo info(path);
  if (info.exists()) {
    LoadDataReaction::loadData(info.canonicalFilePath());
  } else {
    QMessageBox::warning(
      this, "Sample Data not found",
      QString("The data file \"%1\" was not found.").arg(path));
  }
}

void MainWindow::openRecon()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/Recon_NanoParticle_doi_10.1021-nl103400a.emd";
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
  openUrl(link);
}

void MainWindow::openReadTheDocs()
{
  openHelpUrl();
}

void MainWindow::openUserGuide()
{
  QString path = QApplication::applicationDirPath() +
                 "/../share/tomviz/docs/TomvizBasicUserGuide.pdf";
  QFileInfo info(path);
  if (info.exists()) {
    QUrl userGuideUrl = QUrl::fromLocalFile(path);
    openUrl(userGuideUrl);
  } else {
    QMessageBox::warning(
      this, "User Guide not found",
      QString("The user guide \"%1\" was not found.").arg(path));
  }
}

void MainWindow::openVisIntro()
{
  QString link = "https://www.cambridge.org/core/journals/microscopy-today/"
                 "article/"
                 "tutorial-on-the-visualization-of-volumetric-data-using-"
                 "tomviz/55B58F40A16E96CDEB644202D9FD08BB";
  openUrl(link);
}

void MainWindow::dataSourceChanged(DataSource* dataSource)
{
  m_ui->propertiesPanelStackedWidget->setCurrentWidget(
    m_ui->dataPropertiesScrollArea);
  if (dataSource) {
    bool canAdd = !dataSource->pipeline()->editingOperators();
    m_ui->menuData->setEnabled(canAdd);
    m_ui->menuSegmentation->setEnabled(canAdd);
    m_ui->menuTomography->setEnabled(canAdd);
    m_customTransformsMenu->setEnabled(canAdd);
  }
}

void MainWindow::moleculeSourceChanged(MoleculeSource*)
{
  m_ui->propertiesPanelStackedWidget->setCurrentWidget(
    m_ui->moleculePropertiesScrollArea);
}

void MainWindow::moduleChanged(Module*)
{
  m_ui->propertiesPanelStackedWidget->setCurrentWidget(
    m_ui->modulePropertiesScrollArea);
}

void MainWindow::operatorChanged(Operator*)
{
  m_ui->propertiesPanelStackedWidget->setCurrentWidget(
    m_ui->operatorPropertiesScrollArea);
}

void MainWindow::operatorResultChanged(OperatorResult* res)
{
  if (res) {
    m_ui->propertiesPanelStackedWidget->setCurrentWidget(
      m_ui->operatorResultPropertiesScrollArea);
  }
}

void MainWindow::importCustomTransform()
{
  QStringList filters;
  filters << "Python (*.py)";

  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setNameFilters(filters);
  dialog.setObjectName("ImportCustomTransform-tomviz");
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList filePaths = dialog.selectedFiles();
    QString format = dialog.selectedNameFilter();
    QFileInfo fileInfo(filePaths[0]);
    QString filePath = fileInfo.absolutePath();
    QString fileBaseName = fileInfo.baseName();
    QString pythonSourcePath = QString("%1%2%3.py")
                                 .arg(filePath)
                                 .arg(QDir::separator())
                                 .arg(fileBaseName);
    QString jsonSourcePath = QString("%1%2%3.json")
                               .arg(filePath)
                               .arg(QDir::separator())
                               .arg(fileBaseName);

    // Ensure the tomviz directory exists
    QStringList locations =
      QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
    QString home = locations[0];
    QString path = QString("%1%2tomviz").arg(home).arg(QDir::separator());
    QDir dir(path);
    // dir.mkpath() returns true if the path already exists or if it was
    // successfully created.
    if (!dir.mkpath(path)) {
      QMessageBox::warning(
        this, "Could not create tomviz directory",
        QString("Could not create tomviz directory '%1'.").arg(path));
      return;
    }

    // Copy the Python file to the tomviz directory if it exists
    QFileInfo pythonFileInfo(pythonSourcePath);
    if (pythonFileInfo.exists()) {
      QString pythonDestPath =
        QString("%1%2%3.py").arg(path).arg(QDir::separator()).arg(fileBaseName);
      QFile::copy(pythonSourcePath, pythonDestPath);

      // Copy the JSON file if it exists.
      QFileInfo jsonFileInfo(jsonSourcePath);
      if (jsonFileInfo.exists()) {
        QString jsonDestPath = QString("%1%2%3.json")
                                 .arg(path)
                                 .arg(QDir::separator())
                                 .arg(fileBaseName);
        QFile::copy(jsonSourcePath, jsonDestPath);
      }

      // Register custom operators again.
      registerCustomOperators(findCustomOperators());
    }
  }
}

void MainWindow::showEvent(QShowEvent* e)
{
  QMainWindow::showEvent(e);
  if (m_isFirstShow) {
    m_isFirstShow = false;
    QTimer::singleShot(1, this, SLOT(onFirstWindowShow()));
  }
}

void MainWindow::closeEvent(QCloseEvent* e)
{
  if (ModuleManager::instance().hasRunningOperators()) {
    QMessageBox::StandardButton response =
      QMessageBox::question(this, "Close tomviz?",
                            "You have transforms that are not completed "
                            "running in the background. These may not exit "
                            "cleanly. Are "
                            "you sure you want to try exiting anyway?");
    if (response == QMessageBox::No) {
      e->ignore();
      return;
    }
  } else if (ModuleManager::instance().hasDataSources()) {
    QMessageBox::StandardButton response =
      QMessageBox::question(this, "Close?", "Are you sure you want to exit?");
    if (response == QMessageBox::No) {
      e->ignore();
      return;
    }
  }
  // This is a little hackish, but we must ensure all PV proxy unregister calls
  // happen early enough in application destruction that the ParaView proxy
  // management code can still run without segfaulting.
  PipelineManager::instance().removeAllPipelines();
  ModuleManager::instance().removeAllModules();
  ModuleManager::instance().removeAllDataSources();
  ModuleManager::instance().removeAllMoleculeSources();
  e->accept();
}

bool MainWindow::checkOpenGL()
{
  // Check for required OpenGL version, and pop up a warning dialog if needed.
  QSurfaceFormat format;
  format.setVersion(3, 2);
  QOffscreenSurface offScreen;
  offScreen.setFormat(format);
  offScreen.create();
  QOpenGLContext context;
  context.setFormat(format);
  context.create();
  context.makeCurrent(&offScreen);
  if (!context.isValid() || context.format().version() < qMakePair(3, 2)) {
    QMessageBox::critical(this, "Tomviz",
                          "This system does not support OpenGL 3.2 or higher. "
                          "The application is unlikely to function correctly.");
    return false;
  }
  return true;
}

void MainWindow::onFirstWindowShow()
{
  QFile file(getAutosaveFile());
  if (!file.exists()) {
    QSettings* settings = pqApplicationCore::instance()->settings();
    bool showWelcome =
      settings->value("GeneralSettings.ShowWelcomeDialog", true).toBool();
    if (showWelcome) {
      QString path =
        QApplication::applicationDirPath() + "/../share/tomviz/Data";
      path += "/Recon_NanoParticle_doi_10.1021-nl103400a.emd";
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
                          "There is an autosave file present. Load it?",
                          QMessageBox::Yes | QMessageBox::No);
  if (response == QMessageBox::Yes) {
    SaveLoadStateReaction::loadState(getAutosaveFile());
  }
}

void MainWindow::autosave()
{
  SaveLoadStateReaction::saveState(getAutosaveFile(), false);
}

void MainWindow::handleMessage(const QString&, int type)
{
  QDockWidget* dock = m_ui->dockWidgetMessages;
  if (!dock->isVisible() && (type == QtCriticalMsg || type == QtWarningMsg)) {
    // if dock is not visible, we always pop it up as a floating dialog. This
    // avoids causing re-renders which may cause more errors and more confusion.
    QRect rectApp = geometry();

    QRect rectDock(QPoint(0, 0), QSize(static_cast<int>(rectApp.width() * 0.4),
                                       dock->sizeHint().height()));
    rectDock.moveCenter(QPoint(
      rectApp.center().x(), rectApp.bottom() - dock->sizeHint().height() / 2));
    dock->setFloating(true);
    dock->setGeometry(rectDock);
    dock->show();
  }
  if (dock->isVisible()) {
    dock->raise();
  }
}

void MainWindow::registerCustomOperators(
  std::vector<OperatorDescription> operators)
{
  // Always create the Custom Transforms menu so that it is possible to import
  // new operators.
  m_customTransformsMenu->clear();

  QAction* importCustomTransformAction =
    m_customTransformsMenu->addAction("Import Custom Transform...");
  m_customTransformsMenu->addSeparator();
  connect(importCustomTransformAction, SIGNAL(triggered()),
          SLOT(importCustomTransform()));

  if (!operators.empty()) {
    for (const OperatorDescription& op : operators) {
      QAction* action = m_customTransformsMenu->addAction(op.label);
      action->setEnabled(op.valid);
      if (!op.loadError.isNull()) {
        qWarning().noquote()
          << QString("An error occurred trying to load an operator from '%1':")
               .arg(op.pythonPath);
        qWarning().noquote() << op.loadError;
        continue;
      } else if (!op.valid) {
        qWarning().noquote()
          << QString("'%1' doesn't contain a valid operator definition.")
               .arg(op.pythonPath);
        continue;
      }

      QString source;
      QString json;
      // Read the Python source
      QFile pythonFile(op.pythonPath);
      if (pythonFile.open(QIODevice::ReadOnly)) {
        source = pythonFile.readAll();
      } else {
        qCritical() << QString("Unable to read '%1'.").arg(op.pythonPath);
      }
      // Read the JSON if we have any
      if (!op.jsonPath.isNull()) {
        QFile jsonFile(op.jsonPath);
        if (jsonFile.open(QIODevice::ReadOnly)) {
          json = jsonFile.readAll();
        } else {
          qCritical() << QString("Unable to read '%1'.").arg(op.jsonPath);
        }
      }
      new AddPythonTransformReaction(action, op.label, source, false, false,
                                     false, json);
    }
  }
  m_customTransformsMenu->setEnabled(true);
}

std::vector<OperatorDescription> MainWindow::findCustomOperators()
{
  QStringList paths;
  // Search in <home>/.tomviz
  foreach (QString home,
           QStandardPaths::standardLocations(QStandardPaths::HomeLocation)) {
    QString path = QString("%1%2.tomviz").arg(home).arg(QDir::separator());
    if (QFile(path).exists()) {
      paths.append(path);
    }
    path = QString("%1%2tomviz").arg(home).arg(QDir::separator());
    if (QFile(path).exists()) {
      paths.append(path);
    }
  }
  // Search in data locations.
  // For example on window C:/Users/<USER>/AppData/Local/tomviz
  foreach (QString path,
           QStandardPaths::standardLocations(QStandardPaths::DataLocation)) {
    if (QFile(path).exists()) {
      paths.append(path);
    }
  }

  std::vector<OperatorDescription> operators;
  foreach (QString path, paths) {
    std::vector<OperatorDescription> ops = tomviz::findCustomOperators(path);
    operators.insert(operators.end(), ops.begin(), ops.end());
  }

  // Sort so we get a consistent order each time we load
  std::sort(operators.begin(), operators.end(),
            [](const OperatorDescription& op1, const OperatorDescription& op2) {
              return op1.label < op2.label;
            });

  return operators;
}

void MainWindow::setEnabledPythonConsole(bool enabled)
{
  m_ui->dockWidgetPythonConsole->setEnabled(enabled);
}

void MainWindow::syncPythonToApp()
{
  Python python;
  auto tomvizState = python.import("tomviz.state");
  if (!tomvizState.isValid()) {
    qCritical() << "Failed to import tomviz.state";
    return;
  }

  auto sync = tomvizState.findFunction("sync");
  if (!sync.isValid()) {
    qCritical() << "Unable to locate sync.";
    return;
  }

  Python::Tuple args(0);
  Python::Dict kwargs;
  sync.call(args, kwargs);
}

} // namespace tomviz
