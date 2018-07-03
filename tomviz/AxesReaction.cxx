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
#include "AxesReaction.h"

#include <ActiveObjects.h>

#include <pqRenderView.h>
#include <pqRenderViewSelectionReaction.h>
#include <vtkSMRenderViewProxy.h>

#include <QToolBar>

namespace tomviz {

AxesReaction::AxesReaction(QAction* parentObject, AxesReaction::Mode mode)
  : pqReaction(parentObject)
{
  m_reactionMode = mode;

  QObject::connect(&ActiveObjects::instance(),
                   SIGNAL(viewChanged(vtkSMViewProxy*)), this,
                   SLOT(updateEnableState()), Qt::QueuedConnection);

  QObject::connect(&ActiveObjects::instance(),
                   SIGNAL(dataSourceChanged(DataSource*)), this,
                   SLOT(updateEnableState()));

  switch (m_reactionMode) {
    case SHOW_ORIENTATION_AXES:
      QObject::connect(parentObject, SIGNAL(toggled(bool)), this,
                       SLOT(showOrientationAxes(bool)));
      break;
    case SHOW_CENTER_AXES:
      QObject::connect(parentObject, SIGNAL(toggled(bool)), this,
                       SLOT(showCenterAxes(bool)));
      break;
    case PICK_CENTER: {
      auto selectionReaction = new pqRenderViewSelectionReaction(
        parentObject, nullptr,
        pqRenderViewSelectionReaction::SELECT_CUSTOM_BOX);
      QObject::connect(selectionReaction,
                       SIGNAL(selectedCustomBox(int, int, int, int)), this,
                       SLOT(pickCenterOfRotation(int, int)));
    } break;
    default:
      break;
  }

  updateEnableState();
}

void AxesReaction::onTriggered()
{
  switch (m_reactionMode) {
    case RESET_CENTER:
      resetCenterOfRotationToCenterOfCurrentData();
      break;
    default:
      break;
  }
}

void AxesReaction::updateEnableState()
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();

  switch (m_reactionMode) {
    case SHOW_ORIENTATION_AXES:
      parentAction()->setEnabled(renderView != NULL);
      parentAction()->blockSignals(true);
      parentAction()->setChecked(
        renderView ? renderView->getOrientationAxesVisibility() : false);
      parentAction()->blockSignals(false);
      break;
    case SHOW_CENTER_AXES:
      parentAction()->setEnabled(renderView != NULL);
      parentAction()->blockSignals(true);
      parentAction()->setChecked(
        renderView ? renderView->getCenterAxesVisibility() : false);
      parentAction()->blockSignals(false);
      break;
    case RESET_CENTER:
      parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                                 NULL);
      break;
    default:
      break;
  }
}

void AxesReaction::showOrientationAxes(bool show_axes)
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();

  if (!renderView)
    return;

  renderView->setOrientationAxesVisibility(show_axes);
  renderView->render();
}

void AxesReaction::showCenterAxes(bool show_axes)
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();

  if (!renderView)
    return;

  renderView->setCenterAxesVisibility(show_axes);
  renderView->render();
}

void AxesReaction::resetCenterOfRotationToCenterOfCurrentData()
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();
  DataSource* dataSource = ActiveObjects::instance().activeDataSource();

  if (!renderView || !dataSource) {
    // qDebug() << "Active source not shown in active view. Cannot set center.";
    return;
  }

  double bounds[6];
  dataSource->getBounds(bounds);
  double center[3];
  center[0] = (bounds[1] + bounds[0]) / 2.0;
  center[1] = (bounds[3] + bounds[2]) / 2.0;
  center[2] = (bounds[5] + bounds[4]) / 2.0;
  renderView->setCenterOfRotation(center);
  renderView->render();
}

void AxesReaction::pickCenterOfRotation(int posx, int posy)
{
  pqRenderView* renderView = ActiveObjects::instance().activePqRenderView();
  if (renderView) {
    int posxy[2] = { posx, posy };
    double center[3];

    vtkSMRenderViewProxy* proxy = renderView->getRenderViewProxy();
    if (proxy->ConvertDisplayToPointOnSurface(posxy, center)) {
      renderView->setCenterOfRotation(center);
      renderView->render();
    }
  }
}

void AxesReaction::addAllActionsToToolBar(QToolBar* toolBar)
{
  QAction* showOrientationAxesAction =
    toolBar->addAction(QIcon(":pqWidgets/Icons/pqShowOrientationAxes.png"),
                       "Show Orientation Axes");
  showOrientationAxesAction->setCheckable(true);
  new AxesReaction(showOrientationAxesAction,
                   AxesReaction::SHOW_ORIENTATION_AXES);
  QAction* showCenterAxesAction = toolBar->addAction(
    QIcon(":pqWidgets/Icons/pqShowCenterAxes.png"), "Show Center Axes");
  showCenterAxesAction->setCheckable(true);
  new AxesReaction(showCenterAxesAction, AxesReaction::SHOW_CENTER_AXES);
  QAction* resetCenterAction = toolBar->addAction(
    QIcon(":pqWidgets/Icons/pqResetCenter.png"), "Reset Center");
  new AxesReaction(resetCenterAction, AxesReaction::RESET_CENTER);
  QAction* pickCenterAction = toolBar->addAction(
    QIcon(":pqWidgets/Icons/pqPickCenter.png"), "Pick Center");
  pickCenterAction->setCheckable(true);
  new AxesReaction(pickCenterAction, AxesReaction::PICK_CENTER);
}

} // namespace tomviz
