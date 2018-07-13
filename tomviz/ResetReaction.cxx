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
#include "ResetReaction.h"

#include "ModuleManager.h"
#include "Utilities.h"

#include <QMessageBox>

namespace tomviz {

ResetReaction::ResetReaction(QAction* parentObject) : Superclass(parentObject)
{}

void ResetReaction::updateEnableState()
{
  bool enabled = !ModuleManager::instance().hasRunningOperators();
  parentAction()->setEnabled(enabled);
}

void ResetReaction::reset()
{
  if (ModuleManager::instance().hasDataSources()) {
    if (QMessageBox::Yes !=
        QMessageBox::warning(tomviz::mainWidget(), "Reset Warning",
                             "Current data and operators will be cleared when "
                             "resetting.  Proceed anyway?",
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)) {
      return;
    }
  }
  ModuleManager::instance().reset();
}
} // namespace tomviz
