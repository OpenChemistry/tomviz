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

#include "ImageStackModel.h"

#include <QBrush>

#include <iostream>

namespace tomviz {

ImageStackModel::ImageStackModel(QObject* parent)
  : QAbstractTableModel(parent)
{
}

int ImageStackModel::rowCount(const QModelIndex&) const
{
  return m_filesInfo.size();
}

int ImageStackModel::columnCount(const QModelIndex&) const
{
  return NUM_COL;
}

QVariant ImageStackModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid()){
    return QVariant();
  }

  int row = index.row();
  int col = index.column();

  if (row >= m_filesInfo.size()) {
    return QVariant();
  }

  if (role == Qt::DisplayRole){
    if (col == FILE_COL) {
      return m_filesInfo[row].fileInfo.fileName();
    } else if (col == X_COL) {
      return QString("%1").arg(m_filesInfo[row].m);
    } else if (col == Y_COL) {
      return QString("%1").arg(m_filesInfo[row].n);
    }
  } else if (role == Qt::ToolTipRole) {
    if (col == FILE_COL) {
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
    if (m_filesInfo[row].consistent) {
      return successBackground;
    } else {
      return failBackground;
    }
  } else if (role == Qt::CheckStateRole) {
    if (col == CHECK_COL) {
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
      if (section == FILE_COL) {
        return QString("Filename");
      } else if (section == X_COL) {
        return QString("X");
      } else if (section == Y_COL) {
        return QString("Y");
      }
    } else if (orientation == Qt::Horizontal) {
      return QString("%1").arg(section + 1);
    }
  }
  return QVariant();
}

Qt::ItemFlags ImageStackModel::flags(const QModelIndex &index) const
{
  if (index.column() == CHECK_COL) {
    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  } else {
    return Qt::ItemIsEnabled;
  }
}

bool ImageStackModel::setData(const QModelIndex &index,
                              const QVariant &value, int role)
{
  if (index.isValid() && role == Qt::CheckStateRole) {
    int col = index.column();
    int row = index.row();
    if (m_filesInfo[row].consistent && col == CHECK_COL) {
      // m_filesInfo[row].selected = value.toBool();
      // emit dataChanged(index, index);
      emit toggledSelected(row, value.toBool());
      return true;
    }
  }
  return false;
}

QList<ImageInfo> ImageStackModel::getFileInfo() const
{
  return (m_filesInfo);
}

void ImageStackModel::onFilesInfoChanged(QList<ImageInfo> filesInfo)
{
  beginResetModel();
  std::cout << "Summary Changed: MODEL" << std::endl;
  m_filesInfo = filesInfo;
  endResetModel();
  // emit modelReset();
  // emit dataChanged();
}

ImageInfo::ImageInfo(QString fileName, int pos_, int m_, int n_, bool consistent_)
  : fileInfo(QFileInfo(fileName)), pos(pos_),  m(m_), n(n_), consistent(consistent_)
  , selected(consistent_)
{
}

} // namespace tomviz
