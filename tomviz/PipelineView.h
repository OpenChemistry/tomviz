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
#ifndef tomvizPipelineView_h
#define tomvizPipelineView_h

#include <QTreeView>

#include <QPointer>

#include "EditOperatorDialog.h"

namespace tomviz {

class DataSource;
class Module;
class Operator;

class PipelineView : public QTreeView
{
  Q_OBJECT

public:
  PipelineView(QWidget* parent = nullptr);

  void setModel(QAbstractItemModel*) override;
  void initLayout();

protected:
  void keyPressEvent(QKeyEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void currentChanged(const QModelIndex& current,
                      const QModelIndex& previous) override;
  void deleteItems(const QModelIndexList& idxs);
  bool enableDeleteItems(const QModelIndexList& idxs);

private slots:
  void rowActivated(const QModelIndex& idx);
  void rowDoubleClicked(const QModelIndex& idx);

  void setCurrent(DataSource* dataSource);
  void setCurrent(Module* module);
  void setCurrent(Operator* op);
  void deleteItemsConfirm(const QModelIndexList& idxs);
  void setModuleVisibility(const QModelIndexList& idxs, bool visible);
};
}

#endif // tomvizPipelineView_h
