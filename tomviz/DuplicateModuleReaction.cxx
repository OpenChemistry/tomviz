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

#include "DuplicateModuleReaction.h"

#include "ActiveObjects.h"
#include "Module.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"

#include <QJsonObject>

namespace tomviz {

DuplicateModuleReaction::DuplicateModuleReaction(QAction* action)
  : pqReaction(action)
{
  connect(&ActiveObjects::instance(), &ActiveObjects::moduleChanged, this,
          &DuplicateModuleReaction::updateEnableState);
  updateEnableState();
}

void DuplicateModuleReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeModule() !=
                             nullptr);
}

void DuplicateModuleReaction::onTriggered()
{
  auto module = ActiveObjects::instance().activeModule();
  auto dataSource = module->dataSource();
  auto view = ActiveObjects::instance().activeView();
  auto moduleType = ModuleFactory::moduleType(module);
  if (ModuleFactory::moduleApplicable(moduleType, dataSource, view)) {
    // Copy the module
    auto copy = ModuleFactory::createModule(moduleType, dataSource, view, module->operatorResult());
    // Copy its settings
    QJsonObject json = module->serialize();
    copy->deserialize(json);
    ModuleManager::instance().addModule(copy);
  }
}

} // end namespace tomviz
