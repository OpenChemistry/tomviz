/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineView_h
#define tomvizPipelineView_h

#include <QTreeView>

#include <QPointer>

#include "EditOperatorDialog.h"

class vtkTable;

namespace tomviz {

class DataSource;
class MoleculeSource;
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
  void setCurrent(MoleculeSource* dataSource);
  void setCurrent(Module* module);
  void setCurrent(Operator* op);
  void deleteItemsConfirm(const QModelIndexList& idxs);
  void setModuleVisibility(const QModelIndexList& idxs, bool visible);
  void exportTableAsJson(vtkTable*);
};
} // namespace tomviz

#endif // tomvizPipelineView_h
