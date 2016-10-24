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

namespace tomviz {

ModuleMenu::ModuleMenu(QToolBar* toolBar, QMenu* menu, QObject* parentObject)
  : QObject(parentObject), m_menu(menu), m_toolBar(toolBar)
{
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);
  connect(menu, SIGNAL(triggered(QAction*)), SLOT(triggered(QAction*)));
  connect(&ActiveObjects::instance(),
          SIGNAL(dataSourceChanged(DataSource*)), SLOT(updateActions()));
  updateActions();
}

ModuleMenu::~ModuleMenu()
{
}

void ModuleMenu::updateActions()
{
  QMenu* menu = m_menu;
  QToolBar* toolBar = m_toolBar;
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);

  menu->clear();
  toolBar->clear();
  QList<QString> modules =
    ModuleFactory::moduleTypes(ActiveObjects::instance().activeDataSource(),
                               ActiveObjects::instance().activeView());
  if (modules.size() > 0) {
    foreach (const QString& txt, modules) {
      auto actn = menu->addAction(ModuleFactory::moduleIcon(txt), txt);
      toolBar->addAction(actn);
      actn->setData(txt);
    }
  } else {
    auto action = menu->addAction("No modules available");
    action->setEnabled(false);
    toolBar->addAction(action);
  }
}

void ModuleMenu::triggered(QAction* maction)
{
  auto module = ModuleManager::instance().createAndAddModule(
    maction->data().toString(), ActiveObjects::instance().activeDataSource(),
    ActiveObjects::instance().activeView());
  if (module) {
    ActiveObjects::instance().setActiveModule(module);
  } else {
    qCritical("Failed to create requested module.");
  }
}

} // end of namespace tomviz
