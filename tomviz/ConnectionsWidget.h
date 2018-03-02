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

#ifndef tomvizConnectionsWidget_h
#define tomvizConnectionsWidget_h

#include "Connection.h"

#include <QScopedPointer>
#include <QVariantList>
#include <QWidget>

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

private:
  QScopedPointer<Ui::ConnectionsWidget> m_ui;
  QList<Connection> m_connections;

  void readSettings();
  void writeSettings();
  void setConnections(const QVariantList& connections);
  void sortConnections();
  void editConnection(Connection conn);
};
}

#endif
