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
#ifndef tomvizAxesReaction_h
#define tomvizAxesReaction_h

#include <pqReaction.h>

class QToolBar;

namespace tomviz {

/**
 * AxesReaction handles the logic for setting the center
 * rotation axes, toggling its visibility etc.
 *
 * This class was adapted from the "pqAxesToolbar" and "pqCameraReaction"
 * classes in ParaView.
 */
class AxesReaction : public pqReaction
{
  Q_OBJECT

public:
  enum Mode
  {
    SHOW_ORIENTATION_AXES,
    SHOW_CENTER_AXES,
    RESET_CENTER,
    PICK_CENTER
  };

  AxesReaction(QAction* parent, Mode mode);

  static void addAllActionsToToolBar(QToolBar* toolBar);

public slots:

  static void showCenterAxes(bool);
  static void showOrientationAxes(bool);
  static void resetCenterOfRotationToCenterOfCurrentData();
  static void pickCenterOfRotation(int, int);

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
  Q_DISABLE_COPY(AxesReaction)
  Mode m_reactionMode;
};

} // namespace tomviz

#endif
