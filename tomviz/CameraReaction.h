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
#ifndef tomvizCameraReaction_h
#define tomvizCameraReaction_h

#include <pqReaction.h>

namespace tomviz {

/**
 * @ingroup Reactions
 * CameraReaction has the logic to handle common operations associated with
 * the camera such as reset view along X axis etc.
 *
 * This class was adapted from pqCameraReaction in ParaView.
 */
class CameraReaction : public pqReaction
{
  Q_OBJECT

public:
  enum Mode
  {
    RESET_CAMERA,
    RESET_POSITIVE_X,
    RESET_POSITIVE_Y,
    RESET_POSITIVE_Z,
    RESET_NEGATIVE_X,
    RESET_NEGATIVE_Y,
    RESET_NEGATIVE_Z,
    ROTATE_CAMERA_CW,
    ROTATE_CAMERA_CCW
  };

  CameraReaction(QAction* parent, Mode mode);

  static void resetCamera();
  static void resetPositiveX();
  static void resetPositiveY();
  static void resetPositiveZ();
  static void resetNegativeX();
  static void resetNegativeY();
  static void resetNegativeZ();
  static void resetDirection(double look_x, double look_y, double look_z,
                             double up_x, double up_y, double up_z);
  static void rotateCamera(double angle);

public slots:
  /**
   * Updates the enabled state. Applications need not explicitly call
   * this.
   */
  void updateEnableState() override;

protected:
  /**
   * Called when the action is triggered.
   */
  void onTriggered() override;

private:
  Q_DISABLE_COPY(CameraReaction)
  Mode m_reactionMode;
};
} // namespace tomviz

#endif
