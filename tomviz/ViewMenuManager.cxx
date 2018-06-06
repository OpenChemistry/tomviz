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

#include <pqCoreUtilities.h>
#include <pqView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <vtkCommand.h>
#include <vtkGridAxes3DActor.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "ScaleLegend.h"
#include "Utilities.h"

namespace tomviz {

ViewMenuManager::ViewMenuManager(QMainWindow* mainWindow, QMenu* menu)
  : pqViewMenuManager(mainWindow, menu), m_perspectiveProjectionAction(nullptr),
    m_orthographicProjectionAction(nullptr), m_scaleLegendCubeAction(nullptr),
    m_scaleLegendRulerAction(nullptr), m_hideScaleLegendAction(nullptr)
{
  m_view = ActiveObjects::instance().activeView();
  if (m_view) {
    m_viewObserverId =
      pqCoreUtilities::connect(m_view, vtkCommand::PropertyModifiedEvent, this,
                               SLOT(onViewPropertyChanged()));
  }
  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          SLOT(onViewChanged()));
  Menu->addSeparator();
  // Projection modes
  QActionGroup* projectionGroup = new QActionGroup(this);

  m_perspectiveProjectionAction = Menu->addAction("Perspective Projection");
  m_perspectiveProjectionAction->setCheckable(true);
  m_perspectiveProjectionAction->setActionGroup(projectionGroup);
  m_perspectiveProjectionAction->setChecked(true);
  connect(m_perspectiveProjectionAction, SIGNAL(triggered()),
          SLOT(setProjectionModeToPerspective()));
  m_orthographicProjectionAction = Menu->addAction("Orthographic Projection");
  m_orthographicProjectionAction->setCheckable(true);
  m_orthographicProjectionAction->setActionGroup(projectionGroup);
  m_orthographicProjectionAction->setChecked(false);
  connect(m_orthographicProjectionAction, SIGNAL(triggered()),
          SLOT(setProjectionModeToOrthographic()));

  Menu->addSeparator();

  m_showAxesGridAction = Menu->addAction("Show Axes Grid");
  m_showAxesGridAction->setCheckable(true);
  m_showAxesGridAction->setChecked(false);
  connect(m_showAxesGridAction, &QAction::triggered, this,
          &ViewMenuManager::setShowAxesGrid);
  m_showCenterAxesAction = Menu->addAction("Show Center Axes");
  m_showCenterAxesAction->setCheckable(true);
  m_showCenterAxesAction->setChecked(false);
  connect(m_showCenterAxesAction, &QAction::triggered, this,
          &ViewMenuManager::setShowCenterAxes);
  m_showOrientationAxesAction = Menu->addAction("Show Orientation Axes");
  m_showOrientationAxesAction->setCheckable(true);
  m_showOrientationAxesAction->setChecked(true);
  connect(m_showOrientationAxesAction, &QAction::triggered, this,
          &ViewMenuManager::setShowOrientationAxes);

  Menu->addSeparator();

  m_scaleLegendCubeAction = Menu->addAction("Show Legend as Cube");
  m_scaleLegendCubeAction->setCheckable(true);
  m_scaleLegendRulerAction = Menu->addAction("Show Legend as Ruler");
  m_scaleLegendRulerAction->setCheckable(true);
  m_hideScaleLegendAction = Menu->addAction("Hide Legend");
  m_hideScaleLegendAction->setEnabled(false);

  connect(m_scaleLegendCubeAction, &QAction::triggered, this, [&]() {
    setScaleLegendStyle(ScaleLegendStyle::Cube);
    setScaleLegendVisibility(true);
    m_scaleLegendCubeAction->setChecked(true);
    m_scaleLegendRulerAction->setChecked(false);
    m_hideScaleLegendAction->setEnabled(true);
  });

  connect(m_scaleLegendRulerAction, &QAction::triggered, this, [&]() {
    setScaleLegendStyle(ScaleLegendStyle::Ruler);
    setScaleLegendVisibility(true);
    m_scaleLegendCubeAction->setChecked(false);
    m_scaleLegendRulerAction->setChecked(true);
    m_hideScaleLegendAction->setEnabled(true);
  });

  connect(m_hideScaleLegendAction, &QAction::triggered, this, [&]() {
    setScaleLegendVisibility(false);
    m_hideScaleLegendAction->setDisabled(true);
    m_scaleLegendCubeAction->setChecked(false);
    m_scaleLegendRulerAction->setChecked(false);
  });

  Menu->addSeparator();
}

ViewMenuManager::~ViewMenuManager()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
  }
}

void ViewMenuManager::setProjectionModeToPerspective()
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (parallel) {
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").Set(0);
    m_view->UpdateVTKObjects();
    pqView* view = tomviz::convert<pqView*>(m_view);
    if (view) {
      view->render();
    }
  }
}

void ViewMenuManager::setProjectionModeToOrthographic()
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (!parallel) {
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").Set(1);
    m_view->UpdateVTKObjects();
    pqView* view = tomviz::convert<pqView*>(m_view);
    if (view) {
      view->render();
    }
  }
}

