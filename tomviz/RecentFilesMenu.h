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
#ifndef tomvizRecentFilesMenu_h
#define tomvizRecentFilesMenu_h

#include <QObject>

class QAction;
class QMenu;

namespace tomviz {

class DataSource;

/// Adds recent file and recent state file support to Tomviz.
class RecentFilesMenu : public QObject
{
  Q_OBJECT

public:
  RecentFilesMenu(QMenu& menu, QObject* parent = nullptr);
  ~RecentFilesMenu() override;

  /// Pushes a reader on the recent files stack.
  static void pushDataReader(DataSource* dataSource);
  static void pushStateFile(const QString& filename);

private slots:
  void aboutToShowMenu();
  void dataSourceTriggered(QAction* actn, bool stack = false);
  void stateTriggered();

private:
  Q_DISABLE_COPY(RecentFilesMenu)
};
}
#endif
