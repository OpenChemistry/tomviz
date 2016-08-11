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
#include "PipelineModel.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Operator.h"

#include <QFileInfo>

namespace tomviz {

struct PipelineModel::Item
{
  Item(DataSource *source) : tag(DATASOURCE), s(source) { }
  Item(Module *module) : tag(MODULE), m(module) { }
  Item(Operator *op) : tag(OPERATOR), o(op) { }

  DataSource* dataSource() { return tag == DATASOURCE ? s : nullptr; }
  Module* module() { return tag == MODULE ? m : nullptr; }
  Operator* op() { return tag == OPERATOR ? o : nullptr; }

  enum { DATASOURCE, MODULE, OPERATOR } tag;
  union
  {
    DataSource *s;
    Module *m;
    Operator *o;
  };
};

class PipelineModel::TreeItem
{
public:
  explicit TreeItem(const PipelineModel::Item &item,
                    TreeItem *parent = nullptr);
  ~TreeItem();

  TreeItem* parent() { return m_parent; }
  TreeItem* child(int index);
  int childCount() const { return m_children.count(); }
  QList<TreeItem*>& children() { return m_children; }
  int childIndex() const;
  bool appendChild(const PipelineModel::Item &item);
  bool appendAndMoveChildren(const PipelineModel::Item &item);
  bool insertChild(int position, const PipelineModel::Item &item);
  bool removeChild(int position);
  bool moveChildren(TreeItem *newParent);

  bool remove(DataSource *source);
  bool remove(Module *module);
  bool remove(Operator *op);

  bool hasOp(Operator *op);

  TreeItem* find(Module *module);
  TreeItem* find(Operator *op);

  DataSource* dataSource() { return m_item.dataSource(); }
  Module* module() { return m_item.module(); }
  Operator* op() { return m_item.op(); }

private:
  QList<TreeItem*> m_children;
  PipelineModel::Item m_item;
  TreeItem *m_parent;
};

PipelineModel::TreeItem::TreeItem(const PipelineModel::Item &i, TreeItem *p)
  : m_item(i), m_parent(p)
{
}

PipelineModel::TreeItem::~TreeItem()
{
  qDeleteAll(m_children);
}

PipelineModel::TreeItem* PipelineModel::TreeItem::child(int index)
{
  return m_children.value(index);
}

int PipelineModel::TreeItem::childIndex() const
{
  if (m_parent) {
    return m_parent->m_children.indexOf(const_cast<TreeItem*>(this));
  }
  return 0;
}

bool PipelineModel::TreeItem::insertChild(int pos,
                                          const PipelineModel::Item &item)
{
  if (pos < 0 || pos >= m_children.size()) {
    return false;
  }
  TreeItem *treeItem = new TreeItem(item, this);
  m_children.insert(pos, treeItem);
  return true;
}

bool PipelineModel::TreeItem::appendChild(const PipelineModel::Item &item)
{
  TreeItem *treeItem = new TreeItem(item, this);
  m_children.append(treeItem);
  return true;
}

bool PipelineModel::TreeItem::appendAndMoveChildren(const PipelineModel::Item &item)
{
  auto treeItem = new TreeItem(item, this);
  // This enforces our desired hierarchy, if modules then move all to the
  // operator added, otherwise if already operators move to the last operator.
  if (childCount() && child(0)->module()) {
    moveChildren(treeItem);
  } else if (childCount() && child(0)->op()) {
    child(childCount() - 1)->moveChildren(treeItem);
  }
  m_children.append(treeItem);
  return true;
}

bool PipelineModel::TreeItem::removeChild(int pos)
{
  if (pos < 0 || pos >= m_children.size()) {
    return false;
  }
  delete m_children.takeAt(pos);
  return true;
}

bool PipelineModel::TreeItem::moveChildren(TreeItem *newParent)
{
  newParent->m_children.append(m_children);
  foreach(TreeItem *item, m_children) {
    item->m_parent = newParent;
  }
  m_children.clear();
  return true;
}

bool PipelineModel::TreeItem::remove(DataSource *source)
{
  if (source != dataSource()) {
    return false;
  }
  for (auto i = 0; i < childCount(); ++i) {
    if (child(i)->module()) {
      remove(child(i)->module());
    }
    else if (child(i)->op() && child(i)->childCount()) {
      for (auto j = 0; j < child(i)->childCount(); ++j) {
        child(i)->remove(child(i)->child(j)->module());
      }
    }
  }
  if (parent()) {
    parent()->removeChild(childIndex());
    return true;
  }
  return false;
}

bool PipelineModel::TreeItem::remove(Module *module)
{
  foreach(auto item, m_children) {
    if (item->op() && item->childCount()){
      return item->remove(module);
    } else if (item->module() == module) {
      removeChild(item->childIndex());
      return true;
    }
  }
  return false;
}

bool PipelineModel::TreeItem::remove(Operator *o)
{
  foreach(auto item, m_children) {
    if (item->op() == o) {
      if (item->childCount()) {
        if (item->childIndex() == 0) {
          item->moveChildren(this);
          removeChild(0);
        } else {
          item->moveChildren(child(item->childIndex() - 1));
          removeChild(item->childIndex());
        }
      }
      else {
        removeChild(item->childIndex());
      }
      return true;
    }
  }
  return false;
}

bool PipelineModel::TreeItem::hasOp(Operator *o)
{
  foreach(TreeItem *item, m_children) {
    if (item->op() == o) {
      return true;
    }
  }
  return false;
}

PipelineModel::TreeItem* PipelineModel::TreeItem::find(Module *module)
{
  foreach(auto treeItem, m_children) {
    if (treeItem->module() == module) {
      return treeItem;
    }
    foreach(auto childItem, treeItem->m_children) {
      if (childItem->module() == module) {
        return childItem;
      }
    }
  }
  return nullptr;
}

PipelineModel::TreeItem* PipelineModel::TreeItem::find(Operator *op)
{
  foreach(auto treeItem, m_children) {
    if (treeItem->op() == op) {
      return treeItem;
    }
    foreach(auto childItem, treeItem->m_children) {
      if (childItem->op() == op) {
        return childItem;
      }
    }
  }
  return nullptr;
}

PipelineModel::PipelineModel(QObject *p) : QAbstractItemModel(p)
{
  connect(&ModuleManager::instance(), SIGNAL(dataSourceAdded(DataSource*)),
          SLOT(dataSourceAdded(DataSource*)));
  connect(&ModuleManager::instance(), SIGNAL(moduleAdded(Module*)),
          SLOT(moduleAdded(Module*)));

  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          SIGNAL(modelReset()));
  connect(&ModuleManager::instance(), SIGNAL(dataSourceRemoved(DataSource*)),
          SLOT(dataSourceRemoved(DataSource*)));
  connect(&ModuleManager::instance(), SIGNAL(moduleRemoved(Module*)),
          SLOT(moduleRemoved(Module*)));
}

