/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPresetModel_h
#define tomvizPresetModel_h

#include <QAbstractTableModel>
#include <QApplication>
#include <QList>

#include <vtkNew.h>
#include <vtkSMTransferFunctionPresets.h>

namespace tomviz {

class PresetModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  PresetModel(QObject* parent = nullptr);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  QString presetName();

signals:
  void applyPreset();

public slots:
  void changePreset(const QModelIndex&);
  void setName(const QModelIndex& index);

private:
  QList<QPair<QString, QPixmap>> m_Pixmaps;
  QString m_name;
  QString getMatplotlibColorMapFile();
};
} // namespace tomviz
#endif
