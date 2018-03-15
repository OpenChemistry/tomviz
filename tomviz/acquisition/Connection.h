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
  // Connection(const Connection& other);
  ~Connection();

  QString name() const { return m_name; };
  void setName(const QString& name) { m_name = name; };
  QString hostName() const { return m_hostName; };
  void setHostName(const QString& hostName) { m_hostName = hostName; };
  int port() const { return m_port; };
  void setPort(int port) { m_port = port; };

  static void registerType();

private:
  QString m_name;
  QString m_hostName;
  int m_port;
};
}

Q_DECLARE_METATYPE(tomviz::Connection)

#endif
