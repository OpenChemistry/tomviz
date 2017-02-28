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
#ifndef tomvizDeleteDataReaction_h
#define tomvizDeleteDataReaction_h

#include <pqReaction.h>

#include <QPointer>

class vtkSMProxy;

namespace tomviz {
class DataSource;

/// DeleteDataReaction handles the "Delete Data" action in tomviz. On trigger,
/// this will delete the active data source and all Modules connected to it.
class DeleteDataReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  DeleteDataReaction(QAction* parentAction);
  virtual ~DeleteDataReaction();

  /// Create a raw data source from the reader.
  static void deleteDataSource(DataSource*);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;
  void updateEnableState() override;

private slots:
  void activeDataSourceChanged();

private:
  Q_DISABLE_COPY(DeleteDataReaction)

  QPointer<DataSource> m_activeDataSource;
};
}
#endif
