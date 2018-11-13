/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
  m_ui->setupUi(this);

  readSettings();

  // New
  connect(m_ui->newConnectionButton, &QPushButton::clicked, [this]() {
    ConnectionDialog dialog;
    auto r = dialog.exec();

    if (r != QDialog::Accepted) {
      return;
    }

    QString name = dialog.name();
    QString hostName = dialog.hostName();
    int port = dialog.port();

    if (hostName.trimmed().isEmpty()) {
      return;
    }

    if (name.trimmed().isEmpty()) {
      name = hostName.trimmed() + QString(":%1").arg(port);
    }

    Connection newConnection(name, hostName, port);

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
      sortConnections();
      m_ui->connectionsWidget->addItem(newConnection.name());
    }
    writeSettings();
  });

  // Edit
  connect(m_ui->connectionsWidget, &QListWidget::itemDoubleClicked,
          [this](QListWidgetItem* item) {
            auto row = this->m_ui->connectionsWidget->row(item);
            this->editConnection(this->m_connections[row], row);
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

  connect(m_ui->connectionsWidget, &QListWidget::itemSelectionChanged, this,
          &ConnectionsWidget::selectionChanged);

  connect(m_ui->connectionsWidget, &QListWidget::itemSelectionChanged,
          [this]() { this->writeSettings(); });
}

ConnectionsWidget::~ConnectionsWidget() = default;

void ConnectionsWidget::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  if (!settings->contains("acquisition/connections")) {
    // Add a default localhost connection
    Connection local("localhost", "localhost", 8080);
    m_connections.append(local);
    m_ui->connectionsWidget->addItem(local.name());
    m_ui->connectionsWidget->setCurrentRow(0);
    return;
  }
  settings->beginGroup("acquisition");
  auto connections = settings->value("connections").toList();
  foreach (const QVariant v, connections) {
    auto conn = v.value<Connection>();
    m_connections.append(conn);
    m_ui->connectionsWidget->addItem(conn.name());
  }

  auto selectedConnection = settings->value("selectedConnections").toInt();
  m_ui->connectionsWidget->setCurrentRow(selectedConnection);

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
  settings->setValue("selectedConnections",
                     m_ui->connectionsWidget->currentRow());
  settings->endGroup();
}

void ConnectionsWidget::sortConnections()
{
  std::sort(m_connections.begin(), m_connections.end(),
            [](Connection a, Connection b) { return a.name() < b.name(); });
}

void ConnectionsWidget::editConnection(Connection conn, size_t row)
{
  ConnectionDialog dialog(conn.name(), conn.hostName(), conn.port());
  auto r = dialog.exec();

  if (r != QDialog::Accepted) {
    return;
  }

  QString name = dialog.name();
  QString hostName = dialog.hostName();
  int port = dialog.port();

  if (hostName.trimmed().isEmpty()) {
    return;
  }

  if (name.trimmed().isEmpty()) {
    name = hostName.trimmed() + QString(":%1").arg(port);
  }

  Connection newConnection(name, hostName, port);
  // First delete the current connection, to prevent always adding an extra one
  // when editing
  this->m_connections.removeAt(row);
  auto item = this->m_ui->connectionsWidget->takeItem(row);
  delete item;

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

Connection* ConnectionsWidget::selectedConnection()
{
  auto selectedRow =
    m_ui->connectionsWidget->row(m_ui->connectionsWidget->currentItem());

  if (selectedRow == -1) {
    return nullptr;
  }

  return &m_connections[selectedRow];
}
} // namespace tomviz
