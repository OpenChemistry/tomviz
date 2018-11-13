/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CameraReaction.h"

#include <pqRenderView.h>
#include <pqRenderViewSelectionReaction.h>
#include <vtkCamera.h>
#include <vtkPVXMLElement.h>
#include <vtkSMRenderViewProxy.h>

#include "ActiveObjects.h"
#include "Utilities.h"

#include <QMenu>
#include <QToolBar>
#include <QToolButton>

namespace tomviz {

CameraReaction::CameraReaction(QAction* parentObject, CameraReaction::Mode mode)
  : pqReaction(parentObject)
{
  m_reactionMode = mode;
  QObject::connect(&ActiveObjects::instance(),
                   SIGNAL(viewChanged(vtkSMViewProxy*)), this,
                   SLOT(updateEnableState()), Qt::QueuedConnection);
  updateEnableState();
}

void CameraReaction::updateEnableState()
{
  pqView* view = ActiveObjects::instance().activePqView();
  pqRenderView* rview = ActiveObjects::instance().activePqRenderView();
  if (view && m_reactionMode == RESET_CAMERA) {
    parentAction()->setEnabled(true);
  } else if (rview) {
    // Check hints to see if actions should be disabled
    bool cameraResetButtonsEnabled = true;
    vtkPVXMLElement* hints = rview->getHints();
    if (hints) {
      cameraResetButtonsEnabled =
        hints->FindNestedElementByName("DisableCameraToolbarButtons") == NULL;
    }

    parentAction()->setEnabled(cameraResetButtonsEnabled);

  } else {
    parentAction()->setEnabled(false);
  }
}

void CameraReaction::onTriggered()
{
  switch (m_reactionMode) {
    case RESET_CAMERA:
      resetCamera();
      break;
    case RESET_POSITIVE_X:
      resetPositiveX();
      break;
    case RESET_POSITIVE_Y:
      resetPositiveY();
      break;
    case RESET_POSITIVE_Z:
      resetPositiveZ();
      break;
    case RESET_NEGATIVE_X:
      resetNegativeX();
      break;
    case RESET_NEGATIVE_Y:
      resetNegativeY();
      break;
    case RESET_NEGATIVE_Z:
      resetNegativeZ();
      break;
    case ROTATE_CAMERA_CW:
      rotateCamera(90.0);
      break;
    case ROTATE_CAMERA_CCW:
      rotateCamera(-90.0);
      break;
  }
}

void CameraReaction::resetCamera()
{
  pqView* view = ActiveObjects::instance().activePqView();
  if (view)
    view->resetDisplay();
}

void CameraReaction::resetDirection(double look_x, double look_y, double look_z,
                                    double up_x, double up_y, double up_z)
{
  pqRenderView* rview = ActiveObjects::instance().activePqRenderView();
  if (rview)
    rview->resetViewDirection(look_x, look_y, look_z, up_x, up_y, up_z);
}

void CameraReaction::resetPositiveX()
{
  CameraReaction::resetDirection(1, 0, 0, 0, 0, 1);
}

void CameraReaction::resetNegativeX()
{
  CameraReaction::resetDirection(-1, 0, 0, 0, 0, 1);
}

void CameraReaction::resetPositiveY()
{
  CameraReaction::resetDirection(0, 1, 0, 0, 0, 1);
}

void CameraReaction::resetNegativeY()
{
  CameraReaction::resetDirection(0, -1, 0, 0, 0, 1);
}

void CameraReaction::resetPositiveZ()
{
  CameraReaction::resetDirection(0, 0, 1, 0, 1, 0);
}

void CameraReaction::resetNegativeZ()
{
  CameraReaction::resetDirection(0, 0, -1, 0, 1, 0);
}

void CameraReaction::rotateCamera(double angle)
{
  pqRenderView* rview = ActiveObjects::instance().activePqRenderView();

  if (rview) {
    rview->getRenderViewProxy()->GetActiveCamera()->Roll(angle);
    rview->render();
  }
}

void CameraReaction::addAllActionsToToolBar(QToolBar* toolBar)
{
  QAction* resetCamera = toolBar->addAction(
    QIcon(":/pqWidgets/Icons/pqResetCamera.png"), tr("Reset Camera"));
  new CameraReaction(resetCamera, CameraReaction::RESET_CAMERA);

  QAction* zoomToBox = toolBar->addAction(
    QIcon(":/pqWidgets/Icons/pqZoomToSelection.png"), tr("Zoom to Box"));
  zoomToBox->setCheckable(true);
  new pqRenderViewSelectionReaction(zoomToBox, nullptr,
                                    pqRenderViewSelectionReaction::ZOOM_TO_BOX);

  QMenu* menuResetViewDirection =
    new QMenu(tr("Reset view direction"), toolBar);
  QAction* setViewPlusX = menuResetViewDirection->addAction(
    QIcon(":/pqWidgets/Icons/pqXPlus.png"), "+X");
  new CameraReaction(setViewPlusX, CameraReaction::RESET_POSITIVE_X);
  QAction* setViewMinusX = menuResetViewDirection->addAction(
    QIcon(":/pqWidgets/Icons/pqXMinus.png"), "-X");
  new CameraReaction(setViewMinusX, CameraReaction::RESET_NEGATIVE_X);

  QAction* setViewPlusY = menuResetViewDirection->addAction(
    QIcon(":/pqWidgets/Icons/pqYPlus.png"), "+Y");
  new CameraReaction(setViewPlusY, CameraReaction::RESET_POSITIVE_Y);
  QAction* setViewMinusY = menuResetViewDirection->addAction(
    QIcon(":/pqWidgets/Icons/pqYMinus.png"), "-Y");
  new CameraReaction(setViewMinusY, CameraReaction::RESET_NEGATIVE_Y);

  QAction* setViewPlusZ = menuResetViewDirection->addAction(
    QIcon(":/pqWidgets/Icons/pqZPlus.png"), "+Z");
  new CameraReaction(setViewPlusZ, CameraReaction::RESET_POSITIVE_Z);
  QAction* setViewMinusZ = menuResetViewDirection->addAction(
    QIcon(":/pqWidgets/Icons/pqZMinus.png"), "-Z");
  new CameraReaction(setViewMinusZ, CameraReaction::RESET_NEGATIVE_Z);

  QScopedPointer<QToolButton> toolButton(new QToolButton);
  toolButton->setIcon(QIcon(":/pqWidgets/Icons/pqXPlus.png"));
  toolButton->setMenu(menuResetViewDirection);
  toolButton->setToolTip(tr("Reset view direction"));
  toolButton->setPopupMode(QToolButton::InstantPopup);
  toolBar->addWidget(toolButton.take());

  QAction* rotateCameraCW =
    toolBar->addAction(QIcon(":/pqWidgets/Icons/pqRotateCameraCW.png"),
                       tr("Rotate 90° clockwise"));
  new CameraReaction(rotateCameraCW, CameraReaction::ROTATE_CAMERA_CW);

  QAction* rotateCameraCCW =
    toolBar->addAction(QIcon(":/pqWidgets/Icons/pqRotateCameraCCW.png"),
                       tr("Rotate 90° counterclockwise"));
  new CameraReaction(rotateCameraCCW, CameraReaction::ROTATE_CAMERA_CCW);
}

} // namespace tomviz
