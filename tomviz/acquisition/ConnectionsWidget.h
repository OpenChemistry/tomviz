/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConnectionsWidget_h
#define tomvizConnectionsWidget_h

#include <QWidget>

#include "Connection.h"

#include <QScopedPointer>
#include <QVariantList>

namespace Ui {
class ConnectionsWidget;
}

namespace tomviz {

class ConnectionsWidget : public QWidget
{
  Q_OBJECT

public:
  ConnectionsWidget(QWidget* parent);
  ~ConnectionsWidget() override;

  Connection* selectedConnection();

signals:
  void selectionChanged();

private:
  QScopedPointer<Ui::ConnectionsWidget> m_ui;
  QList<Connection> m_connections;

  void readSettings();
  void writeSettings();
  void setConnections(const QVariantList& connections);
  void sortConnections();
  void editConnection(Connection conn, size_t row);
};
} // namespace tomviz

#endif
