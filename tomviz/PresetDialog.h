/* This source file is part of the Tomviz project, https://tomviz.org/.
  It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPresetDialog_h
#define tomvizPresetDialog_h

#include <QDialog>
#include <QScopedPointer>

class QTableView;
class vtkSMProxy;

namespace Ui {
class PresetDialog;
}

namespace tomviz {

class PresetModel;
  
class PresetDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PresetDialog(QWidget* parent);
  QString presetName();
  void addNewPreset(const QJsonObject& newPreset);
  QJsonObject jsonObject();
  ~PresetDialog() override;

signals:
  void applyPreset();

private:
  QScopedPointer<Ui::PresetDialog> m_ui;
  PresetModel* m_model;
  QTableView* m_view;
  void customMenuRequested(const QModelIndex& Index);
};
} // namespace tomviz

#endif
