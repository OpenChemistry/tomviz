/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ImageStackModel.h"

#include <QBrush>

namespace tomviz {

ImageStackModel::ImageStackModel(QObject* parent) : QAbstractTableModel(parent)
{
}

int ImageStackModel::rowCount(const QModelIndex&) const
{
  return m_filesInfo.size();
}

int ImageStackModel::columnCount(const QModelIndex&) const
{
  return c_numCol;
}

QVariant ImageStackModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()) {
    return QVariant();
  }

  int row = index.row();
  int col = index.column();

  if (row >= m_filesInfo.size()) {
    return QVariant();
  }

  if (role == Qt::DisplayRole) {
    if (col == c_fileCol) {
      return m_filesInfo[row].fileInfo.fileName();
    } else if (col == c_xCol) {
      return QString("%1").arg(m_filesInfo[row].m);
    } else if (col == c_yCol) {
      return QString("%1").arg(m_filesInfo[row].n);
    } else if (col == c_posCol) {
      return QString("%1").arg(m_filesInfo[row].pos);
    }
  } else if (role == Qt::ToolTipRole) {
    if (col == c_fileCol) {
      return m_filesInfo[row].fileInfo.absoluteFilePath();
    }
  } else if (role == Qt::BackgroundRole) {
    QColor failColor = Qt::red;
    failColor.setAlphaF(0.25);
    QBrush failBackground(failColor);
    QColor successColor = Qt::green;
    if (m_filesInfo[row].selected) {
      successColor.setAlphaF(0.125);
    } else {
      successColor.setAlphaF(0.0625);
    }
    QBrush successBackground(successColor);
    if (m_filesInfo[row].m == -1 || m_filesInfo[row].n == -1) {
      return QVariant();
    } else if (m_filesInfo[row].consistent) {
      return successBackground;
    } else {
      return failBackground;
    }
  } else if (role == Qt::CheckStateRole) {
    if (col == c_checkCol) {
      return m_filesInfo[row].selected ? Qt::Checked : Qt::Unchecked;
    }
  }
  return QVariant();
}

QVariant ImageStackModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      if (section == c_fileCol) {
        return QString("Filename");
      } else if (section == c_xCol) {
        return QString("X");
      } else if (section == c_yCol) {
        return QString("Y");
      } else if (section == c_posCol) {
        if (m_stackType == DataSource::DataSourceType::Volume) {
          return QString("Slice");
        } else if (m_stackType == DataSource::DataSourceType::TiltSeries) {
          return QString("Angle");
        }
      }
    } else if (orientation == Qt::Horizontal) {
      return QString("%1").arg(section + 1);
    }
  }
  return QVariant();
}

Qt::ItemFlags ImageStackModel::flags(const QModelIndex& index) const
{
  if (index.column() == c_checkCol) {
    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  } else {
    return Qt::ItemIsEnabled;
  }
}

bool ImageStackModel::setData(const QModelIndex& index, const QVariant& value,
                              int role)
{
  if (index.isValid() && role == Qt::CheckStateRole) {
    int col = index.column();
    int row = index.row();
    if (m_filesInfo[row].consistent && col == c_checkCol) {
      emit toggledSelected(row, value.toBool());
      return true;
    }
  }
  return false;
}

QList<ImageInfo> ImageStackModel::getFileInfo() const
{
  return m_filesInfo;
}

void ImageStackModel::onFilesInfoChanged(QList<ImageInfo> filesInfo)
{
  beginResetModel();
  m_filesInfo = filesInfo;
  endResetModel();
}

void ImageStackModel::onStackTypeChanged(DataSource::DataSourceType stackType)
{
  beginResetModel();
  m_stackType = stackType;
  endResetModel();
}

ImageInfo::ImageInfo(QString fileName, int pos_, int m_, int n_,
                     bool consistent_)
  : fileInfo(QFileInfo(fileName)), pos(pos_), m(m_), n(n_),
    consistent(consistent_), selected(consistent_)
{
}

} // namespace tomviz
