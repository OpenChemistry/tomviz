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

#include <pqActiveObjects.h>
#include <pqPipelineRepresentation.h>
#include <pqRenderView.h>

#include <vtkCamera.h>
#include <vtkPVXMLElement.h>
#include <vtkSMRenderViewProxy.h>

//-----------------------------------------------------------------------------
CameraReaction::CameraReaction(QAction* parentObject, CameraReaction::Mode mode)
  : Superclass(parentObject)
{
  this->ReactionMode = mode;
  QObject::connect(&pqActiveObjects::instance(), SIGNAL(viewChanged(pqView*)),
                   this, SLOT(updateEnableState()), Qt::QueuedConnection);
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void CameraReaction::updateEnableState()
{
  pqView* view = pqActiveObjects::instance().activeView();
  pqRenderView* rview = qobject_cast<pqRenderView*>(view);
  if (view && this->ReactionMode == RESET_CAMERA) {
    this->parentAction()->setEnabled(true);
  } else if (rview) {
    // Check hints to see if actions should be disabled
    bool cameraResetButtonsEnabled = true;
    vtkPVXMLElement* hints = rview->getHints();
    if (hints) {
      cameraResetButtonsEnabled =
        hints->FindNestedElementByName("DisableCameraToolbarButtons") == NULL;
    }

    this->parentAction()->setEnabled(cameraResetButtonsEnabled);

  } else {
    this->parentAction()->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------
void CameraReaction::onTriggered()
{
  switch (this->ReactionMode) {
    case RESET_CAMERA:
      this->resetCamera();
      break;

    case RESET_POSITIVE_X:
      this->resetPositiveX();
      break;

    case RESET_POSITIVE_Y:
      this->resetPositiveY();
      break;

    case RESET_POSITIVE_Z:
      this->resetPositiveZ();
      break;

    case RESET_NEGATIVE_X:
      this->resetNegativeX();
      break;

    case RESET_NEGATIVE_Y:
      this->resetNegativeY();
      break;

    case RESET_NEGATIVE_Z:
      this->resetNegativeZ();
      break;

    case ROTATE_CAMERA_CW:
      this->rotateCamera(90.0);
      break;

    case ROTATE_CAMERA_CCW:
      this->rotateCamera(-90.0);
      break;
  }
}

//-----------------------------------------------------------------------------
void CameraReaction::resetCamera()
{
  pqView* view = pqActiveObjects::instance().activeView();
  if (view) {
    view->resetDisplay();
  }
}

//-----------------------------------------------------------------------------
void CameraReaction::resetDirection(double look_x, double look_y, double look_z,
                                    double up_x, double up_y, double up_z)
{
  pqRenderView* ren =
    qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
  if (ren) {
    ren->resetViewDirection(look_x, look_y, look_z, up_x, up_y, up_z);
  }
}

//-----------------------------------------------------------------------------
void CameraReaction::resetPositiveX()
{
  CameraReaction::resetDirection(1, 0, 0, 0, 0, 1);
}

//-----------------------------------------------------------------------------
void CameraReaction::resetNegativeX()
{
  CameraReaction::resetDirection(-1, 0, 0, 0, 0, 1);
}

//-----------------------------------------------------------------------------
void CameraReaction::resetPositiveY()
{
  CameraReaction::resetDirection(0, 1, 0, 0, 0, 1);
}

//-----------------------------------------------------------------------------
void CameraReaction::resetNegativeY()
{
  CameraReaction::resetDirection(0, -1, 0, 0, 0, 1);
}

//-----------------------------------------------------------------------------
void CameraReaction::resetPositiveZ()
{
  CameraReaction::resetDirection(0, 0, 1, 0, 1, 0);
}

//-----------------------------------------------------------------------------
void CameraReaction::resetNegativeZ()
{
  CameraReaction::resetDirection(0, 0, -1, 0, 1, 0);
}

//-----------------------------------------------------------------------------
void CameraReaction::rotateCamera(double angle)
{
  pqRenderView* renModule =
    qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());

  if (renModule) {
    renModule->getRenderViewProxy()->GetActiveCamera()->Roll(angle);
    renModule->render();
  }
}
