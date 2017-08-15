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
#ifndef tomvizExportDataReaction_h
#define tomvizExportDataReaction_h

#include <pqReaction.h>

namespace tomviz {
class Module;

/// ExportDataReaction handles the "Export as ..." action in tomviz. On trigger,
/// this will save the data from the active module (or the module that is set)
/// to a file
class ExportDataReaction : public pqReaction
{
  Q_OBJECT

public:
  ExportDataReaction(QAction* parentAction, Module* module = nullptr);

  /// Save the file
  bool exportData(const QString& filename);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Module* m_module;

  Q_DISABLE_COPY(ExportDataReaction)
};
}
#endif
