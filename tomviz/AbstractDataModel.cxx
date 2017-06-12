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
#include <iostream>

#include <QDebug>
#include <QTreeWidgetItem>

#include "AbstractDataModel.h"


// -----------------------------------------------------------------------------
AbstractDataModel::AbstractDataModel(QObject* parent_)
  : QAbstractItemModel(parent_)
{
}

// -----------------------------------------------------------------------------
AbstractDataModel::~AbstractDataModel()
{
  qDeleteAll(RootItem->takeChildren());
  delete RootItem;
}

// -----------------------------------------------------------------------------
int AbstractDataModel::rowCount(const QModelIndex& parent_) const
{
  QTreeWidgetItem* parentItem = this->getItem(parent_);
  return parentItem->childCount();
}

// -----------------------------------------------------------------------------
int AbstractDataModel::columnCount(const QModelIndex& parent_) const
{
  QTreeWidgetItem* parentItem = this->getItem(parent_);
  return parentItem->columnCount();
}

// -----------------------------------------------------------------------------
QModelIndex AbstractDataModel::index(int row, int column, const QModelIndex& parent_) const
{
  if (!hasIndex(row, column, parent_))
  {
    return QModelIndex();
  }

  QTreeWidgetItem* parentItem = this->getItem(parent_);
  QTreeWidgetItem* childItem = parentItem->child(row);

  return (childItem ? this->createIndex(row, column, childItem) : QModelIndex());
}

// -----------------------------------------------------------------------------
QModelIndex AbstractDataModel::parent(const QModelIndex& index_) const
{
  if (!index_.isValid())
  {
    return QModelIndex();
  }

  QTreeWidgetItem* childItem = this->getItem(index_);
  QTreeWidgetItem* parentItem = childItem->parent();

  if (parentItem == RootItem)
  {
    return QModelIndex();
  }

  if (parentItem == nullptr)
  {
    return QModelIndex();
  }

  QTreeWidgetItem* grandParentItem = parentItem->parent();
  const int row = grandParentItem->indexOfChild(parentItem);

  return QAbstractItemModel::createIndex(row, 0, parentItem);
}

// -----------------------------------------------------------------------------
QVariant AbstractDataModel::data(const QModelIndex& index_, int role) const
{
  if (!this->isIndexValidUpperBound(index_))
  {
    return QVariant();
  }

  QModelIndex parentIndex = index_.parent();
  QTreeWidgetItem* parent_ = this->getItem(parentIndex);

  QTreeWidgetItem* item = parent_->child(index_.row());
  if (role == Qt::DisplayRole)
  {
    return item->data(index_.column(), role);
  }
  else
  {
    return QVariant();
  }
}

// -----------------------------------------------------------------------------
bool AbstractDataModel::setData(const QModelIndex& index_, const QVariant& value, int role)
{
  if (!this->isIndexValidUpperBound(index_))
  {
    return false;
  }

  QTreeWidgetItem* item = this->getItem(index_);
  if (role == Qt::DisplayRole)
  {
    item->setData(index_.column(), Qt::DisplayRole, value);
    QAbstractItemModel::dataChanged(index_, index_);
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
QVariant AbstractDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    return RootItem->data(section, role);
  }

  return QVariant();
}

// -----------------------------------------------------------------------------
Qt::ItemFlags AbstractDataModel::flags(const QModelIndex& index_) const
{
  if (index_.isValid() && index_.column() == 0)
  {
    return (Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  }

  return QAbstractItemModel::flags(index_);
}

// -----------------------------------------------------------------------------
bool AbstractDataModel::isIndexValidUpperBound(const QModelIndex& index_) const
{
  if (!index_.isValid())
  {
    return false;
  }

  QModelIndex const& parent_ = index_.parent();
  if (index_.row() >= this->rowCount(parent_) || index_.column() >= this->columnCount(parent_))
  {
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
QTreeWidgetItem* AbstractDataModel::getItem(const QModelIndex& index) const
{
  if (index.isValid())
  {
    QTreeWidgetItem* item = static_cast<QTreeWidgetItem*>(index.internalPointer());
    if (item)
    {
      return item;
    }
  }

  return this->RootItem;
}

// -----------------------------------------------------------------------------
const QModelIndex AbstractDataModel::getDefaultIndex()
{
  return this->index(0, 0, QModelIndex());
}

// -----------------------------------------------------------------------------
bool AbstractDataModel::removeRows(int row, int count, const QModelIndex& parent)
{
  QTreeWidgetItem* parentItem = this->getItem(parent);
  if (row < 0 || row + count > parentItem->childCount())
  {
    return false;
  }

  QAbstractItemModel::beginRemoveRows(parent, row, row + count - 1);

  for (int i = 0; i < count; i++)
  {
    // Shortens the list on every iteration
    delete parentItem->takeChild(row);
  }

  QAbstractItemModel::endRemoveRows();
  return true;
}
