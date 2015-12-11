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

#include "ViewMenuManager.h"

#include "pqCoreUtilities.h"
#include "pqView.h"
#include "vtkCommand.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMViewProxy.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "Utilities.h"
#include "ViewPropertiesPanel.h"

namespace tomviz
{

ViewMenuManager::ViewMenuManager(QMainWindow* mainWindow, QMenu* menu)
  : pqViewMenuManager(mainWindow, menu)
{
  this->viewPropertiesDialog = new QDialog(mainWindow);
  this->viewPropertiesDialog->setWindowTitle("View Properties");
  ViewPropertiesPanel* panel = new ViewPropertiesPanel(this->viewPropertiesDialog);
  QHBoxLayout *layout = new QHBoxLayout;
  layout->addWidget(panel);
  this->viewPropertiesDialog->setLayout(layout);
  this->connect(this->viewPropertiesDialog, SIGNAL(finished(int)),
                    SLOT(viewPropertiesDialogHidden()));

  this->showViewPropertiesAction = new QAction("View Properties", this->Menu);
  this->showViewPropertiesAction->setCheckable(true);
  this->connect(this->showViewPropertiesAction, SIGNAL(triggered(bool)),
                    SLOT(showViewPropertiesDialog(bool)));
  this->View = ActiveObjects::instance().activeView();
  if (this->View)
  {
    this->ViewObserverId = pqCoreUtilities::connect(
        this->View, vtkCommand::PropertyModifiedEvent,
        this, SLOT(onViewPropertyChanged()));
  }
  this->connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(onViewChanged()));
}

void ViewMenuManager::buildMenu()
{
  bool showViewPropertiesChecked = this->showViewPropertiesAction->isChecked();
  bool perspectiveProjectionChecked = true;
  if (this->perspectiveProjectionAction)
  {
    perspectiveProjectionChecked = this->perspectiveProjectionAction->isChecked();
  }
  this->showViewPropertiesAction = nullptr; // The object is about to be deleted
  this->perspectiveProjectionAction = nullptr;
  this->orthographicProjectionAction = nullptr;
  pqViewMenuManager::buildMenu(); // deletes all prior menu items and repopulates menu

  // Projection modes
  QMenu *projectionMenu = this->Menu->addMenu("Projection Mode");
  QActionGroup *projectionGroup = new QActionGroup(this);

  this->perspectiveProjectionAction = projectionMenu->addAction("Perspective");
  this->perspectiveProjectionAction->setCheckable(true);
  this->perspectiveProjectionAction->setActionGroup(projectionGroup);
  this->perspectiveProjectionAction->setChecked(perspectiveProjectionChecked);
  this->connect(this->perspectiveProjectionAction, SIGNAL(triggered()),
                SLOT(setProjectionModeToPerspective()));
  this->orthographicProjectionAction = projectionMenu->addAction("Orthographic");
  this->orthographicProjectionAction->setCheckable(true);
  this->orthographicProjectionAction->setActionGroup(projectionGroup);
  this->orthographicProjectionAction->setChecked(!perspectiveProjectionChecked);
  this->connect(this->orthographicProjectionAction, SIGNAL(triggered()),
                SLOT(setProjectionModeToOrthographic()));

  // Show view properties
  this->showViewPropertiesAction = new QAction("View Properties", this->Menu);
  this->showViewPropertiesAction->setCheckable(true);
  this->showViewPropertiesAction->setChecked(showViewPropertiesChecked);
  this->connect(this->showViewPropertiesAction, SIGNAL(triggered(bool)),
                    SLOT(showViewPropertiesDialog(bool)));
  this->Menu->addSeparator();
  this->Menu->addAction(this->showViewPropertiesAction);
}

void ViewMenuManager::showViewPropertiesDialog(bool show)
{
  if (show)
  {
    this->viewPropertiesDialog->show();
  }
  else
  {
    this->viewPropertiesDialog->accept();
  }
}

void ViewMenuManager::viewPropertiesDialogHidden()
{
  this->showViewPropertiesAction->setChecked(false);
}

void ViewMenuManager::setProjectionModeToPerspective()
{
  int parallel = vtkSMPropertyHelper(this->View, "CameraParallelProjection").GetAsInt();
  if (parallel)
  {
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").Set(0);
    this->View->UpdateVTKObjects();
    pqView* view = tomviz::convert<pqView*>(this->View);
    if (view)
    {
      view->render();
    }
  }
}

void ViewMenuManager::setProjectionModeToOrthographic()
{
  int parallel = vtkSMPropertyHelper(this->View, "CameraParallelProjection").GetAsInt();
  if (!parallel)
  {
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").Set(1);
    this->View->UpdateVTKObjects();
    pqView* view = tomviz::convert<pqView*>(this->View);
    if (view)
    {
      view->render();
    }
  }
}

void ViewMenuManager::onViewPropertyChanged()
{
  int parallel = vtkSMPropertyHelper(this->View, "CameraParallelProjection").GetAsInt();
  if (!this->perspectiveProjectionAction)
  {
    return;
  }
  if (parallel && this->perspectiveProjectionAction->isChecked())
  {
    this->orthographicProjectionAction->setChecked(true);
  }
  else if (!parallel && this->orthographicProjectionAction->isChecked())
  {
    this->perspectiveProjectionAction->setChecked(true);
  }
}

void ViewMenuManager::onViewChanged()
{
  if (this->View)
  {
    this->View->RemoveObserver(this->ViewObserverId);
  }
  this->View = ActiveObjects::instance().activeView();
  if (this->View)
  {
    this->ViewObserverId = pqCoreUtilities::connect(
        this->View, vtkCommand::PropertyModifiedEvent,
        this, SLOT(onViewPropertyChanged()));
  }
}

}
