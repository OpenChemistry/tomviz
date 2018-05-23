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
#include "BasicFormatWidget.h"

#include "ui_BasicFormatWidget.h"

#include <QDebug>

namespace tomviz {

BasicFormatWidget::BasicFormatWidget(QWidget *parent)
  : QWidget(parent), m_ui(new Ui::BasicFormatWidget),
    m_defaultFormatOrder(makeDefaultFormatOrder()),
    m_defaultFileNames(makeDefaultFileNames()),
    m_defaultFormatLabels(makeDefaultLabels()),
    m_defaultRegexParams(makeDefaultRegexParams())
{
  m_ui->setupUi(this);

  connect(m_ui->fileFormatCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
          &BasicFormatWidget::formatChanged);

  setupFileFormatCombo();

  connect(m_ui->customFormatWidget, &CustomFormatWidget::fieldsChanged, this, &BasicFormatWidget::customFileRegex);
}

BasicFormatWidget::~BasicFormatWidget() = default;

void BasicFormatWidget::setupFileFormatCombo()
{
  foreach (auto format, m_defaultFormatOrder) {
    m_ui->fileFormatCombo->addItem(m_defaultFormatLabels[format]);
  }
}

void BasicFormatWidget::formatChanged(int index)
{
  qDebug() << "Format Changed";
  qDebug() << index;

  if (index >= m_defaultFormatOrder.size() || index < 0) {
    return;
  }

  QStringList defaultNames;
  foreach (auto format, m_defaultFormatOrder) {
    defaultNames << m_defaultFileNames[format];
  }
  
  // bool isDefaultTestFileName = defaultNames.contains(m_testFileName);
  TestRegexFormat format = m_defaultFormatOrder[index];

  switch(format) {
    case TestRegexFormat::Custom :
      m_ui->customGroupBox->setVisible(true);
      m_ui->customGroupBox->setEnabled(true);
      // m_ui->customFormatWidget->setAllowEdit(true);
      break;
    default :
      m_ui->customGroupBox->setVisible(true);
      m_ui->customGroupBox->setEnabled(false);
      // m_ui->customFormatWidget->setAllowEdit(false);
  }

  buildFileRegex(m_defaultRegexParams[format][0], m_defaultRegexParams[format][1],
                 m_defaultRegexParams[format][2], m_defaultRegexParams[format][3],
                 m_defaultRegexParams[format][4]);

  m_ui->customFormatWidget->setFields(m_defaultRegexParams[format][0], m_defaultRegexParams[format][1],
                 m_defaultRegexParams[format][2], m_defaultRegexParams[format][3],
                 m_defaultRegexParams[format][4]);

  // if (isDefaultTestFileName) {
    // m_testFileName = m_defaultFileNames[format];
    // m_ui->testFileFormatEdit->setText(m_testFileName);
  // }

  // validateFileNameRegex();
}

void BasicFormatWidget::buildFileRegex(QString prefix, QString negChar, QString posChar, QString suffix, QString extension)
{
  // Greediness differences between Qt regex engine in the client,
  // and the python regex engine on the server,
  // require adjustments to the pattern
  if (prefix.trimmed() == "") {
    prefix = QString("*");
  }
  QString pythonPrefix = prefix;
  prefix.replace(QString("*"), QString(".*"));
  pythonPrefix.replace(QString("*"), QString(".*?"));

  if (suffix.trimmed() == "") {
    suffix = QString("*");
  }
  QString pythonSuffix = suffix;
  suffix.replace(QString("*"), QString(".*"));
  pythonSuffix.replace(QString("*"), QString(".*?"));

  if (extension.trimmed() == "" || extension.trimmed() == "*") {
    extension = QString(".+");
  }
  if (negChar.trimmed() == "") {
    negChar = QString("n");
  }
  if (posChar.trimmed() == "") {
    posChar = QString("p");
  }

  QString regExpStr = QString("^%1((%2|%3)?(\\d+(\\.\\d+)?))%4(\\.%5)$")
                        .arg(prefix)
                        .arg(QRegExp::escape(negChar))
                        .arg(QRegExp::escape(posChar))
                        .arg(suffix)
                        .arg(extension);
  
  m_fileNameRegex = QRegExp(regExpStr);
  m_pythonFileNameRegex = QString("^%1((%2|%3)?(\\d+(\\.\\d+)?))%4(\\.%5)$")
                            .arg(pythonPrefix)
                            .arg(QRegExp::escape(negChar))
                            .arg(QRegExp::escape(posChar))
                            .arg(pythonSuffix)
                            .arg(extension);
  m_negChar = negChar;
  m_posChar = posChar;
  // qDebug() << regExpStr;
  emit regexChanged(regExpStr);
}

void BasicFormatWidget::customFileRegex()
{
  QStringList fields = m_ui->customFormatWidget->getFields();
  buildFileRegex(fields[0], fields[1], fields[2], fields[3], fields[4]);
}

MatchInfo BasicFormatWidget::matchFileName(QString fileName) const
{
  QString prefix;
  QString suffix;
  QString sign;
  QString numStr;
  QString ext;
  double num;
  bool valid = false;
  bool match = m_fileNameRegex.exactMatch(fileName);

  if (match) {
    valid = true;

    sign = m_fileNameRegex.cap(2);
		numStr = m_fileNameRegex.cap(3);
		ext = m_fileNameRegex.cap(5);
		
		int start = 0;
		int stop = 0;
		stop = m_fileNameRegex.pos(1);
		prefix = fileName.left(stop);
		start = m_fileNameRegex.pos(1) + m_fileNameRegex.cap(1).size();
		stop = m_fileNameRegex.pos(5);
		suffix = fileName.mid(start, stop - start);
    
    if (sign == m_posChar) {
			sign = "+";
		} else if (sign == m_negChar) {
			sign = "-";
		}
		num = (sign+numStr).toFloat();

    // Special case: only 0 can be missing a positive/negative identifier
    if (sign.trimmed() == "" && num != 0) {
      valid = false;
    }
  }
  
  if (!valid) {
    prefix = "";
    suffix = "";
    sign = "";
    numStr = "";
    ext = "";
  }

  MatchInfo result;
  result.matched = valid;
  QList<CapGroup> groups;
  groups << CapGroup("Prefixx", prefix);
  groups << CapGroup("Angle", sign+numStr);
  groups << CapGroup("Suffix", suffix);
  groups << CapGroup("Ext", ext);
  result.groups = groups;
  return result;
}

QString BasicFormatWidget::getRegex() const
{
  return m_fileNameRegex.pattern();
}

QString BasicFormatWidget::getPythonRegex() const
{
  return m_pythonFileNameRegex;
}

QJsonArray BasicFormatWidget::getRegexGroups() const
{
  return QJsonArray::fromStringList(QStringList(QString("angle")));
}

QJsonObject BasicFormatWidget::getRegexSubsitutions() const
{
  QJsonObject substitutions;
  QJsonArray regexToSubs;
  QJsonObject sub0;
  sub0[m_posChar] = "+";
  QJsonObject sub1;
  sub1[m_negChar] = "-";
  regexToSubs.append(sub0);
  regexToSubs.append(sub1);
  substitutions["angle"] = regexToSubs;
  return substitutions;
}

QMap<TestRegexFormat, QString> BasicFormatWidget::makeDefaultFileNames() const
{
  QMap<TestRegexFormat, QString> defaultFilenames;
  defaultFilenames[TestRegexFormat::npDm3] = QString("ThePrefix_n12.3degree_TheSuffix.dm3");
  defaultFilenames[TestRegexFormat::pmDm3] = QString("ThePrefix_-12.3degree_TheSuffix.dm3");
  defaultFilenames[TestRegexFormat::npTiff] = QString("ThePrefix_n12.3_TheSuffix.tif");
  defaultFilenames[TestRegexFormat::pmTiff] = QString("ThePrefix_+12.3_TheSuffix.tif");
  defaultFilenames[TestRegexFormat::Custom] = QString("");
  // defaultFilenames[TestRegexFormat::Advanced] = QString("");

  return defaultFilenames;
}

QMap<TestRegexFormat, QString> BasicFormatWidget::makeDefaultLabels() const
{
  QMap<TestRegexFormat, QString> defaultLabels;
  defaultLabels[TestRegexFormat::npDm3] = QString("<prefix>[n|p]<angle>degree<suffix>.dm3");
  defaultLabels[TestRegexFormat::pmDm3] = QString("<prefix>[-|+]<angle>degree<suffix>.dm3");
  defaultLabels[TestRegexFormat::npTiff] = QString("<prefix>[n|p]<angle><suffix>.tif[f]");
  defaultLabels[TestRegexFormat::pmTiff] = QString("<prefix>[-|+]<angle><suffix>.tif[f]");
  defaultLabels[TestRegexFormat::Custom] = QString("Custom");
  // defaultLabels[TestRegexFormat::Advanced] = QString("Advanced");

  return defaultLabels;
}

QMap<TestRegexFormat, QStringList> BasicFormatWidget::makeDefaultRegexParams() const
{
  QMap<TestRegexFormat, QStringList> defaultRegexParams;
  
  QStringList params0;
  params0 << "*" << "n" << "p" << "degree*" << "dm3";
  defaultRegexParams[TestRegexFormat::npDm3] = params0;

  QStringList params1;
  params1 << "*" << "-" << "+" << "degree*" << "dm3";
  defaultRegexParams[TestRegexFormat::pmDm3] = params1;

  QStringList params2;
  params2 << "*" << "n" << "p" << "*" << "tif[f]?";
  defaultRegexParams[TestRegexFormat::npTiff] = params2;

  QStringList params3;
  params3 << "*" << "-" << "+" << "*" << "tif[f]?";
  defaultRegexParams[TestRegexFormat::pmTiff] = params3;

  QStringList params4;
  params4 << "*" << "n" << "p" << "*" << "*";
  defaultRegexParams[TestRegexFormat::Custom] = params4;

  defaultRegexParams[TestRegexFormat::Advanced] = params4;

  return defaultRegexParams;
}

QList<TestRegexFormat> BasicFormatWidget::makeDefaultFormatOrder() const
{
  QList<TestRegexFormat> defaultOrder;
  defaultOrder << TestRegexFormat::npDm3
               << TestRegexFormat::pmDm3
               << TestRegexFormat::npTiff
               << TestRegexFormat::pmTiff
               << TestRegexFormat::Custom;
              //  << TestRegexFormat::Advanced;
  return defaultOrder;               
}


}
