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
#include "CameraReaction.h"

#include <pqRenderView.h>

#include <vtkCamera.h>
#include <vtkPVXMLElement.h>
#include <vtkSMRenderViewProxy.h>

#include "ActiveObjects.h"
#include "Utilities.h"

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
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  auto rview = qobject_cast<pqRenderView*>(view);
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
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
    view->resetDisplay();
}

void CameraReaction::resetDirection(double look_x, double look_y, double look_z,
                                    double up_x, double up_y, double up_z)
{
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  auto ren = qobject_cast<pqRenderView*>(view);
  if (ren)
    ren->resetViewDirection(look_x, look_y, look_z, up_x, up_y, up_z);
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
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  auto ren = qobject_cast<pqRenderView*>(view);

  if (ren) {
    ren->getRenderViewProxy()->GetActiveCamera()->Roll(angle);
    ren->render();
  }
}

} // namespace tomviz
