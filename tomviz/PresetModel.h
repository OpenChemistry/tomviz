/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPresetModel_h
#define tomvizPresetModel_h

#include <QAbstractTableModel>
#include <QList>

#include <vtkNew.h>
#include <vtkSMTransferFunctionPresets.h>

namespace tomviz
{
  
class PresetModel : public QAbstractTableModel
{
  Q_OBJECT
  
 public:
  PresetModel(QObject *parent = nullptr);
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

 signals:
  void getPreset(const Json::Value& preset);
  
 public slots:
  void handleClick(const QModelIndex &index);

 private:
  vtkNew<vtkSMTransferFunctionPresets> Presets;
  QList<QPair<QString, QPixmap>> m_Pixmaps;
};
}// namespace tomviz
#endif
