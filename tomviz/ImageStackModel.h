#ifndef tomvizImageStackModel_h
#define tomvizImageStackModel_h

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QModelIndex>
#include <QObject>
#include <QString>

class ImageInfo;

class ImageStackModel : public QAbstractTableModel
{
public:
  ImageStackModel(QObject* parent, QList<ImageInfo>* filesInfo);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

private:
  const QList<ImageInfo>* filesInfo;
};

class ImageInfo
{
private:
public:
  ImageInfo(QString filename_, int m_, int n_, bool consistent_);
  QFileInfo fileInfo;
  int m;
  int n;
  bool consistent;
};

#endif
