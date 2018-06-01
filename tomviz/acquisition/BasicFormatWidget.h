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
#ifndef tomvizBasicFormatWidget_h
#define tomvizBasicFormatWidget_h

#include <QWidget>

#include "MatchInfo.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QScopedPointer>
#include <QString>

namespace Ui {
class BasicFormatWidget;
}

namespace tomviz {

enum class RegexFormat
{
  NegativePositive,
  PlusMinus,
  Custom
};

enum class RegexExtension
{
  tiff,
  dm3
};

class BasicFormatWidget : public QWidget
{
  Q_OBJECT

public:
  BasicFormatWidget(QWidget* parent = nullptr);
  ~BasicFormatWidget() override;

  QString getRegex() const;
  QString getPythonRegex() const;
  MatchInfo matchFileName(QString) const;
  QJsonArray getRegexGroups() const;
  QJsonObject getRegexSubsitutions() const;
  bool isDefaultFilename(const QString& fileName) const;
  QString getDefaultFilename() const;

private slots:
  void onFormatChanged(int);
  void onExtensionChanged(int);

signals:
  void fileFormatChanged();
  void regexChanged(QString);

private:
  QScopedPointer<Ui::BasicFormatWidget> m_ui;

  QString m_fileNameRegex;
  QString m_negChar;
  QString m_posChar;
  QString m_pythonFileNameRegex;
  QString m_testFileName;
  RegexFormat m_format = RegexFormat::NegativePositive;
  RegexExtension m_extension = RegexExtension::dm3;

  void setupFileFormatCombo();
  void setupFileExtensionCombo();
  void setupRegexDisplayLine();

  QList<RegexFormat> makeDefaultFormatOrder() const;
  QList<RegexExtension> makeDefaultExtensionOrder() const;
  QMap<std::pair<RegexFormat, RegexExtension>, QString> makeDefaultFileNames()
    const;
  QMap<RegexFormat, QString> makeDefaultFormatLabels() const;
  QMap<RegexExtension, QString> makeDefaultExtensionLabels() const;
  QMap<std::pair<RegexFormat, RegexExtension>, QStringList>
  makeDefaultRegexParams() const;

  const QList<RegexFormat> m_defaultFormatOrder;
  const QList<RegexExtension> m_defaultExtensionOrder;
  const QMap<std::pair<RegexFormat, RegexExtension>, QString>
    m_defaultFileNames;
  const QMap<RegexFormat, QString> m_defaultFormatLabels;
  const QMap<RegexExtension, QString> m_defaultExtensionLabels;
  const QMap<std::pair<RegexFormat, RegexExtension>, QStringList>
    m_defaultRegexParams;

  void updateRegex();
  void buildFileRegex(QString, QString, QString, QString, QString);
  void customFileRegex();
  void validateFileNameRegex();
};
}

#endif
