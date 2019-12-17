/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineSettingsDialog_h
#define tomvizPipelineSettingsDialog_h

#include <QDialog>
#include <QMetaEnum>
#include <QScopedPointer>

#include "Pipeline.h"

namespace Ui {
class PipelineSettingsDialog;
}

namespace tomviz {

class PipelineSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  PipelineSettingsDialog(QWidget* parent = nullptr);
  ~PipelineSettingsDialog() override;
  void readSettings();

private slots:
  void writeSettings();

protected:
  void showEvent(QShowEvent* event) override;

private:
  Q_DISABLE_COPY(PipelineSettingsDialog)
  QScopedPointer<Ui::PipelineSettingsDialog> m_ui;
  QMetaEnum m_executorTypeMetaEnum;
  void checkEnableOk();
};
} // namespace tomviz

#endif
