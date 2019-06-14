/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetModel.h"

namespace tomviz {

// ctor
PresetModel::PresetModel(QObject *parent)
  : QAbstractTableModel(parent)
{
}

// the number of rows the view should display
int PresetModel::rowCount(const QModelIndex &/*parent*/) const
{
  return 5;
}

// the number of columns the view should display
int PresetModel::columnCount(const QModelIndex &/*parent*/) const
{
  return 1;
}

// returns what value should be in the cell at a given index
QVariant PresetModel::data(const QModelIndex &index, int role) const
{
  switch (role) {
    // placeholder data
    case Qt::DisplayRole :
      return QString("Row%1 Column%2")
        .arg(index.row() + 1)
        .arg(index.column() + 1);
      // format the data
    case Qt::TextAlignmentRole:
      return Qt::AlignCenter + Qt::AlignVCenter;
  }
  
  return QVariant();
}

QVariant PresetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role ==Qt::DisplayRole && orientation == Qt::Horizontal) {
    return QString("Presets");
  }

  return QVariant();
}
} // namespace tomviz
