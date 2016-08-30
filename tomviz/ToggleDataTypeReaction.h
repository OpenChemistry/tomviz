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
#ifndef tomvizToggleDataTypeReaction_h
#define tomvizToggleDataTypeReaction_h

#include <pqReaction.h>

class QMainWindow;

namespace tomviz {

class DataSource;

class ToggleDataTypeReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  ToggleDataTypeReaction(QAction* action, QMainWindow* mw);
  ~ToggleDataTypeReaction();

  static void toggleDataType(QMainWindow* mw, DataSource* source = nullptr);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;
  void updateEnableState() override;

private:
  void setWidgetText(DataSource* dsource);

  QMainWindow* mainWindow;

  Q_DISABLE_COPY(ToggleDataTypeReaction)
};
}

#endif
