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
#include "ModuleMenu.h"

#include "ModuleManager.h"
#include "ModuleFactory.h"
#include "ActiveObjects.h"
#include <QMenu>

namespace TEM
{

//-----------------------------------------------------------------------------
ModuleMenu::ModuleMenu(QMenu* parentObject) : Superclass(parentObject)
{
  Q_ASSERT(parentObject);
  this->connect(parentObject, SIGNAL(aboutToShow()), SLOT(aboutToShow()));
  this->connect(parentObject, SIGNAL(triggered(QAction*)), SLOT(triggered(QAction*)));
}

//-----------------------------------------------------------------------------
ModuleMenu::~ModuleMenu()
{
}

//-----------------------------------------------------------------------------
QMenu* ModuleMenu::parentMenu() const
{
  return qobject_cast<QMenu*>(this->parent());
}

//-----------------------------------------------------------------------------
void ModuleMenu::aboutToShow()
{
  QMenu* menu = this->parentMenu();
  Q_ASSERT(menu);

  menu->clear();
  QList<QString> modules =
      ModuleFactory::moduleTypes(ActiveObjects::instance().activeDataSource(),
                                 ActiveObjects::instance().activeView());

  if (modules.size() > 0)
    {
    foreach (const QString& txt, modules)
      {
      menu->addAction(txt);
      }
    }
  else
    {
    QAction* action = menu->addAction("No modules available");
    action->setEnabled(false);
    }
}

//-----------------------------------------------------------------------------
void ModuleMenu::triggered(QAction* maction)
{
  Module* module =
      ModuleFactory::createModule(maction->text(),
                                  ActiveObjects::instance().activeDataSource(),
                                  ActiveObjects::instance().activeView());
  if (module)
    {
    ModuleManager::instance().addModule(module);
    ActiveObjects::instance().setActiveModule(module);
    }
  else
    {
    qCritical("Failed to create requested module.");
    }
}

} // end of namespace TEM
