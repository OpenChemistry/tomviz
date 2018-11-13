/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRegexGroupSubstitutionDialog_h
#define tomvizRegexGroupSubstitutionDialog_h

#include <QDialog>

#include <QLabel>
#include <QScopedPointer>

namespace Ui {
class RegexGroupSubstitutionDialog;
}

namespace tomviz {

class RegexGroupSubstitutionDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RegexGroupSubstitutionDialog(const QString groupName = "",
                                        const QString regex = "",
                                        const QString substitution = "",
                                        QWidget* parent = nullptr);
  ~RegexGroupSubstitutionDialog() override;

  QString groupName();
  QString regex();
  QString substitution();

public slots:
  void done(int r) override;

private:
  QScopedPointer<Ui::RegexGroupSubstitutionDialog> m_ui;
  QLabel m_regexErrorLabel;
};
} // namespace tomviz

#endif
