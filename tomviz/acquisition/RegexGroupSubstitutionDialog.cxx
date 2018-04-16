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

#include "RegexGroupSubstitutionDialog.h"
#include "ui_RegexGroupSubstitutionDialog.h"

#include <QPalette>
#include <QRegExp>

namespace tomviz {

RegexGroupSubstitutionDialog::RegexGroupSubstitutionDialog(
  const QString groupName, const QString regex, const QString substitution,
  QWidget* parent)
  : QDialog(parent), m_ui(new Ui::RegexGroupSubstitutionDialog)
{
  m_ui->setupUi(this);

  m_ui->groupNameLineEdit->setText(groupName);
  m_ui->regexLineEdit->setText(regex);
  m_ui->substitutionLineEdit->setText(substitution);

  auto palette = m_regexErrorLabel.palette();
  palette.setColor(m_regexErrorLabel.foregroundRole(), Qt::red);
  m_regexErrorLabel.setPalette(palette);

  connect(m_ui->regexLineEdit, &QLineEdit::textChanged, [this]() {
    m_ui->formLayout->removeWidget(&m_regexErrorLabel);
    m_regexErrorLabel.setText("");
  });
}

RegexGroupSubstitutionDialog::~RegexGroupSubstitutionDialog() = default;

QString RegexGroupSubstitutionDialog::groupName()
{
  return m_ui->groupNameLineEdit->text();
}

QString RegexGroupSubstitutionDialog::regex()
{
  return m_ui->regexLineEdit->text();
}

QString RegexGroupSubstitutionDialog::substitution()
{
  return m_ui->substitutionLineEdit->text();
}

void RegexGroupSubstitutionDialog::done(int r)
{
  if (QDialog::Accepted == r) {
    QRegExp regExp(m_ui->regexLineEdit->text());
    if (!regExp.isValid()) {
      m_regexErrorLabel.setText(regExp.errorString());
      m_ui->formLayout->insertRow(2, "", &m_regexErrorLabel);
      return;
    }
  }

  QDialog::done(r);
}
}
