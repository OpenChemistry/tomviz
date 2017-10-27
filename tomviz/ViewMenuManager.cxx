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
#include "vtkGridAxes3DActor.h"
#include "vtkProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMViewProxy.h"
#include "vtkTextProperty.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "ScaleLegend.h"
#include "Utilities.h"
#include "ViewPropertiesPanel.h"

namespace tomviz {

ViewMenuManager::ViewMenuManager(QMainWindow* mainWindow, QMenu* menu)
  : pqViewMenuManager(mainWindow, menu), perspectiveProjectionAction(nullptr),
    orthographicProjectionAction(nullptr), scaleLegendCubeAction(nullptr),
    scaleLegendRulerAction(nullptr), hideScaleLegendAction(nullptr)
{
  this->viewPropertiesDialog = new QDialog(mainWindow);
  this->viewPropertiesDialog->setWindowTitle("View Properties");
  ViewPropertiesPanel* panel =
    new ViewPropertiesPanel(this->viewPropertiesDialog);
  QHBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(panel);
  this->viewPropertiesDialog->setLayout(layout);
  this->connect(this->viewPropertiesDialog, SIGNAL(finished(int)),
                SLOT(viewPropertiesDialogHidden()));

  this->showViewPropertiesAction = new QAction("View Properties", this->Menu);
  this->showViewPropertiesAction->setCheckable(true);
  this->connect(this->showViewPropertiesAction, SIGNAL(triggered(bool)),
                SLOT(showViewPropertiesDialog(bool)));
  this->View = ActiveObjects::instance().activeView();
  if (this->View) {
    this->ViewObserverId =
      pqCoreUtilities::connect(this->View, vtkCommand::PropertyModifiedEvent,
                               this, SLOT(onViewPropertyChanged()));
  }
  this->connect(&ActiveObjects::instance(),
                SIGNAL(viewChanged(vtkSMViewProxy*)), SLOT(onViewChanged()));
  this->Menu->addSeparator();
  // Projection modes
  QActionGroup* projectionGroup = new QActionGroup(this);

  this->perspectiveProjectionAction =
    this->Menu->addAction("Perspective Projection");
  this->perspectiveProjectionAction->setCheckable(true);
  this->perspectiveProjectionAction->setActionGroup(projectionGroup);
  this->perspectiveProjectionAction->setChecked(true);
  this->connect(this->perspectiveProjectionAction, SIGNAL(triggered()),
                SLOT(setProjectionModeToPerspective()));
  this->orthographicProjectionAction =
    this->Menu->addAction("Orthographic Projection");
  this->orthographicProjectionAction->setCheckable(true);
  this->orthographicProjectionAction->setActionGroup(projectionGroup);
  this->orthographicProjectionAction->setChecked(false);
  this->connect(this->orthographicProjectionAction, SIGNAL(triggered()),
                SLOT(setProjectionModeToOrthographic()));

  this->Menu->addSeparator();

  this->scaleLegendCubeAction = this->Menu->addAction("Show Legend as Cube");
  this->scaleLegendRulerAction = this->Menu->addAction("Show Legend as Ruler");
  this->hideScaleLegendAction = this->Menu->addAction("Hide Legend");
  this->hideScaleLegendAction->setEnabled(false);

  connect(this->scaleLegendCubeAction, &QAction::triggered, this, [&]() {
    this->setScaleLegendStyle(ScaleLegendStyle::Cube);
    this->setScaleLegendVisibility(true);
    this->hideScaleLegendAction->setEnabled(true);
  });

  connect(this->scaleLegendRulerAction, &QAction::triggered, this, [&]() {
    this->setScaleLegendStyle(ScaleLegendStyle::Ruler);
    this->setScaleLegendVisibility(true);
    this->hideScaleLegendAction->setEnabled(true);
  });

  connect(this->hideScaleLegendAction, &QAction::triggered, this, [&]() {
    this->setScaleLegendVisibility(false);
    this->hideScaleLegendAction->setDisabled(true);
  });

  this->Menu->addSeparator();

  // Show view properties
  this->showViewPropertiesAction = this->Menu->addAction("View Properties");
  this->showViewPropertiesAction->setCheckable(true);
  this->showViewPropertiesAction->setChecked(false);
  this->connect(this->showViewPropertiesAction, SIGNAL(triggered(bool)),
                SLOT(showViewPropertiesDialog(bool)));
}

void ViewMenuManager::showViewPropertiesDialog(bool show)
{
  if (show) {
    this->viewPropertiesDialog->show();
  } else {
    this->viewPropertiesDialog->accept();
  }
}

void ViewMenuManager::viewPropertiesDialogHidden()
{
  this->showViewPropertiesAction->setChecked(false);
}

void ViewMenuManager::setProjectionModeToPerspective()
{
  if (!this->View->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").GetAsInt();
  if (parallel) {
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").Set(0);
    this->View->UpdateVTKObjects();
    pqView* view = tomviz::convert<pqView*>(this->View);
    if (view) {
      view->render();
    }
  }
}

void ViewMenuManager::setProjectionModeToOrthographic()
{
  if (!this->View->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").GetAsInt();
  if (!parallel) {
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").Set(1);
    this->View->UpdateVTKObjects();
    pqView* view = tomviz::convert<pqView*>(this->View);
    if (view) {
      view->render();
    }
  }
}

void ViewMenuManager::onViewPropertyChanged()
{
  if (!this->perspectiveProjectionAction ||
      !this->orthographicProjectionAction) {
    return;
  }
  if (!this->View->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(this->View, "CameraParallelProjection").GetAsInt();
  if (parallel && this->perspectiveProjectionAction->isChecked()) {
    this->orthographicProjectionAction->setChecked(true);
  } else if (!parallel && this->orthographicProjectionAction->isChecked()) {
    this->perspectiveProjectionAction->setChecked(true);
  }
}

void ViewMenuManager::onViewChanged()
{
  this->View = ActiveObjects::instance().activeView();
  if (this->View) {
    this->ViewObserverId =
      pqCoreUtilities::connect(this->View, vtkCommand::PropertyModifiedEvent,
                               this, SLOT(onViewPropertyChanged()));
    if (this->View->GetProperty("AxesGrid")) {
      vtkSMPropertyHelper axesGridProp(this->View, "AxesGrid");
      vtkSMProxy* proxy = axesGridProp.GetAsProxy();
      if (!proxy) {
        vtkSMSessionProxyManager* pxm = this->View->GetSessionProxyManager();
        proxy = pxm->NewProxy("annotations", "GridAxes3DActor");
        axesGridProp.Set(proxy);
        this->View->UpdateVTKObjects();
        proxy->Delete();
      }
    }
  }
  bool enableProjectionModes =
    (this->View && this->View->GetProperty("CameraParallelProjection"));
  // We have to check since this can be called before buildMenu
  if (this->orthographicProjectionAction && this->perspectiveProjectionAction) {
    this->orthographicProjectionAction->setEnabled(enableProjectionModes);
    this->perspectiveProjectionAction->setEnabled(enableProjectionModes);
  }
}
}
