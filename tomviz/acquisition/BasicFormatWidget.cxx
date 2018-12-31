/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "BasicFormatWidget.h"

#include "ui_BasicFormatWidget.h"

#include <QDebug>

namespace tomviz {

BasicFormatWidget::BasicFormatWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::BasicFormatWidget),
    m_defaultFormatOrder(makeDefaultFormatOrder()),
    m_defaultExtensionOrder(makeDefaultExtensionOrder()),
    m_defaultFileNames(makeDefaultFileNames()),
    m_defaultFormatLabels(makeDefaultFormatLabels()),
    m_defaultExtensionLabels(makeDefaultExtensionLabels()),
    m_defaultRegexParams(makeDefaultRegexParams())
{
  m_ui->setupUi(this);

  connect(m_ui->fileFormatCombo, static_cast<void (QComboBox::*)(int)>(
                                   &QComboBox::currentIndexChanged),
          this, &BasicFormatWidget::onFormatChanged);

  connect(m_ui->fileExtensionCombo, static_cast<void (QComboBox::*)(int)>(
                                      &QComboBox::currentIndexChanged),
          this, &BasicFormatWidget::onExtensionChanged);

  setupFileFormatCombo();
  setupFileExtensionCombo();
  setupRegexDisplayLine();

  connect(m_ui->customFormatWidget, &CustomFormatWidget::fieldsChanged, this,
          &BasicFormatWidget::customFileRegex);
}

BasicFormatWidget::~BasicFormatWidget() = default;

void BasicFormatWidget::setupFileFormatCombo()
{
  foreach (auto format, m_defaultFormatOrder) {
    m_ui->fileFormatCombo->addItem(m_defaultFormatLabels[format]);
  }
}

void BasicFormatWidget::setupFileExtensionCombo()
{
  foreach (auto format, m_defaultExtensionOrder) {
    m_ui->fileExtensionCombo->addItem(m_defaultExtensionLabels[format]);
  }
}

void BasicFormatWidget::onFormatChanged(int index)
{
  if (index >= m_defaultFormatOrder.size() || index < 0) {
    return;
  }

  m_format = m_defaultFormatOrder[index];

  switch (m_format) {
    case RegexFormat::Custom:
      m_ui->customGroupBox->setVisible(true);
      m_ui->customGroupBox->setEnabled(true);
      m_ui->fileExtensionCombo->setEnabled(false);
      break;
    default:
      m_ui->customGroupBox->setVisible(true);
      m_ui->customGroupBox->setEnabled(false);
      m_ui->fileExtensionCombo->setEnabled(true);
  }
  updateRegex();
  emit fileFormatChanged();
}

void BasicFormatWidget::onExtensionChanged(int index)
{
  if (index >= m_defaultExtensionOrder.size() || index < 0) {
    return;
  }
  m_extension = m_defaultExtensionOrder[index];
  updateRegex();
  emit fileFormatChanged();
}

void BasicFormatWidget::updateRegex()
{
  QStringList params =
    m_defaultRegexParams[std::make_pair(m_format, m_extension)];
  if (params.size() != 5) {
    qCritical() << "Default Regex Parameters size not equal to 5";
    return;
  }
  buildFileRegex(params[0], params[1], params[2], params[3], params[4]);

  m_ui->customFormatWidget->setFields(params);
}

void BasicFormatWidget::buildFileRegex(QString prefix, QString negChar,
                                       QString posChar, QString suffix,
                                       QString extension)
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

  m_fileNameRegex = QString("^%1((%2|%3)(\\d+(\\.\\d+)?))%4(\\.%5)$")
                      .arg(prefix)
                      .arg(QRegExp::escape(negChar))
                      .arg(QRegExp::escape(posChar))
                      .arg(suffix)
                      .arg(extension);

  m_pythonFileNameRegex = QString("^%1((%2|%3)(\\d+(\\.\\d+)?))%4(\\.%5)$")
                            .arg(pythonPrefix)
                            .arg(QRegExp::escape(negChar))
                            .arg(QRegExp::escape(posChar))
                            .arg(pythonSuffix)
                            .arg(extension);
  m_negChar = negChar;
  m_posChar = posChar;

  m_ui->regexDisplay->setText(m_fileNameRegex);
  emit regexChanged(m_fileNameRegex);
}

