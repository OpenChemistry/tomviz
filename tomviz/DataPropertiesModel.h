/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataPropertiesModel_h
#define tomvizDataPropertiesModel_h

#include <QAbstractTableModel>

#include "DataSource.h"

#include <QModelIndex>
#include <QString>

namespace tomviz {

struct ArrayInfo;

/// Adapter to visualize the scalars arrays information of a datasource in a
/// QTableView
class DataPropertiesModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  DataPropertiesModel(QObject* parent = nullptr);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

  Qt::ItemFlags flags(const QModelIndex& index) const;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole);

  QList<ArrayInfo> getArraysInfo() const;

public slots:
  void setArraysInfo(QList<ArrayInfo> arraysInfo);

signals:
  void activeScalarsChanged(QString name);
  void scalarsRenamed(QString oldNama, QString newName);

private:
  QList<ArrayInfo> m_arraysInfo;
  const int c_numCol = 4;
  const int c_activeCol = 0;
  const int c_nameCol = 1;
  const int c_rangeCol = 2;
  const int c_typeCol = 3;
};

/// Basic scalar array metadata container
struct ArrayInfo
{
  ArrayInfo(QString name, QString range, QString type, bool active = false);
  QString name;
  QString range;
  QString type;
  bool active;
};

} // namespace tomviz

#endif
