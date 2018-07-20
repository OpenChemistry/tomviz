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
#ifndef tomvizSetDataTypeReaction_h
#define tomvizSetDataTypeReaction_h

#include <pqReaction.h>

#include "DataSource.h"

class QMainWindow;

namespace tomviz {

class DataSource;

class SetDataTypeReaction : public pqReaction
{
  Q_OBJECT

public:
  SetDataTypeReaction(QAction* action, QMainWindow* mw,
                      DataSource::DataSourceType t = DataSource::Volume);

  static void setDataType(QMainWindow* mw, DataSource* source = nullptr,
                          DataSource::DataSourceType t = DataSource::Volume);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;
  void updateEnableState() override;

private:
  QMainWindow* m_mainWindow;
  DataSource::DataSourceType m_type;

  void setWidgetText(DataSource::DataSourceType t);

  Q_DISABLE_COPY(SetDataTypeReaction)
};
} // namespace tomviz

#endif
