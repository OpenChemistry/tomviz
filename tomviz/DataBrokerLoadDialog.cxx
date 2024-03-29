/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataBrokerLoadDialog.h"
#include "DataBroker.h"
#include "Utilities.h"

#include "ui_DataBrokerLoadDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QTreeWidget>

#include <pqSettings.h>

namespace tomviz {

const char* DATABROKER_GROUP = "DataBroker";
const char* FILTER_FROM_SETTINGS_LABEL = "FilterFromDate";
const char* FILTER_TO_SETTINGS_LABEL = "FilterToDate";
const char* LIMIT_SETTINGS_LABEL = "Limit";

DataBrokerLoadDialog::DataBrokerLoadDialog(DataBroker* dataBroker,
                                           QWidget* parent)
  : QDialog(parent), m_ui(new Ui::DataBrokerLoadDialog),
    m_dataBroker(dataBroker)
{
  m_ui->setupUi(this);
  allowFilter(false);
  enableFilterByDate(false);
  enableFilterByID(false);
  m_ui->fromDateEdit->setCalendarPopup(true);
  m_ui->toDateEdit->setCalendarPopup(true);
  m_ui->fromDateEdit->setDate(QDate::currentDate().addDays(-365));
  m_ui->toDateEdit->setDate(QDate::currentDate().addDays(1));

  m_ui->idLineEdit->setValidator(new QIntValidator(0, 10000000, this));

  auto tree = m_ui->treeWidget;
  tree->setExpandsOnDoubleClick(false);
  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

  auto resetButton = m_ui->buttonBox->button(QDialogButtonBox::Reset);
  resetButton->setVisible(false);
  connect(resetButton, &QPushButton::clicked, this, [this]() {
    setEnabledOkButton(false);
    clearErrorMessage();
    this->loadCatalogs();
  });

  connect(m_ui->filterByDateCheckBox, &QPushButton::toggled, this,
          [this](bool enable) {
            this->enableFilterByDate(enable);
            this->applyFilter();
          });

  connect(m_ui->filterByIDCheckBox, &QPushButton::toggled, this,
          [this](bool enable) {
            this->enableFilterByID(enable);
            this->applyFilter();
          });

  connect(m_ui->fromDateEdit, &QDateEdit::dateChanged, this,
          [this](QDate date) { m_fromDate = date; });

  connect(m_ui->toDateEdit, &QDateEdit::dateChanged, this,
          [this](QDate date) { m_toDate = date; });

  connect(m_ui->applyFilterButton, &QPushButton::clicked, this,
          &DataBrokerLoadDialog::applyFilter);

  setEnabledOkButton(false);

  // Load settings
  QSettings* settings = pqApplicationCore::instance()->settings();
  settings->beginGroup(DATABROKER_GROUP);
  if (settings->contains(FILTER_FROM_SETTINGS_LABEL)) {
    m_fromDate = settings->value(FILTER_FROM_SETTINGS_LABEL).toDate();
    m_ui->fromDateEdit->setDate(m_fromDate);
  }

  if (settings->contains(FILTER_TO_SETTINGS_LABEL)) {
    m_toDate = settings->value(FILTER_TO_SETTINGS_LABEL).toDate();
    m_ui->toDateEdit->setDate(m_toDate);
  }

  if (settings->contains(LIMIT_SETTINGS_LABEL)) {
    m_limit = settings->value(LIMIT_SETTINGS_LABEL).toInt();
    m_ui->limitSpinBox->setValue(m_limit);
  }
  settings->endGroup();

  // See we have a catalog override in the environment
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  if (env.contains("TILED_CATALOG")) {
    m_selectedCatalog = env.value("TILED_CATALOG");
  }

  loadRuns(m_selectedCatalog, m_id, m_dateFilter, m_fromDate, m_toDate,
           m_limit);
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

  allowFilter(false);

  connect(tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedCatalog = item->data(0, Qt::DisplayRole).toString();
            this->loadRuns(m_selectedCatalog, m_id, m_dateFilter, m_fromDate,
                           m_toDate, m_limit);
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

void DataBrokerLoadDialog::loadRuns(const QString& catalog, int id,
                                    bool dateFilter, const QDate& fromDate,
                                    const QDate& toDate, int limit)
{
  beginCall();

  auto call =
    dateFilter
      ? m_dataBroker->runs(catalog, id, fromDate.toString(Qt::ISODate),
                           toDate.toString(Qt::ISODate), limit)
      : m_dataBroker->runs(catalog, id, QString(""), QString(""), limit);
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
  disconnect(tree, &QTreeWidget::itemClicked, nullptr, nullptr);

  allowFilter(true);

  connect(tree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedRunUid = item->data(0, Qt::DisplayRole).toString();
            this->accept();
          });

  connect(tree, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            m_selectedRunUid = item->data(0, Qt::DisplayRole).toString();
            setEnabledOkButton(true);
          });

  tree->setColumnCount(3);

  QStringList headers = { "UID", "Plan Name", "Scan Id", "Start", "Stop" };
  tree->setHeaderLabels(headers);

  QList<QTreeWidgetItem*> items;
  for (auto run : m_runs) {
    QDateTime startTime;
    QDateTime stopTime;
    {
      auto seconds = run["startTime"].toDouble();
      auto mseconds = seconds * 1000;
      startTime = QDateTime::fromMSecsSinceEpoch(mseconds);
    }

    if (run.contains("stopTime")) {
      auto seconds = run["stopTime"].toDouble();
      auto mseconds = seconds * 1000;
      stopTime = QDateTime::fromMSecsSinceEpoch(mseconds);
    }
    QStringList row = { run["uid"].toString().mid(0, 8),
                        run["planName"].toString(), run["scanId"].toString(),
                        startTime.toString(Qt::TextDate),
                        stopTime.toString(Qt::TextDate) };

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

  allowFilter(false);

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

  allowFilter(false);

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

void DataBrokerLoadDialog::allowFilter(bool allow)
{
  m_ui->filterWidget->setVisible(allow);
}

void DataBrokerLoadDialog::enableFilterByDate(bool enable)
{
  m_dateFilter = enable;
  m_ui->fromDateEdit->setEnabled(enable);
  m_ui->toDateEdit->setEnabled(enable);
}

void DataBrokerLoadDialog::enableFilterByID(bool enable)
{
  m_idFilter = enable;
  m_ui->idLineEdit->setEnabled(enable);
}

void DataBrokerLoadDialog::applyFilter()
{

  m_fromDate = m_ui->fromDateEdit->date();
  m_toDate = m_ui->toDateEdit->date();
  m_limit = m_ui->limitSpinBox->value();

  auto idStr = m_ui->idLineEdit->text();
  bool ok = false;
  auto id = idStr.toInt(&ok);
  if (ok && m_idFilter) {
    m_id = id;
  } else {
    m_id = -1;
  }

  // Save the range
  QSettings* settings = pqApplicationCore::instance()->settings();
  settings->beginGroup(DATABROKER_GROUP);
  settings->setValue(FILTER_FROM_SETTINGS_LABEL, QVariant(m_fromDate));
  settings->setValue(FILTER_TO_SETTINGS_LABEL, QVariant(m_toDate));
  settings->setValue(LIMIT_SETTINGS_LABEL, QVariant(m_limit));
  settings->endGroup();

  this->loadRuns(m_selectedCatalog, m_id, m_dateFilter, m_fromDate, m_toDate,
                 m_limit);
}

} // namespace tomviz
