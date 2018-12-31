/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAxesReaction_h
#define tomvizAxesReaction_h

#include <pqReaction.h>

class QToolBar;

namespace tomviz {

/**
 * AxesReaction handles the logic for setting the center rotation axes,
 * toggling its visibility etc.
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
   * Updates the enabled state. Applications need not explicitly call this.
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
