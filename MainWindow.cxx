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
#include "pqProxyGroupMenuManager.h"
#include "pqProxyGroupMenuManager.h"
#include "pqPVApplicationCore.h"
#include "pqPythonShellReaction.h"
#include "pqMacroReaction.h"
#include "pqSaveDataReaction.h"

#include "ActiveObjects.h"
#include "Behaviors.h"
#include "LoadDataReaction.h"

namespace TEM
{

class MainWindow::MWInternals
{
public:
  Ui::MainWindow Ui;
};

MainWindow::MainWindow(QWidget* _parent, Qt::WindowFlags _flags)
  : Superclass(_parent, _flags),
  Internals(new MainWindow::MWInternals())
{
  Ui::MainWindow& ui = this->Internals->Ui;
  ui.setupUi(this);

  this->setWindowTitle("Materials Visualization Environment");

  // Link the histogram in the central widget to the active data source.
  ui.centralWidget->connect(&ActiveObjects::instance(),
    SIGNAL(dataSourceChanged(pqPipelineSource*)),
    SLOT(setDataSource(pqPipelineSource*)));

  // connect quit.
  pqApplicationCore::instance()->connect(
    ui.actionExit, SIGNAL(triggered()), SLOT(quit()));

  //pqProxyGroupMenuManager* mgr =
  //  new pqProxyGroupMenuManager(ui.menuFilters, QString("NotUsed"));
  //mgr->setRecentlyUsedMenuSize(0);
  //pqPVApplicationCore::instance()->registerForQuicklaunch(
  //  mgr->widgetActionsHolder());
  //new pqFiltersMenuReaction(mgr);

  //mgr->addProxy("filters", "Clip");
  //mgr->addProxy("filters", "Contour");
  //mgr->addProxy("filters", "Cut");
  //mgr->addProxy("filters", "ExtractHistogram");
  //mgr->addProxy("filters", "Calculator");
  //mgr->addProxy("filters", "PlotAttributes");
  //mgr->addProxy("filters", "ProbeLine");
  //mgr->addProxy("filters", "PythonCalculator");
  //mgr->addProxy("filters", "ProgrammableFilter");
  //mgr->populateMenu();

  //new pqLoadDataReaction(ui.actionOpen);
  //new pqSaveDataReaction(ui.actionSave);



  //new pqPythonShellReaction(ui.actionPythonConsole);
  //new pqMacroReaction(ui.actionMacros);

  // Instantiate MatViz application behavior.
  new Behaviors(this);


  new LoadDataReaction(ui.actionOpen);
}

MainWindow::~MainWindow()
{
  delete this->Internals;
}

}
