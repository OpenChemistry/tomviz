/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAdvancedFormatWidget_h
#define tomvizAdvancedFormatWidget_h

#include <QWidget>

#include "MatchInfo.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QScopedPointer>

namespace Ui {
class AdvancedFormatWidget;
}

namespace tomviz {
class AdvancedFormatWidget : public QWidget
{
  Q_OBJECT

public:
  AdvancedFormatWidget(QWidget* parent = nullptr);
  ~AdvancedFormatWidget() override;

  QString getRegex() const;
  QString getPythonRegex() const;
  MatchInfo matchFileName(QString) const;
  QJsonArray getRegexGroups() const;
  QJsonObject getRegexSubsitutions() const;

public slots:

private slots:

signals:
  void regexChanged(QString);

private:
  QScopedPointer<Ui::AdvancedFormatWidget> m_ui;

  QLabel m_regexErrorLabel;

  QString m_fileNameRegex;

  void setEnabledRegexGroupsWidget(bool enabled);
  void setEnabledRegexGroupsSubstitutionsWidget(bool enabled);
};
}

#endif
