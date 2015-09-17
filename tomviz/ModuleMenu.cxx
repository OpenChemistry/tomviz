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
#include "ModuleMenu.h"

#include "ActiveObjects.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"

#include <QMenu>
#include <QToolBar>

namespace tomviz
{

//-----------------------------------------------------------------------------
ModuleMenu::ModuleMenu(QToolBar* toolBar, QMenu* menu, QObject* parentObject) :
  Superclass(parentObject),
  Menu(menu),
  ToolBar(toolBar)
{
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);
  this->connect(menu, SIGNAL(triggered(QAction*)), SLOT(triggered(QAction*)));
  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateActions()));
  this->updateActions();
}

//-----------------------------------------------------------------------------
ModuleMenu::~ModuleMenu()
{
}

//-----------------------------------------------------------------------------
void ModuleMenu::updateActions()
{
  QMenu* menu = this->Menu;
  QToolBar* toolBar = this->ToolBar;
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);

  menu->clear();
  toolBar->clear();
  QList<QString> modules =
      ModuleFactory::moduleTypes(ActiveObjects::instance().activeDataSource(),
                                 ActiveObjects::instance().activeView());
  if (modules.size() > 0)
  {
    foreach (const QString& txt, modules)
    {
      QAction* actn = menu->addAction(ModuleFactory::moduleIcon(txt), txt);
      toolBar->addAction(actn);
    }
  }
  else
  {
    QAction* action = menu->addAction("No modules available");
    action->setEnabled(false);
    toolBar->addAction(action);
  }
}

//-----------------------------------------------------------------------------
void ModuleMenu::triggered(QAction* maction)
{
  Module* module =
    ModuleManager::instance().createAndAddModule(maction->text(),
      ActiveObjects::instance().activeDataSource(),
      ActiveObjects::instance().activeView());
  if (module)
  {
    ActiveObjects::instance().setActiveModule(module);
  }
  else
  {
    qCritical("Failed to create requested module.");
  }
}

} // end of namespace tomviz
