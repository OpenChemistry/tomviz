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
#ifndef tomvizSaveWebReaction_h
#define tomvizSaveWebReaction_h

#include <pqReaction.h>

namespace tomviz {

class MainWindow;

/// SaveWebReaction handles the "Save Web" action in tomviz. On trigger,
/// this will save several files for web viewing.
class SaveWebReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveWebReaction(QAction* parentAction, MainWindow* mainWindow);

  /// Save the file
  void saveWeb(QMap<QString, QVariant> kwargs);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  MainWindow* m_mainWindow;
};
}
#endif
