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
  this->m_ui->setupUi(this);

  this->readSettings();

  foreach (const RegexGroupSubstitution& substitution, m_substitutions) {
    this->addRegexGroupSubstitution(substitution);
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
    this->sortRegexGroupSubstitutions();
    this->addRegexGroupSubstitution(newSubstitution);

    this->writeSettings();
  });

  // Edit
  connect(m_ui->regexGroupsSubstitutionsWidget,
          &QTableWidget::itemDoubleClicked, [this](QTableWidgetItem* item) {
            auto row = this->m_ui->regexGroupsSubstitutionsWidget->row(item);
            this->editRegexGroupSubstitution(row);
            this->writeSettings();
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
  RegexGroupSubstitution substitution = this->m_substitutions[row];
  RegexGroupSubstitutionDialog dialog(substitution.groupName(),
                                      substitution.regex(),
                                      substitution.substitution());
  dialog.exec();
  RegexGroupSubstitution newSubstitution(dialog.groupName(), dialog.regex(),
                                         dialog.substitution());

  m_substitutions[row] = newSubstitution;
  this->setRegexGroupSubstitution(row, newSubstitution);
}

void RegexGroupsSubstitutionsWidget::addRegexGroupSubstitution(
  RegexGroupSubstitution substitution)
{
  auto row = this->m_ui->regexGroupsSubstitutionsWidget->rowCount();
  this->m_ui->regexGroupsSubstitutionsWidget->insertRow(row);
  this->setRegexGroupSubstitution(row, substitution);
}

void RegexGroupsSubstitutionsWidget::setRegexGroupSubstitution(
  int row, RegexGroupSubstitution substitution)
{
  QTableWidgetItem* groupNameItem =
    new QTableWidgetItem(substitution.groupName());
  QTableWidgetItem* regexItem = new QTableWidgetItem(substitution.regex());
  QTableWidgetItem* substitutionItem =
    new QTableWidgetItem(substitution.substitution());
  this->m_ui->regexGroupsSubstitutionsWidget->setItem(row, 0, groupNameItem);
  this->m_ui->regexGroupsSubstitutionsWidget->setItem(row, 1, regexItem);
  this->m_ui->regexGroupsSubstitutionsWidget->setItem(row, 2, substitutionItem);
}

QList<RegexGroupSubstitution> RegexGroupsSubstitutionsWidget::substitutions()
{
  return m_substitutions;
}
}
