/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AdvancedFormatWidget.h"

#include "ui_AdvancedFormatWidget.h"

namespace tomviz {

AdvancedFormatWidget::AdvancedFormatWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::AdvancedFormatWidget)
{
  m_ui->setupUi(this);

  // Default file name regex
  m_fileNameRegex = QString(".*_([n,p]{1}[\\d,\\.]+)degree.*\\.dm3");
  m_ui->fileNameRegexLineEdit->setText(m_fileNameRegex);

  connect(m_ui->fileNameRegexLineEdit, &QLineEdit::textChanged,
          [this](QString pattern) {
            setEnabledRegexGroupsWidget(!pattern.isEmpty());
            m_fileNameRegex = pattern;
            emit regexChanged(pattern);
          });
  setEnabledRegexGroupsWidget(!m_ui->fileNameRegexLineEdit->text().isEmpty());

  connect(m_ui->regexGroupsWidget, &RegexGroupsWidget::groupsChanged, [this]() {
    emit regexChanged(m_fileNameRegex);
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
  QRegExp regex = QRegExp(m_fileNameRegex);
  bool match = regex.exactMatch(fileName);
  MatchInfo result;
  result.matched = match;
  QList<CapGroup> groups;
  QStringList namedGroups = m_ui->regexGroupsWidget->regexGroups();

  if (namedGroups.size() == 0) {
    groups << CapGroup(QString("Full match"), regex.cap(0));
  } else {
    for (int i = 0; i < namedGroups.size(); ++i) {
      groups << CapGroup(namedGroups[i], regex.cap(i + 1));
    }
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
  return m_fileNameRegex;
}

QString AdvancedFormatWidget::getPythonRegex() const
{
  QString regex = m_fileNameRegex;
  // Let's not replace it if it isn't needed
  regex.replace(QString(".*?"), QString(".*"));
  regex.replace(QString(".*"), QString(".*?"));
  return regex;
}
}
