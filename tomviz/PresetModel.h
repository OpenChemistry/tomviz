/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPresetModel_h
#define tomvizPresetModel_h

#include <QAbstractTableModel>

#include <QJsonArray>

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
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  QString presetName();
  void addNewPreset(const QJsonObject& newPreset);
  QJsonObject jsonObject();
  void deletePreset(const QModelIndex& index);

signals:
  void applyPreset();

public slots:
  void changePreset(const QModelIndex&);
  void setRow(const QModelIndex& index);
  void resetToDefaults();

private:
  QJsonArray m_presets;
  int m_row = 2;

  void loadFromFile();
  QPixmap render(const QJsonObject& newPreset) const;
  void updateRow();
  void saveSettings();
  void modelChanged();
};

} // namespace tomviz
#endif
