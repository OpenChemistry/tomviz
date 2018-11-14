/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataPropertiesModel.h"

#include <QBrush>
#include <QDebug>

namespace tomviz {

DataPropertiesModel::DataPropertiesModel(QObject* parent)
  : QAbstractTableModel(parent)
{
}

int DataPropertiesModel::rowCount(const QModelIndex&) const
{
  return m_arraysInfo.size();
}

int DataPropertiesModel::columnCount(const QModelIndex&) const
{
  return c_numCol;
}

QVariant DataPropertiesModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return QVariant();
  }

  int row = index.row();
  int col = index.column();

  if (row >= m_arraysInfo.size()) {
    return QVariant();
  }

  if (role == Qt::DisplayRole) {
    if (col == c_nameCol) {
      return m_arraysInfo[row].name;
    } else if (col == c_rangeCol) {
      return m_arraysInfo[row].range;
    } else if (col == c_typeCol) {
      return m_arraysInfo[row].type;
    }
  } else if (role == Qt::CheckStateRole) {
    if (col == c_activeCol) {
      return m_arraysInfo[row].active ? Qt::Checked : Qt::Unchecked;
    }
  }
  return QVariant();
}

QVariant DataPropertiesModel::headerData(int section,
                                         Qt::Orientation orientation,
                                         int role) const
{
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      if (section == c_nameCol) {
        return QString("Name");
      } else if (section == c_rangeCol) {
        return QString("Data Range");
      } else if (section == c_typeCol) {
        return QString("Data Type");
      }
    }
  }
  return QVariant();
}

Qt::ItemFlags DataPropertiesModel::flags(const QModelIndex& index) const
{
  if (index.column() == c_nameCol) {
    return Qt::ItemIsEditable | Qt::ItemIsEnabled;
  } else if (index.column() == c_activeCol) {
    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  } else {
    return Qt::ItemIsEnabled;
  }
}

bool DataPropertiesModel::setData(const QModelIndex& index,
                                  const QVariant& value, int role)
{
  if (index.isValid()) {
    int col = index.column();
    int row = index.row();
    if (col == c_nameCol && role == Qt::EditRole) {
      emit scalarsRenamed(m_arraysInfo[row].name, value.toString());
    } else if (col == c_activeCol && role == Qt::CheckStateRole) {
      emit activeScalarsChanged(m_arraysInfo[row].name);
    }
  }
  return false;
}

QList<ArrayInfo> DataPropertiesModel::getArraysInfo() const
{
  return m_arraysInfo;
}

void DataPropertiesModel::setArraysInfo(QList<ArrayInfo> arraysInfo)
{
  beginResetModel();
  m_arraysInfo = arraysInfo;
  endResetModel();
}

ArrayInfo::ArrayInfo(QString name_, QString range_, QString type_, bool active_)
  : name(name_), range(range_), type(type_), active(active_)
{
}

} // namespace tomviz
