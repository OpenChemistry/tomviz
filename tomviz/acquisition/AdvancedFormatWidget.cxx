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
#include "AdvancedFormatWidget.h"

#include "ui_AdvancedFormatWidget.h"

namespace tomviz {

AdvancedFormatWidget::AdvancedFormatWidget(QWidget *parent)
  : QWidget(parent), m_ui(new Ui::AdvancedFormatWidget)
{
  m_ui->setupUi(this);

  // Default file name regex
  m_ui->fileNameRegexLineEdit->setText(".*_([n,p]{1}[\\d,\\.]+)degree.*\\.dm3");

  connect(m_ui->fileNameRegexLineEdit, &QLineEdit::textChanged, [this](QString regex) {
    setEnabledRegexGroupsWidget(!regex.isEmpty());
    m_fileNameRegex = QRegExp(regex);
    emit regexChanged(regex);
  });
  setEnabledRegexGroupsWidget(!m_ui->fileNameRegexLineEdit->text().isEmpty());

  connect(m_ui->regexGroupsWidget, &RegexGroupsWidget::groupsChanged, [this]() {
    setEnabledRegexGroupsSubstitutionsWidget(
      !m_ui->regexGroupsWidget->regexGroups().isEmpty());
  });
  setEnabledRegexGroupsSubstitutionsWidget(
    !m_ui->regexGroupsWidget->regexGroups().isEmpty());

  // Setup regex error label
  auto palette = m_regexErrorLabel.palette();
  palette.setColor(m_regexErrorLabel.foregroundRole(), Qt::red);
  m_regexErrorLabel.setPalette(palette);

  connect(m_ui->fileNameRegexLineEdit, &QLineEdit::textChanged, [this]() {
    m_ui->gridLayout->removeWidget(&m_regexErrorLabel);
    m_regexErrorLabel.setText("");
  });
}

AdvancedFormatWidget::~AdvancedFormatWidget() = default;

void AdvancedFormatWidget::setEnabledRegexGroupsWidget(bool enabled)
{
  m_ui->regexGroupsLabel->setEnabled(enabled);
  m_ui->regexGroupsWidget->setEnabled(enabled);
}

void AdvancedFormatWidget::setEnabledRegexGroupsSubstitutionsWidget(
  bool enabled)
{
  m_ui->regexGroupsSubstitutionsLabel->setEnabled(enabled);
  m_ui->regexGroupsSubstitutionsWidget->setEnabled(enabled);
}

MatchInfo AdvancedFormatWidget::matchFileName(QString fileName) const
{
  bool match = m_fileNameRegex.exactMatch(fileName);
  MatchInfo result;
  result.matched = match;
  QList<CapGroup> groups;
  QStringList namedGroups = m_ui->regexGroupsWidget->regexGroups();
  for (int i = 0; i < namedGroups.size(); ++i) {
    groups << CapGroup(namedGroups[i], m_fileNameRegex.cap(i+1));
  }
  result.groups = groups;
  return result;
}

QJsonArray AdvancedFormatWidget::getRegexGroups() const
{
  auto regexGroups = m_ui->regexGroupsWidget->regexGroups();
  return QJsonArray::fromStringList(regexGroups);
}

QJsonObject AdvancedFormatWidget::getRegexSubsitutions() const
{
  auto regexSubstitutions =
    m_ui->regexGroupsSubstitutionsWidget->substitutions();
  QJsonObject substitutions;
  foreach (RegexGroupSubstitution sub, regexSubstitutions) {
    QJsonArray regexToSubs;
    if (substitutions.contains(sub.groupName())) {
      regexToSubs = substitutions.value(sub.groupName()).toArray();
    }
    QJsonObject mapping;
    mapping[sub.regex()] = sub.substitution();
    regexToSubs.append(mapping);
    substitutions[sub.groupName()] = regexToSubs;
  }
  return substitutions;
}

QString AdvancedFormatWidget::getRegex() const
{
  return m_fileNameRegex.pattern();
}

QString AdvancedFormatWidget::getPythonRegex() const
{
  auto regex = m_fileNameRegex.pattern();
  regex.replace(QString("*"), QString("*?"));
  return regex;
}

}
