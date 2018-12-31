/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizStartServerDialog_h
#define tomvizStartServerDialog_h

#include <QDialog>

#include <QScopedPointer>

namespace Ui {
class StartServerDialog;
}

namespace tomviz {

class StartServerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit StartServerDialog(QWidget* parent = nullptr);
  ~StartServerDialog() override;
  QString pythonExecutablePath() { return m_pythonExecutablePath; }

private:
  QScopedPointer<Ui::StartServerDialog> m_ui;
  QString m_pythonExecutablePath;

  void readSettings();
  void writeSettings();
  void setPythonExecutablePath(const QString& path);
};
} // namespace tomviz

#endif
