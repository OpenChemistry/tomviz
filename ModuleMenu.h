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
#ifndef __ModuleMenu_h
#define __ModuleMenu_h

#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
class QToolBar;

namespace TEM
{

/// ModuleMenu is manager for the Modules menu. It fills it up with actions
/// and handles their triggers based on available modules reported by
/// ModuleFactory.
class ModuleMenu : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  ModuleMenu(QToolBar* toolBar, QMenu* parentMenu, QObject* parent=NULL);
  virtual ~ModuleMenu();

private slots:
  void updateActions();
  void triggered(QAction* maction);

private:
  Q_DISABLE_COPY(ModuleMenu);
  QPointer<QMenu> Menu;
  QPointer<QToolBar> ToolBar;
};

}

#endif