PipelineModel::~PipelineModel()
{
}

QVariant PipelineModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.column() > 2)
    return QVariant();

  // Data source
  if (!index.parent().isValid()) {
    auto treeItem = static_cast<TreeItem *>(index.internalPointer());
    auto source = treeItem->dataSource();

    if (index.column() == 0) {
      switch (role) {
        case Qt::DecorationRole:
          return QIcon(":/pqWidgets/Icons/pqInspect22.png");
        case Qt::DisplayRole:
          return QFileInfo(source->filename()).baseName();
        case Qt::ToolTipRole:
          return source->filename();
        default:
          return QVariant();
      }
    }
  }
  else {
    // Module or operator
    auto treeItem = static_cast<TreeItem *>(index.internalPointer());
    auto module = treeItem->module();
    auto op = treeItem->op();
    if (module) {
      if (index.column() == 0) {
        switch (role) {
          case Qt::DecorationRole:
            return module->icon();
          case Qt::DisplayRole:
            return module->label();
          case Qt::ToolTipRole:
            return module->label();
          default:
            return QVariant();
        }
      } else if (index.column() == 1) {
        if (role == Qt::DecorationRole) {
          if (module->visibility()) {
            return QIcon(":/pqWidgets/Icons/pqEyeball16.png");
          } else {
            return QIcon(":/pqWidgets/Icons/pqEyeballd16.png");
          }
        }
      }
    } else if (op) {
      if (index.column() == 0) {
        switch (role) {
          case Qt::DecorationRole:
            return op->icon();
          case Qt::DisplayRole:
            return op->label();
          case Qt::ToolTipRole:
            return op->label();
          default:
            return QVariant();
        }
      } else if (index.column() == 1) {
        if (role == Qt::DecorationRole) {
          return QIcon(":/QtWidgets/Icons/pqDelete32.png");
        }
      }
    }
  }
  return QVariant();
}

bool PipelineModel::setData(const QModelIndex &index, const QVariant &value,
                            int role)
{
  if (role != Qt::CheckStateRole) {
    return false;
  }

  auto treeItem = static_cast<TreeItem*>(index.internalPointer());
  if (index.column() == 1 && treeItem->module()) {
    treeItem->module()->setVisibility(value == Qt::Checked);
    emit dataChanged(index, index);
  }
  return true;
}

