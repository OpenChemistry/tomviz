/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRegexGroupDialog_h
#define tomvizRegexGroupDialog_h

#include <QDialog>

#include <QScopedPointer>

namespace Ui {
class RegexGroupDialog;
}

namespace tomviz {

class RegexGroupDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RegexGroupDialog(const QString name = "", QWidget* parent = nullptr);
  ~RegexGroupDialog() override;

  QString name();

private:
  QScopedPointer<Ui::RegexGroupDialog> m_ui;
};
} // namespace tomviz

#endif
