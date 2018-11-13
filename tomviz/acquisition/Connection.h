/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConnection_h
#define tomvizConnection_h

#include <QMetaType>
#include <QString>

namespace tomviz {

class Connection
{
public:
  Connection();
  Connection(QString name, QString hostName, int port);
  ~Connection();

  QString name() const { return m_name; }
  void setName(const QString& name) { m_name = name; }
  QString hostName() const { return m_hostName; }
  void setHostName(const QString& hostName) { m_hostName = hostName; }
  int port() const { return m_port; }
  void setPort(int port) { m_port = port; }

  static void registerType();

private:
  QString m_name;
  QString m_hostName;
  int m_port;
};
} // namespace tomviz

Q_DECLARE_METATYPE(tomviz::Connection)

#endif
