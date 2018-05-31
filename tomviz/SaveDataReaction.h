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
#ifndef tomvizSaveDataReaction_h
#define tomvizSaveDataReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;
class PythonWriterFactory;

/// SaveDataReaction handles the "Save Data" action in tomviz. On trigger,
/// this will save the data file.
class SaveDataReaction : public pqReaction
{
  Q_OBJECT

public:
  SaveDataReaction(QAction* parentAction);

  /// Save the file
  bool saveData(const QString& filename);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(SaveDataReaction)

  static void registerPythonWriters();

  static bool m_registeredPythonWriters;
  static QList<PythonWriterFactory*> m_pythonWriters;
  static QMap<QString, int> m_pythonExtWriterMap;
};
} // namespace tomviz
#endif
