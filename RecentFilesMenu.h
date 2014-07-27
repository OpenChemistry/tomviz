/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __RecentFilesMenu_h
#define __RecentFilesMenu_h

#include <QObject>
#include <QAction>

class QMenu;
class vtkSMProxy;

namespace TEM
{

/// Extends pqRecentFilesMenu to add support to open a data file customized for
/// TomViz.
class RecentFilesMenu : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  RecentFilesMenu(QMenu& menu, QObject* parent=NULL);
  virtual ~RecentFilesMenu();

  /// Pushes a reader on the recent files stack.
  static void pushDataReader(vtkSMProxy* readerProxy);
  static void pushStateFile(const QString& filename);

private slots:
  void aboutToShowMenu();
  void dataSourceTriggered();
  void stateTriggered();

private:
  Q_DISABLE_COPY(RecentFilesMenu)
};

}
#endif
