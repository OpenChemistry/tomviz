/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataBrokerLoadDialog.h"
#include "DataBroker.h"
#include "Utilities.h"

#include "ui_DataBrokerLoadDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QPushButton>
#include <QTreeWidget>

namespace tomviz {

DataBrokerLoadDialog::DataBrokerLoadDialog(DataBroker* dataBroker,
                                           QWidget* parent)
  : QDialog(parent), m_ui(new Ui::DataBrokerLoadDialog),
    m_dataBroker(dataBroker)
{
  m_ui->setupUi(this);
  auto tree = m_ui->treeWidget;
  tree->setExpandsOnDoubleClick(false);
  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

  auto resetButton = m_ui->buttonBox->button(QDialogButtonBox::Reset);
  connect(resetButton, &QPushButton::clicked, this, [this]() {
    setEnabledOkButton(false);
    clearErrorMessage();
    this->loadCatalogs();
  });

  setEnabledOkButton(false);

  loadCatalogs();
}

DataBrokerLoadDialog::~DataBrokerLoadDialog() = default;

void DataBrokerLoadDialog::loadCatalogs()
{
  beginCall();

  auto call = m_dataBroker->catalogs();
  connect(call, &ListResourceCall::complete, call,
          [this, call](QList<QVariantMap> catalogs) {
            this->m_catalogs = catalogs;
            this->showCatalogs();
            this->setLabel("Please select a catalog");
            this->endCall();
            call->deleteLater();
          });

  connectErrorSignal(call);
}

void DataBrokerLoadDialog::showCatalogs()
{
  auto tree = m_ui->treeWidget;
  tree->clear();
  disconnect(tree, &QTreeWidget::itemDoubleClicked, nullptr, nullptr);

  connect(tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedCatalog = item->data(0, Qt::DisplayRole).toString();
            this->loadRuns(m_selectedCatalog);
          });

  tree->setColumnCount(2);

  QStringList headers;
  headers.append("Name");
  headers.append("Description");
  tree->setHeaderLabels(headers);

  QList<QTreeWidgetItem*> items;
  for (auto& cat : m_catalogs) {
    QStringList row;
    row.append(cat["name"].toString());
    row.append(cat["descriptions"].toString());
    items.append(new QTreeWidgetItem(tree, row));
  }
  tree->insertTopLevelItems(0, items);
}

void DataBrokerLoadDialog::loadRuns(const QString& catalog)
{
  beginCall();

  auto call = m_dataBroker->runs(catalog);
  connect(call, &ListResourceCall::complete, call,
          [this, call](QList<QVariantMap> runs) {
            this->m_runs = runs;
            this->showRuns();
            this->setLabel("Please select a run");
            this->endCall();
            call->deleteLater();
          });

  connectErrorSignal(call);
}

void DataBrokerLoadDialog::showRuns()
{
  auto tree = m_ui->treeWidget;
  tree->clear();
  disconnect(tree, &QTreeWidget::itemDoubleClicked, nullptr, nullptr);

  connect(tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedRunUid = item->data(0, Qt::DisplayRole).toString();
            this->loadTables(m_selectedCatalog, m_selectedRunUid);
          });

  tree->setColumnCount(3);

  QStringList headers = { "UID", "Name", "Date" };
  tree->setHeaderLabels(headers);

  QList<QTreeWidgetItem*> items;
  for (auto run : m_runs) {
    auto seconds = run["time"].toDouble();
    auto mseconds = seconds * 1000;
    auto dateTime = QDateTime::fromMSecsSinceEpoch(mseconds);
    QStringList row = { run["uid"].toString(), run["name"].toString(),
                        dateTime.toString(Qt::TextDate) };

    items.append(new QTreeWidgetItem(tree, row));
  }
  tree->insertTopLevelItems(0, items);
}

void DataBrokerLoadDialog::loadTables(const QString& catalog,
                                      const QString& runUid)
{
  beginCall();

  auto call = m_dataBroker->tables(catalog, runUid);
  connect(call, &ListResourceCall::complete, call,
          [this, call](QList<QVariantMap> tables) {
            this->m_tables = tables;
            this->showTables();
            this->setLabel("Please select a table");
            this->endCall();
            call->deleteLater();
          });

  connectErrorSignal(call);
}

