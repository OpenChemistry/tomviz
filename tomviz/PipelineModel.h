/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
class MoleculeSource;
class Module;
class Operator;
class OperatorResult;

class PipelineModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  explicit PipelineModel(QObject* parent = 0);
  ~PipelineModel() override;

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
  MoleculeSource* moleculeSource(const QModelIndex& index);
  Module* module(const QModelIndex& index);
  Operator* op(const QModelIndex& index);
  OperatorResult* result(const QModelIndex& index);

  QModelIndex dataSourceIndex(DataSource* source);
  QModelIndex moleculeSourceIndex(MoleculeSource* source);
  QModelIndex moduleIndex(Module* module);
  QModelIndex operatorIndex(Operator* op);
  QModelIndex resultIndex(OperatorResult* result);

  bool removeDataSource(DataSource* dataSource);
  bool removeMoleculeSource(MoleculeSource* moleculeSource);
  bool removeModule(Module* module);
  bool removeOp(Operator* op);
  bool removeResult(OperatorResult* result);

public slots:
  void dataSourceAdded(DataSource* dataSource);
  void moleculeSourceAdded(MoleculeSource* moleculeSource);
  void moduleAdded(Module* module);
  void operatorAdded(Operator* op, DataSource* transformedDataSource = nullptr);
  void operatorRemoved(Operator* op);
  void operatorModified();
  void operatorTransformDone();

  void dataSourceRemoved(DataSource* dataSource);
  void moduleRemoved(Module* module);
  void moleculeSourceRemoved(MoleculeSource* moleculeSource);
  void childDataSourceAdded(DataSource* dataSource);
  void childDataSourceRemoved(DataSource* dataSource);
  void dataSourceMoved(DataSource* dataSource);

signals:
  void dataSourceItemAdded(DataSource* dataSource);
  void childDataSourceItemAdded(DataSource* dataSource);
  void moleculeSourceItemAdded(MoleculeSource* dataSource);
  void moduleItemAdded(Module* module);
  void operatorItemAdded(Operator* op);
  void dataSourceModified(DataSource* dataSource);

private:
  struct Item;
  class TreeItem;

  TreeItem* treeItem(const QModelIndex& index) const;

  QList<TreeItem*> m_treeItems;

  QModelIndex dataSourceIndexHelper(PipelineModel::TreeItem* treeItem,
                                    DataSource* source);
  QModelIndex operatorIndexHelper(PipelineModel::TreeItem* treeItem,
                                  Operator* op);
  void moveDataSourceHelper(DataSource* dataSource, Operator* newParent);
};

} // namespace tomviz

#endif // tomvizPipelineModel_h
