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
};
} // namespace tomviz

#endif
