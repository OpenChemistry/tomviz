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

#include "pqFiltersMenuReaction.h"
#include "pqMacroReaction.h"
#include "pqPVApplicationCore.h"
#include "pqProxyGroupMenuManager.h"
#include "pqProxyGroupMenuManager.h"
#include "pqPythonShellReaction.h"
#include "pqSaveAnimationReaction.h"
#include "pqSaveDataReaction.h"
#include "pqSaveScreenshotReaction.h"
#include "pqSaveStateReaction.h"
#include "vtkPVPlugin.h"

#include "ActiveObjects.h"
#include "AddExpressionReaction.h"
#include "Behaviors.h"
#include "CloneDataReaction.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "ModuleMenu.h"
#include "RecentFilesMenu.h"



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
  Ui::MainWindow Ui;
};

//-----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* _parent, Qt::WindowFlags _flags)
  : Superclass(_parent, _flags),
  Internals(new MainWindow::MWInternals())
{
  Ui::MainWindow& ui = this->Internals->Ui;
  ui.setupUi(this);

  this->setWindowTitle("TomViz");

  // Link the histogram in the central widget to the active data source.
  ui.centralWidget->connect(&ActiveObjects::instance(),
    SIGNAL(dataSourceChanged(DataSource*)),
    SLOT(setDataSource(DataSource*)));

  // connect quit.
  pqApplicationCore::instance()->connect(
    ui.actionExit, SIGNAL(triggered()), SLOT(quit()));

  new pqPythonShellReaction(ui.actionPythonConsole);
  new pqMacroReaction(ui.actionMacros);

  // Instantiate TomViz application behavior.
  new Behaviors(this);


  new LoadDataReaction(ui.actionOpen);

  new CloneDataReaction(ui.actionClone);
  new AddExpressionReaction(ui.actionPython_Expression);

  new ModuleMenu(ui.menuModules);
  new RecentFilesMenu(*ui.menuRecentlyOpened, ui.menuRecentlyOpened);
  new pqSaveStateReaction(ui.actionSave);

  new pqSaveScreenshotReaction(ui.actionSaveScreenshot);
  new pqSaveAnimationReaction(ui.actionSaveMovie);

  //now init the optional dax plugins
#ifdef DAX_DEVICE_ADAPTER
  PV_PLUGIN_IMPORT(tomvizThreshold);
  PV_PLUGIN_IMPORT(tomvizStreaming);
#endif
}

//-----------------------------------------------------------------------------
MainWindow::~MainWindow()
{
  ModuleManager::instance().reset();
  delete this->Internals;
}
}
