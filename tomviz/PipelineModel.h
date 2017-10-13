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
#ifndef tomvizPipelineModel_h
#define tomvizPipelineModel_h

#include <QAbstractItemModel>

namespace {

enum Column
{
  label,
  state
};
}

namespace tomviz {

class DataSource;
class Module;
class Operator;
class OperatorResult;

class PipelineModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  explicit PipelineModel(QObject* parent = 0);
  ~PipelineModel();

  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;

  DataSource* dataSource(const QModelIndex& index);
  Module* module(const QModelIndex& index);
  Operator* op(const QModelIndex& index);
  OperatorResult* result(const QModelIndex& index);

  QModelIndex dataSourceIndex(DataSource* source);
  QModelIndex moduleIndex(Module* module);
  QModelIndex operatorIndex(Operator* op);
  QModelIndex resultIndex(OperatorResult* result);

  bool removeDataSource(DataSource* dataSource);
  bool removeModule(Module* module);
  bool removeOp(Operator* op);
  bool removeResult(OperatorResult* result);

public slots:
  void dataSourceAdded(DataSource* dataSource);
  void moduleAdded(Module* module);
  void operatorAdded(Operator* op);
  void operatorModified();
  void operatorTransformDone();

  void dataSourceRemoved(DataSource* dataSource);
  void moduleRemoved(Module* module);
  void childDataSourceAdded(DataSource* dataSource);
  void childDataSourceRemoved(DataSource* dataSource);

private:
  struct Item;
  class TreeItem;

  TreeItem* treeItem(const QModelIndex& index) const;

  QList<TreeItem*> m_treeItems;

  QModelIndex dataSourceIndexHelper(PipelineModel::TreeItem* treeItem,
                                    DataSource* source);
  QModelIndex operatorIndexHelper(PipelineModel::TreeItem* treeItem,
                                  Operator* op);
};

} // tomviz namespace

#endif // tomvizPipelineModel_h