void DataBrokerLoadDialog::showTables()
{
  auto tree = m_ui->treeWidget;
  tree->clear();
  disconnect(tree, &QTreeWidget::itemDoubleClicked, nullptr, nullptr);

  connect(tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedTable = item->data(0, Qt::DisplayRole).toString();
            this->loadVariables(m_selectedCatalog, m_selectedRunUid,
                                m_selectedTable);
          });

  tree->setColumnCount(1);

  QStringList headers = { "Name", "Date" };
  tree->setHeaderLabels(headers);

  QList<QTreeWidgetItem*> items;
  for (auto& table : m_tables) {
    QStringList row = { table["name"].toString() };
    items.append(new QTreeWidgetItem(tree, row));
  }
  tree->insertTopLevelItems(0, items);
}

void DataBrokerLoadDialog::loadVariables(const QString& catalog,
                                         const QString& runUid,
                                         const QString& table)
{
  beginCall();

  auto call = m_dataBroker->variables(catalog, runUid, table);
  connect(call, &ListResourceCall::complete, call,
          [this, call](QList<QVariantMap> variables) {
            this->m_variables = variables;
            this->showVariables();
            this->setLabel("Please select a variable");
            this->endCall();
            call->deleteLater();
          });

  connectErrorSignal(call);
}

void DataBrokerLoadDialog::showVariables()
{
  auto tree = m_ui->treeWidget;
  tree->clear();
  disconnect(tree, &QTreeWidget::itemDoubleClicked, nullptr, nullptr);

  connect(tree, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedVariable = item->data(0, Qt::DisplayRole).toString();
            setEnabledOkButton(true);
          });

  connect(tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedVariable = item->data(0, Qt::DisplayRole).toString();
            this->accept();
          });

  tree->setColumnCount(2);

  QStringList headers = { "Name", "Size" };
  tree->setHeaderLabels(headers);

  QList<QTreeWidgetItem*> items;
  for (auto& v : m_variables) {
    QStringList row = { v["name"].toString(),
                        getSizeNearestThousand(v["size"].toLongLong()) };
    items.append(new QTreeWidgetItem(tree, row));
  }
  tree->insertTopLevelItems(0, items);
}

void DataBrokerLoadDialog::setLabel(const QString& label)
{
  this->m_ui->label->setText(label);
}

void DataBrokerLoadDialog::setEnabledResetButton(bool enabled)
{
  auto resetButton = m_ui->buttonBox->button(QDialogButtonBox::Reset);
  resetButton->setEnabled(enabled);
}

void DataBrokerLoadDialog::setEnabledOkButton(bool enabled)
{
  auto okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);
  okButton->setEnabled(enabled);
}

void DataBrokerLoadDialog::beginCall()
{
  setEnabledResetButton(false);
  setCursor(Qt::WaitCursor);
  m_ui->treeWidget->setEnabled(false);
  clearErrorMessage();
}

void DataBrokerLoadDialog::endCall()
{
  setEnabledResetButton(true);
  unsetCursor();
  m_ui->treeWidget->setEnabled(true);
}

void DataBrokerLoadDialog::setErrorMessage(const QString& errorMessage)
{
  m_ui->errorLabel->setText(
    QString("%1. See message log for details.").arg(errorMessage));
}

void DataBrokerLoadDialog::clearErrorMessage()
{
  m_ui->errorLabel->setText("");
}

void DataBrokerLoadDialog::connectErrorSignal(ListResourceCall* call)
{
  connect(call, &DataBrokerCall::error, call,
          [this, call](const QString& errorMessage) {
            this->setErrorMessage(errorMessage);
            this->endCall();
            call->deleteLater();
          });
}

QString DataBrokerLoadDialog::selectedCatalog()
{
  return m_selectedCatalog;
}

QString DataBrokerLoadDialog::selectedRunUid()
{
  return m_selectedRunUid;
}

QString DataBrokerLoadDialog::selectedTable()
{
  return m_selectedTable;
}

QString DataBrokerLoadDialog::selectedVariable()
{
  return m_selectedVariable;
}

} // namespace tomviz
