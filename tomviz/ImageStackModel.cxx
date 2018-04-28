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

namespace tomviz {

ImageStackModel::ImageStackModel(QObject* parent,
                                 const QList<ImageInfo>& filesInfo)
  : QAbstractTableModel(parent), m_filesInfo(filesInfo)
{
}

int ImageStackModel::rowCount(const QModelIndex&) const
{
  return m_filesInfo.size();
}

int ImageStackModel::columnCount(const QModelIndex&) const
{
  return 3;
}

QVariant ImageStackModel::data(const QModelIndex& index, int role) const
{
  int row = index.row();
  int col = index.column();

  switch (role) {
    case Qt::DisplayRole: {

      switch (col) {
        case 0:
          return m_filesInfo[row].fileInfo.fileName();
        case 1:
          return QString("%1").arg(m_filesInfo[row].m);
        case 2:
          return QString("%1").arg(m_filesInfo[row].n);
      }
    }

    case Qt::ToolTipRole: {
      if (col == 0) {
        return m_filesInfo[row].fileInfo.absoluteFilePath();
      }
      break;
    }

    case Qt::BackgroundRole: {
      QColor failColor = Qt::red;
      failColor.setAlphaF(0.25);
      QBrush failBackground(failColor);
      QColor successColor = Qt::green;
      successColor.setAlphaF(0.125);
      QBrush successBackground(successColor);
      if (m_filesInfo[row].consistent) {
        return successBackground;
      } else {
        return failBackground;
      }
    }
  }
  return QVariant();
}

QVariant ImageStackModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch (section) {
        case 0:
          return QString("Filename");
        case 1:
          return QString("X");
        case 2:
          return QString("Y");
      }
    } else if (orientation == Qt::Horizontal) {
      return QString("%1").arg(section + 1);
    }
  }
  return QVariant();
}

ImageInfo::ImageInfo(QString fileName, int m_, int n_, bool consistent_)
  : fileInfo(QFileInfo(fileName)), m(m_), n(n_), consistent(consistent_)
{
}

}
