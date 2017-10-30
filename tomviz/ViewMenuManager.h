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

#include <QPointer>

class QDialog;
class QAction;

class vtkSMViewProxy;

namespace tomviz {

enum class ScaleLegendStyle : unsigned int;

class ViewMenuManager : public pqViewMenuManager
{
  Q_OBJECT
public:
  ViewMenuManager(QMainWindow* mainWindow, QMenu* menu);
  ~ViewMenuManager();

signals:
  void setScaleLegendStyle(ScaleLegendStyle);
  void setScaleLegendVisibility(bool);

private slots:
  void showViewPropertiesDialog(bool show);
  void viewPropertiesDialogHidden();

  void setProjectionModeToPerspective();
  void setProjectionModeToOrthographic();
  void onViewPropertyChanged();
  void onViewChanged();

private:
  QDialog* viewPropertiesDialog;
  QPointer<QAction> showViewPropertiesAction;
  QPointer<QAction> perspectiveProjectionAction;
  QPointer<QAction> orthographicProjectionAction;
  QPointer<QAction> scaleLegendCubeAction;
  QPointer<QAction> scaleLegendRulerAction;
  QPointer<QAction> hideScaleLegendAction;

  vtkSMViewProxy* View;
  unsigned long ViewObserverId;
};
}

#endif
