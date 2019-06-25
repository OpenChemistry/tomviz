/* This source file is part of the Tomviz project, https://tomviz.org/.
  It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPresetDialog_h
#define tomvizPresetDialog_h

#include "PresetModel.h"

#include <QDialog>
#include <QScopedPointer>

#include <vtk_jsoncpp.h>

namespace Ui {
class PresetDialog;
}

namespace tomviz {

class PresetDialog : public QDialog
{
  Q_OBJECT

 public:
  explicit PresetDialog(QWidget* parent);
  QString getName();
  ~PresetDialog() override;
  
 signals:
  void applyPreset();

 private:
  QScopedPointer<Ui::PresetDialog> m_ui;
  PresetModel *m_model;
};
} // namespace tomviz                                                                                                                                                                                       

#endif
