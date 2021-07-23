/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConformVolumeDialog_h
#define tomvizConformVolumeDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class ConformVolumeDialog;
}

namespace tomviz {

class DataSource;

class ConformVolumeDialog : public QDialog
{
  Q_OBJECT

public:
  ConformVolumeDialog(QWidget* parent = nullptr);
  ~ConformVolumeDialog() override;

  void setupConnections();

  void setVolumes(QList<DataSource*> volumes);
  DataSource* selectedVolume();

private:
  void updateConformToLabel();

  QList<DataSource*> m_volumes;

  Q_DISABLE_COPY(ConformVolumeDialog)
  QScopedPointer<Ui::ConformVolumeDialog> m_ui;
};
} // namespace tomviz

#endif
