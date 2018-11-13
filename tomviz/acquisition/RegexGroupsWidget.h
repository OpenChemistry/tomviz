/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRegexGroupsWidget_h
#define tomvizRegexGroupsWidget_h

#include <QWidget>

#include <QScopedPointer>
#include <QStringList>

namespace Ui {
class RegexGroupsWidget;
}

namespace tomviz {

class RegexGroupsWidget : public QWidget
{
  Q_OBJECT

public:
  RegexGroupsWidget(QWidget* parent);
  ~RegexGroupsWidget() override;

  QStringList regexGroups();

signals:
  void groupsChanged();

private:
  QScopedPointer<Ui::RegexGroupsWidget> m_ui;

  void readSettings();
  void writeSettings();
};
} // namespace tomviz

#endif
