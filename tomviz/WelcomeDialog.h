/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizWelcomeDialog_h
#define tomvizWelcomeDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class WelcomeDialog;
}

namespace tomviz {
class MainWindow;

class WelcomeDialog : public QDialog
{
  Q_OBJECT

public:
  explicit WelcomeDialog(MainWindow* parent);
  ~WelcomeDialog() override;

private slots:
  // When user clicks yes to load sample data
  void onLoadSampleDataClicked();

  // React to checkbox events
  void onDoNotShowAgainStateChanged(int);

private:
  QScopedPointer<Ui::WelcomeDialog> m_ui;
};
} // namespace tomviz

#endif
