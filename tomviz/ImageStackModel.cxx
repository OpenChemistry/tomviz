#include "ImageStackModel.h"

#include <QBrush>
#include <QFileInfo>
#include <QModelIndex>
#include <QObject>
#include <QString>

ImageStackModel::ImageStackModel(QObject* parent, QList<ImageInfo>* filesInfo_)
  : QAbstractTableModel(parent), filesInfo(filesInfo_)
{
}

int ImageStackModel::rowCount(const QModelIndex& parent) const
{
  return filesInfo->size();
}

int ImageStackModel::columnCount(const QModelIndex& parent) const
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
          return (*filesInfo)[row].fileInfo.fileName();

        case 1:
          return QString("%1").arg((*filesInfo)[row].m);

        case 2:
          return QString("%1").arg((*filesInfo)[row].n);
      }
    }

    case Qt::ToolTipRole: {

      switch (col) {
        case 0:
          return (*filesInfo)[row].fileInfo.absoluteFilePath();
      }
    }

    case Qt::BackgroundRole: {
      QColor failColor = Qt::red;
      failColor.setAlphaF(0.25);
      QBrush failBackground(failColor);
      QColor successColor = Qt::green;
      successColor.setAlphaF(0.125);
      QBrush successBackground(successColor);
      if ((*filesInfo)[row].consistent) {
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
  switch (role) {
    case Qt::DisplayRole:

      switch (orientation) {
        case Qt::Horizontal:

          switch (section) {
            case 0:
              return QString("Filename");

            case 1:
              return QString("X");

            case 2:
              return QString("Y");
          }

        case Qt::Vertical:
          return QString("%1").arg(section + 1);
      }
  }
  return QVariant();
}

ImageInfo::ImageInfo(QString filename_, int m_, int n_, bool consistent_)
  : fileInfo(QFileInfo(filename_)), m(m_), n(n_), consistent(consistent_)
{
}