void ViewMenuManager::onViewPropertyChanged()
{
  if (!m_perspectiveProjectionAction || !m_orthographicProjectionAction) {
    return;
  }
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (parallel && m_perspectiveProjectionAction->isChecked()) {
    m_orthographicProjectionAction->setChecked(true);
  } else if (!parallel && m_orthographicProjectionAction->isChecked()) {
    m_perspectiveProjectionAction->setChecked(true);
  }
}

void ViewMenuManager::onViewChanged()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
  }
  m_view = ActiveObjects::instance().activeView();
  if (m_view) {
    m_viewObserverId =
      pqCoreUtilities::connect(m_view, vtkCommand::PropertyModifiedEvent, this,
                               SLOT(onViewPropertyChanged()));
    if (m_view->GetProperty("AxesGrid")) {
      vtkSMPropertyHelper axesGridProp(m_view, "AxesGrid");
      vtkSMProxy* proxy = axesGridProp.GetAsProxy();
      if (!proxy) {
        vtkSMSessionProxyManager* pxm = m_view->GetSessionProxyManager();
        proxy = pxm->NewProxy("annotations", "GridAxes3DActor");
        axesGridProp.Set(proxy);
        m_view->UpdateVTKObjects();
        proxy->Delete();
      }
      vtkSMPropertyHelper visibilityHelper(proxy, "Visibility");
      m_showAxesGridAction->setChecked(visibilityHelper.GetAsInt() == 1);
      m_showAxesGridAction->setEnabled(true);
    } else {
      m_showAxesGridAction->setChecked(false);
      m_showAxesGridAction->setEnabled(false);
    }
  }
  auto scaleLegend = ScaleLegend::getScaleLegend(m_view);
  if (scaleLegend && scaleLegend->visible()) {
    m_scaleLegendCubeAction->setChecked(scaleLegend->style() ==
                                        ScaleLegendStyle::Cube);
    m_scaleLegendRulerAction->setChecked(scaleLegend->style() ==
                                         ScaleLegendStyle::Ruler);
    m_hideScaleLegendAction->setChecked(false);
    m_hideScaleLegendAction->setEnabled(true);
  } else {
    m_scaleLegendCubeAction->setChecked(false);
    m_scaleLegendRulerAction->setChecked(false);
    m_hideScaleLegendAction->setChecked(true);
    m_hideScaleLegendAction->setEnabled(false);
  }
  bool enableProjectionModes =
    (m_view && m_view->GetProperty("CameraParallelProjection"));
  m_orthographicProjectionAction->setEnabled(enableProjectionModes);
  m_perspectiveProjectionAction->setEnabled(enableProjectionModes);
  if (enableProjectionModes) {
    vtkSMPropertyHelper parallelProjection(m_view, "CameraParallelProjection");
    m_orthographicProjectionAction->setChecked(parallelProjection.GetAsInt() ==
                                               1);
    m_perspectiveProjectionAction->setChecked(parallelProjection.GetAsInt() !=
                                              1);
  }
  bool enableCenterAxes = m_view && m_view->GetProperty("CenterAxesVisibility");
  bool enableOrientationAxes =
    m_view && m_view->GetProperty("OrientationAxesVisibility");
  m_showCenterAxesAction->setEnabled(enableCenterAxes);
  m_showOrientationAxesAction->setEnabled(enableOrientationAxes);
  if (enableCenterAxes) {
    vtkSMPropertyHelper showCenterAxes(m_view, "CenterAxesVisibility");
    m_showCenterAxesAction->setChecked(showCenterAxes.GetAsInt() == 1);
  }
  if (enableOrientationAxes) {
    vtkSMPropertyHelper showOrientationAxes(m_view,
                                            "OrientationAxesVisibility");
    m_showOrientationAxesAction->setChecked(showOrientationAxes.GetAsInt() ==
                                            1);
  }
}

void ViewMenuManager::setShowAxesGrid(bool show)
{
  if (!m_view || !m_view->GetProperty("AxesGrid")) {
    return;
  }
  vtkSMPropertyHelper axesGridProp(m_view, "AxesGrid");
  auto axesGridProxy = axesGridProp.GetAsProxy();
  vtkSMPropertyHelper visibility(axesGridProxy, "Visibility");
  visibility.Set(show ? 1 : 0);
  axesGridProxy->UpdateVTKObjects();
  pqView* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

void ViewMenuManager::setShowCenterAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("CenterAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "CenterAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  pqView* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

void ViewMenuManager::setShowOrientationAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("OrientationAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "OrientationAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  pqView* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

void ViewMenuManager::setScaleLegendStyle(ScaleLegendStyle s)
{
  if (!m_view) {
    return;
  }
  auto scaleLegend = ScaleLegend::getScaleLegend(m_view);
  if (!scaleLegend) {
    return;
  }
  scaleLegend->setStyle(s);
}

void ViewMenuManager::setScaleLegendVisibility(bool v)
{
  if (!m_view) {
    return;
  }
  auto scaleLegend = ScaleLegend::getScaleLegend(m_view);
  if (!scaleLegend) {
    return;
  }
  scaleLegend->setVisibility(v);
}

} // namespace tomviz
