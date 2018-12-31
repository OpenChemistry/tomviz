/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAboutDialog_h
#define tomvizAboutDialog_h

#include <QDialog>

#include <QJsonObject>
#include <QScopedPointer>

namespace Ui {
class AboutDialog;
}

namespace tomviz {

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(QWidget* parent);
  ~AboutDialog() override;

private:
  QScopedPointer<Ui::AboutDialog> m_ui;
  QJsonObject m_details;
};
} // namespace tomviz

#endif
