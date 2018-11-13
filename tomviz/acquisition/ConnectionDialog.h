/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConnectionDialog_h
#define tomvizConnectionDialog_h

#include <QDialog>

#include <QScopedPointer>

namespace Ui {
class ConnectionDialog;
}

namespace tomviz {

class ConnectionDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ConnectionDialog(const QString name = "",
                            const QString hostName = "", int port = 8080,
                            QWidget* parent = nullptr);
  ~ConnectionDialog() override;

  QString name();
  QString hostName();
  int port();

private:
  QScopedPointer<Ui::ConnectionDialog> m_ui;
};
} // namespace tomviz

#endif
