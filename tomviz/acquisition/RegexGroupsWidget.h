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

#ifndef tomvizRegexGroupsWidget_h
#define tomvizRegexGroupsWidget_h

#include <QScopedPointer>
#include <QStringList>
#include <QWidget>

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
}

#endif
