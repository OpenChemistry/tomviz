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
#ifndef tomvizViewMenuManager_h
#define tomvizViewMenuManager_h

#include <pqViewMenuManager.h>

class QDialog;
class QAction;

class vtkSMViewProxy;

namespace tomviz
{

class ViewMenuManager : public pqViewMenuManager
{
  Q_OBJECT
public:
  ViewMenuManager(QMainWindow* mainWindow, QMenu* menu);

protected:
  // Override to add 'show View Properties dialog'
  void buildMenu() override;

private slots:
  void showViewPropertiesDialog(bool show);
  void viewPropertiesDialogHidden();

  void setProjectionModeToPerspective();
  void setProjectionModeToOrthographic();
  void onViewPropertyChanged();
  void onViewChanged();
  void setShowAxisGrid(bool show);
  void onAxesGridChanged();

private:
  QDialog* viewPropertiesDialog;
  QAction* showViewPropertiesAction;
  QAction* perspectiveProjectionAction;
  QAction* orthographicProjectionAction;
  QAction* showAxisGridAction;

  vtkSMViewProxy *View;
  unsigned long ViewObserverId;
  unsigned long AxesGridObserverId;
};

}

#endif
