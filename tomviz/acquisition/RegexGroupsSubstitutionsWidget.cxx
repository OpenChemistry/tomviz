/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RegexGroupsSubstitutionsWidget.h"
#include "RegexGroupSubstitutionDialog.h"
#include "ui_RegexGroupsSubstitutionsWidget.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QListWidget>
#include <QMenu>

#include <algorithm>

namespace tomviz {

RegexGroupsSubstitutionsWidget::RegexGroupsSubstitutionsWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::RegexGroupsSubstitutionsWidget)
{
  m_ui->setupUi(this);
  autoResizeTable();

  readSettings();

  foreach (const RegexGroupSubstitution& substitution, m_substitutions) {
    addRegexGroupSubstitution(substitution);
  }

  // New
  connect(m_ui->newSubstitutionButton, &QPushButton::clicked, [this]() {
    RegexGroupSubstitutionDialog dialog;
    auto r = dialog.exec();
    if (r != QDialog::Accepted) {
      return;
    }

    RegexGroupSubstitution newSubstitution(dialog.groupName(), dialog.regex(),
                                           dialog.substitution());

    m_substitutions.append(newSubstitution);
    sortRegexGroupSubstitutions();
    addRegexGroupSubstitution(newSubstitution);

    writeSettings();
  });

  // Edit
  connect(m_ui->regexGroupsSubstitutionsWidget,
          &QTableWidget::itemDoubleClicked, [this](QTableWidgetItem* item) {
            auto row = m_ui->regexGroupsSubstitutionsWidget->row(item);
            editRegexGroupSubstitution(row);
            writeSettings();
          });

  // Delete
  connect(m_ui->regexGroupsSubstitutionsWidget,
          &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
            auto globalPos =
              this->m_ui->regexGroupsSubstitutionsWidget->mapToGlobal(pos);
            QMenu contextMenu;
            contextMenu.addAction("Delete", this, [this, &pos]() {
              auto item =
                this->m_ui->regexGroupsSubstitutionsWidget->itemAt(pos);
              auto row = this->m_ui->regexGroupsSubstitutionsWidget->row(item);
              this->m_ui->regexGroupsSubstitutionsWidget->removeRow(row);
              this->m_substitutions.removeAt(row);
              this->writeSettings();
              autoResizeTable();
            });

            // Show context menu at handling position
            contextMenu.exec(globalPos);
          });
}

RegexGroupsSubstitutionsWidget::~RegexGroupsSubstitutionsWidget() = default;

void RegexGroupsSubstitutionsWidget::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  auto substitutions = settings->value("regexGroupsSubstitutions").toList();
  // If setting doesn't exist use the defaults
  if (!settings->contains("regexGroupsSubstitutions")) {
    QVariant pos;
    pos.setValue(RegexGroupSubstitution("angle", "n", "-"));
    substitutions.append(pos);
    QVariant neg;
    neg.setValue(RegexGroupSubstitution("angle", "p", "+"));
    substitutions.append(neg);
  }
  foreach (const QVariant conn, substitutions) {
    m_substitutions.append(conn.value<RegexGroupSubstitution>());
  }
  settings->endGroup();
}

void RegexGroupsSubstitutionsWidget::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");

  QVariantList substitutions;
  foreach (const RegexGroupSubstitution& substitution, m_substitutions) {
    QVariant variant;
    variant.setValue(substitution);
    substitutions.append(variant);
  }
  settings->setValue("regexGroupsSubstitutions", substitutions);
  settings->endGroup();
}

void RegexGroupsSubstitutionsWidget::sortRegexGroupSubstitutions()
{
  std::sort(m_substitutions.begin(), m_substitutions.end(),
            [](RegexGroupSubstitution a, RegexGroupSubstitution b) {
              return a.groupName() < b.groupName();
            });
}

void RegexGroupsSubstitutionsWidget::editRegexGroupSubstitution(int row)
{
  RegexGroupSubstitution substitution = m_substitutions[row];
  RegexGroupSubstitutionDialog dialog(substitution.groupName(),
                                      substitution.regex(),
                                      substitution.substitution());
  dialog.exec();
  RegexGroupSubstitution newSubstitution(dialog.groupName(), dialog.regex(),
                                         dialog.substitution());

  m_substitutions[row] = newSubstitution;
  setRegexGroupSubstitution(row, newSubstitution);
}

void RegexGroupsSubstitutionsWidget::addRegexGroupSubstitution(
  RegexGroupSubstitution substitution)
{
  auto row = m_ui->regexGroupsSubstitutionsWidget->rowCount();
  m_ui->regexGroupsSubstitutionsWidget->insertRow(row);
  setRegexGroupSubstitution(row, substitution);
  autoResizeTable();
}

void RegexGroupsSubstitutionsWidget::setRegexGroupSubstitution(
  int row, RegexGroupSubstitution substitution)
{
  QTableWidgetItem* groupNameItem =
    new QTableWidgetItem(substitution.groupName());
  QTableWidgetItem* regexItem = new QTableWidgetItem(substitution.regex());
  QTableWidgetItem* substitutionItem =
    new QTableWidgetItem(substitution.substitution());
  m_ui->regexGroupsSubstitutionsWidget->setItem(row, 0, groupNameItem);
  m_ui->regexGroupsSubstitutionsWidget->setItem(row, 1, regexItem);
  m_ui->regexGroupsSubstitutionsWidget->setItem(row, 2, substitutionItem);
}

QList<RegexGroupSubstitution> RegexGroupsSubstitutionsWidget::substitutions()
{
  return m_substitutions;
}

void RegexGroupsSubstitutionsWidget::autoResizeTable()
{
  // Auto resize the size when adding/deleting entries.
  // Keep the size between is between 0 and 2 rows.
  m_ui->regexGroupsSubstitutionsWidget->resizeColumnsToContents();
  m_ui->regexGroupsSubstitutionsWidget->horizontalHeader()
    ->setSectionResizeMode(0, QHeaderView::Stretch);
  int vSize =
    m_ui->regexGroupsSubstitutionsWidget->horizontalHeader()->height();
  for (int i = 0; i < m_substitutions.size(); ++i) {
    vSize +=
      m_ui->regexGroupsSubstitutionsWidget->verticalHeader()->sectionSize(i);
    if (i >= 1) {
      break;
    }
  }
  vSize += 2;
  m_ui->regexGroupsSubstitutionsWidget->setMinimumHeight(vSize);
  m_ui->regexGroupsSubstitutionsWidget->setMaximumHeight(vSize);
}

} // namespace tomviz
