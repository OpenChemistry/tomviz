/******************************************************************************

  This source file is part of the TEM tomography project.

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
#include "pqViewMenuManager.h"
#include "vtkPVPlugin.h"

#include "ActiveObjects.h"
#include "AddAlignReaction.h"
#include "AddExpressionReaction.h"
#include "AddPythonTransformReaction.h"
#include "Behaviors.h"
#include "CloneDataReaction.h"
#include "DeleteDataReaction.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "ModuleMenu.h"
#include "RecentFilesMenu.h"
#include "ResetReaction.h"
#include "SaveDataReaction.h"
#include "SaveLoadStateReaction.h"

#include "alignimages.h"
#include "misalignimagespoisson.h"
#include "misalignimagesuniform.h"
#include "reconstructdft.h"

//we are building with dax, so we have plugins to import
#ifdef DAX_DEVICE_ADAPTER
  // Adds required forward declarations.
  PV_PLUGIN_IMPORT_INIT(tomvizThreshold);
  PV_PLUGIN_IMPORT_INIT(tomvizStreaming);
#endif

namespace TEM
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

  setWindowTitle("tomviz");

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

  // Instantiate TomViz application behavior.
  new Behaviors(this);

  new LoadDataReaction(ui.actionOpen);
  new DeleteDataReaction(ui.actionDeleteData);

  new AddAlignReaction(ui.actionAlign);
  new CloneDataReaction(ui.actionClone);

  // Add our Python script reactions, these compose Python into menu entries.
  new AddPythonTransformReaction(ui.actionReconstruct, "Reconstruct Volume",
                                 reconstructdft);

  QAction *alignAction = new QAction("Align Images (auto)", this);
  ui.menuData->insertAction(ui.actionReconstruct, alignAction);
  new AddPythonTransformReaction(alignAction, "Align Images (auto)",
                                 alignimages);
  QAction *misalignPoisson = new QAction("Misalign Images (Poisson)", this);
  ui.menuData->insertAction(ui.actionReconstruct, misalignPoisson);
  new AddPythonTransformReaction(misalignPoisson, "Misalign Images (Poisson)",
                                 misalignimagespoisson);
  QAction *misalignUniform = new QAction("Misalign Images (Uniform)", this);
  ui.menuData->insertAction(ui.actionReconstruct, misalignUniform);
  new AddPythonTransformReaction(misalignUniform, "Misalign Images (Uniform)",
                                 misalignimagesuniform);

  new AddExpressionReaction(ui.actionPython_Expression);

  new ModuleMenu(ui.modulesToolbar, ui.menuModules, this);
  new RecentFilesMenu(*ui.menuRecentlyOpened, ui.menuRecentlyOpened);
  new pqSaveStateReaction(ui.actionSaveDebuggingState);

  new SaveDataReaction(ui.actionSaveData);
  new pqSaveScreenshotReaction(ui.actionSaveScreenshot);
  new pqSaveAnimationReaction(ui.actionSaveMovie);

  new SaveLoadStateReaction(ui.actionSaveState);
  new SaveLoadStateReaction(ui.actionLoadState, /*load*/ true);

  new ResetReaction(ui.actionReset);

  new pqViewMenuManager(this, ui.menuView);

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
    }
  this->Internals->AboutDialog->show();
}

}
