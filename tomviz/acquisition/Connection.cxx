/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "Connection.h"

#include <QDataStream>

namespace tomviz {

Connection::Connection() = default;

Connection::Connection(QString name, QString hostName, int port)
  : m_name(name), m_hostName(hostName), m_port(port)
{
}

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
}
