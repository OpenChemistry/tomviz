/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizProgressDialog_h
#define tomvizProgressDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {

class ProgressDialog;
}

namespace tomviz {

class ProgressDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ProgressDialog(const QString& title, const QString& msg,
                          QWidget* parent = nullptr);
  ~ProgressDialog() override;

private:
  QScopedPointer<Ui::ProgressDialog> m_ui;
};
} // namespace tomviz

#endif
