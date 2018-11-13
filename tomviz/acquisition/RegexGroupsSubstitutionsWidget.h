/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRegexGroupsSubstitutionsWidget_h
#define tomvizRegexGroupsSubstitutionsWidget_h

#include <QWidget>

#include <QMap>
#include <QScopedPointer>
#include <QVariantList>

#include "RegexGroupSubstitution.h"

namespace Ui {
class RegexGroupsSubstitutionsWidget;
}

namespace tomviz {

class RegexGroupsSubstitutionsWidget : public QWidget
{
  Q_OBJECT

public:
  RegexGroupsSubstitutionsWidget(QWidget* parent);
  ~RegexGroupsSubstitutionsWidget() override;

  QList<RegexGroupSubstitution> substitutions();

private:
  QScopedPointer<Ui::RegexGroupsSubstitutionsWidget> m_ui;
  QList<RegexGroupSubstitution> m_substitutions;

  void readSettings();
  void writeSettings();
  void setRegexGroupSubstitutions(const QVariantList& substitutions);
  void sortRegexGroupSubstitutions();
  void editRegexGroupSubstitution(int row);
  void addRegexGroupSubstitution(RegexGroupSubstitution substitution);
  void setRegexGroupSubstitution(int row, RegexGroupSubstitution substitution);
  void autoResizeTable();
};
} // namespace tomviz

#endif
