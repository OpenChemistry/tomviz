/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Connection.h"

#include <QDataStream>

namespace tomviz {

Connection::Connection() = default;

Connection::Connection(QString name, QString hostName, int port)
  : m_name(name), m_hostName(hostName), m_port(port)
{}

Connection::~Connection() = default;

void Connection::registerType()
{

  static bool registered = false;
  if (!registered) {
    registered = true;
    qRegisterMetaTypeStreamOperators<tomviz::Connection>("tomviz::Connection");
  }
}

QDataStream& operator<<(QDataStream& out, const Connection& conn)
{
  out << conn.name() << conn.hostName() << conn.port();

  return out;
}

QDataStream& operator>>(QDataStream& in, Connection& conn)
{
  QString name, hostName;
  int port;
  in >> name >> hostName >> port;
  conn.setName(name);
  conn.setHostName(hostName);
  conn.setPort(port);

  return in;
}
} // namespace tomviz
