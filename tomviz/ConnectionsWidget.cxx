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

#include "ConnectionsWidget.h"
#include "ConnectionDialog.h"
#include "ui_ConnectionsWidget.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QListWidget>
#include <QMenu>

#include <algorithm>

namespace tomviz {

ConnectionsWidget::ConnectionsWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::ConnectionsWidget)
{
  this->m_ui->setupUi(this);

  this->readSettings();

  foreach (const Connection& conn, m_connections) {
    m_ui->connectionsWidget->addItem(conn.name());
  }

  // New
  connect(m_ui->newConnectionButton, &QPushButton::clicked, [this]() {
    ConnectionDialog dialog;
    dialog.exec();
    Connection newConnection(dialog.name(), dialog.hostName(), dialog.port());

    bool replaced = false;
    for (int i = 0; i < m_connections.size(); i++) {
      if (m_connections[i].name() == newConnection.name()) {
        m_connections[i] = newConnection;
        replaced = true;
        break;
      }
    }

    if (!replaced) {
      m_connections.append(newConnection);
      this->sortConnections();
      m_ui->connectionsWidget->addItem(newConnection.name());
    }
    this->writeSettings();
  });

  // Edit
  connect(m_ui->connectionsWidget, &QListWidget::itemDoubleClicked,
          [this](QListWidgetItem* item) {
            auto row = this->m_ui->connectionsWidget->row(item);
            this->editConnection(this->m_connections[row]);
            this->writeSettings();
          });

  // Delete
  connect(m_ui->connectionsWidget, &QWidget::customContextMenuRequested,
          [this](const QPoint& pos) {
            auto globalPos = this->m_ui->connectionsWidget->mapToGlobal(pos);
            QMenu contextMenu;
            contextMenu.addAction("Delete", this, [this, &pos]() {
              auto item = this->m_ui->connectionsWidget->itemAt(pos);
              auto row = this->m_ui->connectionsWidget->row(item);
              delete item;
              this->m_connections.removeAt(row);
              this->writeSettings();
            });

            // Show context menu at handling position
            contextMenu.exec(globalPos);
          });
}

ConnectionsWidget::~ConnectionsWidget() = default;

void ConnectionsWidget::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  if (!settings->contains("acquisition/connections")) {
    // Add a default localhost connection
    Connection local("localhost", "localhost", 8080);
    m_connections.append(local);
    return;
  }
  settings->beginGroup("acquisition");
  auto connections = settings->value("connections").toList();
  foreach (const QVariant conn, connections) {
    m_connections.append(conn.value<Connection>());
  }
  settings->endGroup();
}

void ConnectionsWidget::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");

  QVariantList connections;
  foreach (const Connection& conn, m_connections) {
    QVariant variant;
    variant.setValue(conn);
    connections.append(variant);
  }
  settings->setValue("connections", connections);
  settings->endGroup();
}

void ConnectionsWidget::sortConnections()
{
  std::sort(m_connections.begin(), m_connections.end(),
            [](Connection a, Connection b) { return a.name() < b.name(); });
}

void ConnectionsWidget::editConnection(Connection conn)
{
  ConnectionDialog dialog(conn.name(), conn.hostName(), conn.port());
  dialog.exec();
  Connection newConnection(dialog.name(), dialog.hostName(), dialog.port());

  bool replaced = false;
  for (int i = 0; i < m_connections.size(); i++) {
    if (m_connections[i].name() == newConnection.name()) {
      m_connections[i] = newConnection;
      replaced = true;
    }
  }

  if (!replaced) {
    m_connections.append(newConnection);
    m_ui->connectionsWidget->addItem(newConnection.name());
  }
}
}
