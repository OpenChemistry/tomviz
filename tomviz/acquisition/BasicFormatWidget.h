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

enum class TestRegexFormat {
  npDm3,
  pmDm3,
  npTiff,
  pmTiff,
  Custom,
  Advanced
};

class BasicFormatWidget : public QWidget {
  Q_OBJECT

  public:
    BasicFormatWidget(QWidget *parent = nullptr);
    ~BasicFormatWidget() override;
    QString getRegex() const;
    QString getPythonRegex() const;
    MatchInfo matchFileName(QString) const;
    QJsonArray getRegexGroups() const;
    QJsonObject getRegexSubsitutions() const;    

  public slots:

  private slots:
    void formatChanged(int);


  signals:
    void fileFormatChanged(TestRegexFormat);
    void regexChanged(QString);

  private:
  QScopedPointer<Ui::BasicFormatWidget> m_ui;

  QRegExp m_fileNameRegex;
  QString m_negChar;
  QString m_posChar;
  QString m_pythonFileNameRegex;
  QString m_testFileName;

  void setupFileFormatCombo();
  
  QList<TestRegexFormat> makeDefaultFormatOrder() const;
  QMap<TestRegexFormat, QString> makeDefaultFileNames() const;
  QMap<TestRegexFormat, QString> makeDefaultLabels() const;
  QMap<TestRegexFormat, QStringList> makeDefaultRegexParams() const;

  const QList<TestRegexFormat> m_defaultFormatOrder;
  const QMap<TestRegexFormat, QString> m_defaultFileNames;
  const QMap<TestRegexFormat, QString> m_defaultFormatLabels;
  const QMap<TestRegexFormat, QStringList> m_defaultRegexParams;

  void buildFileRegex(QString, QString, QString, QString, QString);
  void customFileRegex();
  void validateFileNameRegex();
};

}



#endif