Qt::ItemFlags PipelineModel::flags(const QModelIndex &index) const
{
  if (!index.isValid())
    return 0;

  auto treeItem = static_cast<TreeItem*>(index.internalPointer());
  auto module = treeItem->module();
  auto view = ActiveObjects::instance().activeView();

  if (module && module->view() != view) {
    return Qt::NoItemFlags;
  }
  return QAbstractItemModel::flags(index);
}

QVariant PipelineModel::headerData(int, Qt::Orientation, int) const
{
  return QVariant();
}

QModelIndex PipelineModel::index(int row, int column,
                                 const QModelIndex &parent) const
{
  if (!parent.isValid() && row < m_treeItems.count()) {
    // Data source
    return createIndex(row, column, m_treeItems[row]);
  } else {
    // Module or operator
    auto treeItem = static_cast<TreeItem *>(parent.internalPointer());
    if (treeItem && row < treeItem->childCount()) {
      return createIndex(row, column, treeItem->child(row));
    }
  }

  return QModelIndex();
}

QModelIndex PipelineModel::parent(const QModelIndex &index) const
{
  if (!index.isValid()) {
    return QModelIndex();
  }
  auto treeItem = static_cast<TreeItem *>(index.internalPointer());
  if (!treeItem->parent()) {
    return QModelIndex();
  }
  return createIndex(treeItem->childIndex(), 0, treeItem->parent());
}

int PipelineModel::rowCount(const QModelIndex &parent) const
{
  if (!parent.isValid()) {
    return m_treeItems.count();
  } else {
    auto treeItem = static_cast<TreeItem*>(parent.internalPointer());
    return treeItem->childCount();
  }
}

int PipelineModel::columnCount(const QModelIndex &) const
{
  return 2;
}

DataSource* PipelineModel::dataSource(const QModelIndex &idx)
{
  if (!idx.isValid())
  {
    return nullptr;
  }
  auto treeItem = static_cast<TreeItem*>(idx.internalPointer());
  return (treeItem ? treeItem->dataSource() : nullptr);
}

Module* PipelineModel::module(const QModelIndex &idx)
{
  if (!idx.isValid())
  {
    return nullptr;
  }
  auto treeItem = static_cast<TreeItem*>(idx.internalPointer());
  return (treeItem ? treeItem->module() : nullptr);
}

Operator* PipelineModel::op(const QModelIndex &idx)
{
  if (!idx.isValid())
  {
    return nullptr;
  }
  auto treeItem = static_cast<TreeItem*>(idx.internalPointer());
  return (treeItem ? treeItem->op() : nullptr);
}

QModelIndex PipelineModel::dataSourceIndex(DataSource *source)
{
  for (int i = 0; i < m_treeItems.count(); ++i) {
    if (m_treeItems[i]->dataSource() == source) {
      return createIndex(i, 0, m_treeItems[i]);
    }
  }
  return QModelIndex();
}

QModelIndex PipelineModel::moduleIndex(Module *module)
{
  foreach(auto treeItem, m_treeItems) {
    auto moduleItem = treeItem->find(module);
    if (moduleItem) {
      return createIndex(moduleItem->childIndex(), 0, moduleItem);
    }
  }
  return QModelIndex();
}

QModelIndex PipelineModel::operatorIndex(Operator *op)
{
  foreach(auto treeItem, m_treeItems) {
    auto operatorItem = treeItem->find(op);
    if (operatorItem) {
      return createIndex(operatorItem->childIndex(), 0, operatorItem);
    }
  }
  return QModelIndex();
}

void PipelineModel::dataSourceAdded(DataSource *dataSource)
{
  m_dataSources.append(dataSource);
  auto treeItem = new PipelineModel::TreeItem(PipelineModel::Item(dataSource));
  beginInsertRows(QModelIndex(), 0, 0);
  m_treeItems.append(treeItem);
  endInsertRows();
  connect(dataSource, SIGNAL(operatorAdded(Operator*)),
          SLOT(operatorAdded(Operator*)));

  // When restoring a data source from a state file it will have its operators
  // before we can listen to the signal above. Display those operators.
  foreach(auto op, dataSource->operators())
  {
    this->operatorAdded(op);
  }
}