void BasicFormatWidget::customFileRegex()
{
  QStringList fields = m_ui->customFormatWidget->getFields();
  if (fields.size() != 5) {
    qCritical() << "Field Regex Parameters size not equal to 5";
    return;
  }
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
  QRegExp regex = QRegExp(m_fileNameRegex);
  bool match = regex.exactMatch(fileName);

  if (match) {
    valid = true;

    sign = regex.cap(2);
    numStr = regex.cap(3);
    ext = regex.cap(5);

    int start = 0;
    int stop = 0;
    stop = regex.pos(1);
    prefix = fileName.left(stop);
    start = regex.pos(1) + regex.cap(1).size();
    stop = regex.pos(5);
    suffix = fileName.mid(start, stop - start);

    if (sign == m_posChar) {
      sign = "+";
    } else if (sign == m_negChar) {
      sign = "-";
    }
    num = (sign + numStr).toFloat();

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
  groups << CapGroup("Prefix", prefix);
  groups << CapGroup("Angle", sign + numStr);
  groups << CapGroup("Suffix", suffix);
  groups << CapGroup("Ext", ext);
  result.groups = groups;
  return result;
}

QString BasicFormatWidget::getRegex() const
{
  return m_fileNameRegex;
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
  sub0[QRegExp::escape(m_posChar)] = "+";
  QJsonObject sub1;
  sub1[QRegExp::escape(m_negChar)] = "-";
  regexToSubs.append(sub0);
  regexToSubs.append(sub1);
  substitutions["angle"] = regexToSubs;
  return substitutions;
}

bool BasicFormatWidget::isDefaultFilename(const QString& fileName) const
{
  foreach (auto format, m_defaultFormatOrder) {
    foreach (auto extension, m_defaultExtensionOrder) {
      if (fileName == m_defaultFileNames[std::make_pair(format, extension)]) {
        return true;
      }
    }
  }
  return false;
}

QString BasicFormatWidget::getDefaultFilename() const
{
  return m_defaultFileNames[std::make_pair(m_format, m_extension)];
}

void BasicFormatWidget::setupRegexDisplayLine()
{
  QLineEdit* display = m_ui->regexDisplay;
  display->setReadOnly(true);
  QPalette pal = display->palette();
  QColor disabledBg = pal.color(QPalette::Disabled, QPalette::Base);
  pal.setColor(QPalette::Active, QPalette::Base, disabledBg);
  pal.setColor(QPalette::Inactive, QPalette::Base, disabledBg);
  display->setPalette(pal);
}

QMap<std::pair<RegexFormat, RegexExtension>, QString>
BasicFormatWidget::makeDefaultFileNames() const
{
  QMap<std::pair<RegexFormat, RegexExtension>, QString> defaultFilenames;
  QString fileBase;
  QString fileExt;
  foreach (auto format, m_defaultFormatOrder) {
    foreach (auto extension, m_defaultExtensionOrder) {
      fileBase = QString("");
      fileExt = QString("");
      if (format == RegexFormat::NegativePositive) {
        fileBase = QString("Prefix_n12.3_Suffix");
      } else if (format == RegexFormat::PlusMinus) {
        fileBase = QString("Prefix_+12.3_Suffix");
      }

      if (format != RegexFormat::Custom) {
        if (extension == RegexExtension::dm3) {
          fileExt = QString(".dm3");
        } else if (extension == RegexExtension::tiff) {
          fileExt = QString(".tiff");
        }
      }

      defaultFilenames[std::make_pair(format, extension)] =
        QString("%1%2").arg(fileBase).arg(fileExt);
    }
  }
  return defaultFilenames;
}

QMap<RegexFormat, QString> BasicFormatWidget::makeDefaultFormatLabels() const
{
  QMap<RegexFormat, QString> defaultLabels;
  defaultLabels[RegexFormat::NegativePositive] =
    QString("<prefix>[n|p]<angle><suffix>");
  defaultLabels[RegexFormat::PlusMinus] =
    QString("<prefix>[-|+]<angle><suffix>");
  defaultLabels[RegexFormat::Custom] = QString("Custom");

  return defaultLabels;
}

QMap<RegexExtension, QString> BasicFormatWidget::makeDefaultExtensionLabels()
  const
{
  QMap<RegexExtension, QString> defaultLabels;
  defaultLabels[RegexExtension::dm3] = QString(".dm3");
  defaultLabels[RegexExtension::tiff] = QString(".tiff");

  return defaultLabels;
}

QMap<std::pair<RegexFormat, RegexExtension>, QStringList>
BasicFormatWidget::makeDefaultRegexParams() const
{
  QMap<std::pair<RegexFormat, RegexExtension>, QStringList> defaultRegexParams;
  foreach (auto format, m_defaultFormatOrder) {
    foreach (auto extension, m_defaultExtensionOrder) {
      QStringList params;

      if (format == RegexFormat::NegativePositive) {
        params << "*"
               << "n"
               << "p"
               << "*";
      } else if (format == RegexFormat::PlusMinus) {
        params << "*"
               << "-"
               << "+"
               << "*";
      } else if (format == RegexFormat::Custom) {
        params << "*"
               << "n"
               << "p"
               << "*";
      }

      if (format == RegexFormat::Custom) {
        params << "*";
      } else if (extension == RegexExtension::dm3) {
        params << "dm3";
      } else if (extension == RegexExtension::tiff) {
        params << "tif[f]?";
      }

      defaultRegexParams[std::make_pair(format, extension)] = params;
    }
  }

  return defaultRegexParams;
}

QList<RegexFormat> BasicFormatWidget::makeDefaultFormatOrder() const
{
  QList<RegexFormat> defaultOrder;
  defaultOrder << RegexFormat::NegativePositive << RegexFormat::PlusMinus
               << RegexFormat::Custom;
  return defaultOrder;
}

QList<RegexExtension> BasicFormatWidget::makeDefaultExtensionOrder() const
{
  QList<RegexExtension> defaultOrder;
  defaultOrder << RegexExtension::dm3 << RegexExtension::tiff;
  return defaultOrder;
}
}