void PipelineModel::moduleAdded(Module *module)
{
  Q_ASSERT(module);
  int idx = m_dataSources.indexOf(module->dataSource());

  Q_ASSERT(idx != -1);

  for (int i = 0; i < m_treeItems.count(); ++i) {
    if (module->dataSource() == m_treeItems[i]->dataSource()) {
      auto dataSourceItem = m_treeItems[i];
      if (dataSourceItem->childCount() && dataSourceItem->child(0)->op()) {
        // The data source has at least one operator as a child.
        // Set the module as a child of the last child operator.
        auto operatorItem = dataSourceItem->child(dataSourceItem->childCount() - 1);
        auto op = operatorItem->op();
        Q_ASSERT(op);
        auto parentIndex = operatorIndex(op);
        Q_ASSERT(parentIndex.isValid());
        auto row = operatorItem->childCount();
        beginInsertRows(parentIndex, row, row);
        operatorItem->appendChild(PipelineModel::Item(module));
        endInsertRows();
      } else {
        // No operators have been applied. Append directly to the data source item.
        auto row = dataSourceItem->childCount();
        beginInsertRows(dataSourceIndex(module->dataSource()), row, row);
        dataSourceItem->appendChild(PipelineModel::Item(module));
        endInsertRows();
      }
      break;
    }
  }
}

void PipelineModel::operatorAdded(Operator *op)
{
  // Operators are special, they operate on all data and are shown in the
  // visualization modules. So there are some moves necessary to show this.
  Q_ASSERT(op);
  auto dataSource = op->dataSource();
  Q_ASSERT(dataSource);
  connect(op, SIGNAL(labelModified()), this, SLOT(operatorModified()));
  for (int i = 0; i < m_treeItems.count(); ++i) {
    if (m_treeItems[i]->dataSource() == dataSource) {
      auto dataSourceItem = m_treeItems[i];
      if (dataSourceItem->childCount() && dataSourceItem->child(0)->op()) {
        // An operator already exists - add the new one to the end
        auto lastOperatorItem = dataSourceItem->child(dataSourceItem->childCount() - 1);
        auto lastOperator = lastOperatorItem->op();
        Q_ASSERT(lastOperator);
        auto parentIndex = operatorIndex(lastOperator);
        Q_ASSERT(parentIndex.isValid());
        auto row = lastOperatorItem->childCount();
        beginInsertRows(parentIndex, row, row);
        dataSourceItem->appendAndMoveChildren(PipelineModel::Item(op));
        endInsertRows();
      } else {
        // Only modules exist so far - add operator to data source and move the modules
        // to be children of the operator.
        auto parentIndex = dataSourceIndex(dataSource);
        auto row = dataSourceItem->childCount();
        beginInsertRows(parentIndex, row, row);;
        dataSourceItem->appendAndMoveChildren(PipelineModel::Item(op));
        endInsertRows();
      }
    }
  }
}

void PipelineModel::operatorModified()
{
  auto op = qobject_cast<Operator*>(sender());
  Q_ASSERT(op);
  /// TODO: Find the index, and update correctly.
  beginResetModel();
  endResetModel();
}

void PipelineModel::dataSourceRemoved(DataSource *source)
{
  foreach(auto item, m_treeItems) {
    if (item->dataSource() == source) {
      beginResetModel();
      item->remove(source);
      m_treeItems.removeAll(item);
      endResetModel();
      return;
    }
  }
}

void PipelineModel::moduleRemoved(Module *module)
{
  foreach(auto item, m_treeItems) {
    if (item->dataSource() == module->dataSource()) {
      beginResetModel();
      item->remove(module);
      endResetModel();
      return;
    }
  }
}

bool PipelineModel::removeDataSource(DataSource *source)
{
  foreach(auto item, m_treeItems) {
    if (item->dataSource() == source) {
      foreach(auto childItem, item->children()) {
        if (childItem->op() && childItem->childCount()) {
          foreach(auto moduleItem, childItem->children()) {
            removeModule(moduleItem->module());
          }
        } else if (childItem->module()) {
          removeModule(childItem->module());
        }
      }
    }
  }
  dataSourceRemoved(source);
  ModuleManager::instance().removeDataSource(source);
  return true;

  foreach(auto item, m_treeItems) {
    if (item->dataSource() == source) {
      beginResetModel();
      item->remove(source);
      m_treeItems.removeAll(item);
      endResetModel();

      return true;
    }
  }

  return false;
}

bool PipelineModel::removeModule(Module *module)
{
  moduleRemoved(module);
  ModuleManager::instance().removeModule(module);
  return true;

  foreach(auto item, m_treeItems) {
    if (item->dataSource() == module->dataSource()) {
      beginResetModel();
      item->remove(module);
      endResetModel();
      ModuleManager::instance().removeModule(module);
      return true;
    }
  }
  return false;
}

bool PipelineModel::removeOp(Operator *o)
{
  foreach(TreeItem *item, m_treeItems) {
    if (item->hasOp(o)) {
      // Should circle back to this, but removing an operator potentially moves
      // the children up a level, or to the final operator in the chain.
      beginResetModel();
      item->remove(o);
      endResetModel();
      o->dataSource()->removeOperator(o);
      return true;
    }
  }
  return false;
}

} // tomviz namespace
