/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCameraViewpoints_h
#define tomvizCameraViewpoints_h

#include "SceneSnapshot.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class pqRenderView;
class vtkCamera;
class vtkCameraInterpolator;

namespace tomviz {

/// A saved camera position, plus how an animation leaves it for the next
/// one.
struct Viewpoint
{
  std::array<double, 3> position = { 0, 0, 1 };
  std::array<double, 3> focalPoint = { 0, 0, 0 };
  std::array<double, 3> viewUp = { 0, 1, 0 };
  double viewAngle = 30.0;
  double parallelScale = 1.0;
  bool parallelProjection = false;

  /// Frames the leg leaving this viewpoint takes. The animation is as
  /// long as its legs and orbits add up to, so lengthening one leg
  /// never shortens another. The last viewpoint has no leg leaving it,
  /// so its value is unused.
  int legFrames = 100;

  /// Ease in and out of that segment, so the camera slows to a stop at
  /// each end instead of rounding the corner at full speed.
  bool eased = true;

  /// Full turns the camera makes around the focal point once it has
  /// arrived here, before the leg to the next viewpoint; 0 is none.
  /// Positive turns counterclockwise as seen from above (looking down
  /// this viewpoint's view-up), negative clockwise. The last viewpoint
  /// can orbit too, which ends the animation on a spin.
  int orbitTurns = 0;

  /// Frames that orbit takes. Unused when orbitTurns is 0.
  int orbitFrames = 120;

  /// Ease in and out of that orbit. Off by default: a spin at constant
  /// speed loops without a visible stop, as tomviz's camera orbit always
  /// has. Separate from eased, which is for the leg.
  bool orbitEased = false;

  /// What the user calls this viewpoint. Stable: renumbering on every
  /// delete would silently repoint anything that refers to viewpoints by
  /// name, so default names never reuse a number.
  QString name;

  /// A small PNG of the view this was saved from. "Viewpoint 3" says
  /// nothing about which view it is; the picture does. Captured when the
  /// viewpoint is saved, because it cannot be regenerated later without
  /// moving the camera there and back.
  QByteArray thumbnail;

  /// Caption shown in the render view (and exported movies) while the
  /// path is at this viewpoint and on the leg leaving it. Empty = none.
  QString label;

  /// Module state recorded with this viewpoint (visibility, opacity,
  /// volume curve, cut-out, exploded view). RecordedAnimations turns
  /// the differences between viewpoints into animations and rows in the
  /// Animation Helper.
  SceneSnapshot scene;

  void readFrom(vtkCamera* camera);
  void applyTo(vtkCamera* camera) const;

  QJsonObject serialize() const;
  static Viewpoint deserialize(const QJsonObject& json);
};

/// Put @a camera @a u of the way (0 to 1) through the orbit at
/// @a viewpoint: its position swung around the focal point on the
/// viewpoint's view-up axis, through viewpoint.orbitTurns full turns,
/// with everything else as saved. It starts and ends at the viewpoint
/// itself. A camera sitting on the axis has nothing to swing and stays
/// put.
void orbitAt(const Viewpoint& viewpoint, double u, vtkCamera* camera);

/// The camera viewpoints saved for the current session, and the timing
/// that turns them into a path.
///
/// The list outlives the Animation Helper dialog, which is created once
/// and only hidden, and is saved with the state file, so it lives here
/// rather than in the dialog.
class CameraViewpoints : public QObject
{
  Q_OBJECT

public:
  static CameraViewpoints& instance();

  const QList<Viewpoint>& viewpoints() const { return m_viewpoints; }
  int size() const { return m_viewpoints.size(); }
  const Viewpoint& at(int index) const { return m_viewpoints.at(index); }

  /// Whether there is anything to fly: two or more viewpoints, or one
  /// that orbits, which is an animation on its own.
  bool isPath() const;

  /// Frames the whole path takes: its legs and orbits added up, never
  /// below two. Zero without a path. The animation scene is kept at
  /// this count while there is a path (see AnimationSceneGuard).
  int totalFrames() const;

  void append(const Viewpoint& viewpoint);
  void replace(int index, const Viewpoint& viewpoint);
  void removeAt(int index);
  void move(int from, int to);
  void clear();

  /// The progress at which each viewpoint is reached, with every leg and
  /// every orbit weighted by its duration. The first entry is always 0.
  /// Empty when there is no path (see isPath). A viewpoint that orbits
  /// is reached at its stop and left at its departure; for the others
  /// the two coincide, and the last departure is always 1.
  QList<double> stops() const;
  /// The progress at which the camera leaves each viewpoint, after any
  /// orbit there. Same shape as stops().
  QList<double> departures() const;

  /// The next unused default name, "Viewpoint N". Numbers are never
  /// reused within a session, so deleting Viewpoint 2 does not cause the
  /// next viewpoint to take its name.
  QString nextDefaultName() const;

  /// The time at which the path reaches viewpoint `anchor`, in [0, 1].
  /// With no path, anchor 0 is the start of the animation and anything
  /// later is the end, so a curve keyed to anchors still spans the
  /// timeline. Out-of-range anchors clamp.
  double anchorTime(int anchor) const;
  /// The time at which the path leaves viewpoint `anchor`, after its
  /// orbit if it has one; what an animation from that anchor waits for,
  /// so the state recorded there holds through the spin.
  double departureTime(int anchor) const;

  /// Map animation progress in [0, 1] onto a time along the path, also
  /// in [0, 1], applying each segment's easing. Segment boundaries are
  /// fixed points, so this only changes the pacing within a segment,
  /// never which viewpoint is reached when.
  double remapProgress(double progress) const;

  /// How far through one leg of the path the animation is, given its
  /// overall progress. Before the leg starts this is 0 and after it ends
  /// it is 1, so a visualization bound to a leg does its whole sweep
  /// while the camera flies that leg and holds still either side of it.
  ///
  /// A leg that no longer exists gives back `progress` unchanged, so
  /// the animation runs over the whole timeline rather than freezing.
  double segmentProgress(double progress, int segment) const;

  /// Move `camera` to time `t` in [0, 1] along the path. Parallel
  /// projection is left alone, since switching it mid-path would jump.
  void interpolate(double t, vtkCamera* camera);

  /// Start or stop flying the camera along the path. It lives here
  /// rather than in the dialog so it survives the dialog closing and is
  /// visible to the state file.
  /// Arm the flight so animation playback drives the camera along the
  /// path. When @a snapToHead is true the camera jumps to the start of
  /// the path immediately as a preview; pass false to leave the camera
  /// where it is until playback moves it.
  void startFlight(pqRenderView* view, bool snapToHead = true);
  void stopFlight();
  bool isFlying() const;
  /// The camera flies whenever there is a path and not otherwise: arm
  /// the flight in @a view if a path appeared, stop it if the path
  /// went. The camera is left where it is either way. Returns true when
  /// a flight was just armed.
  bool syncFlight(pqRenderView* view);

  /// Where viewpoint captions are centered, as fractions of the view's
  /// width and height from its lower-left corner, so the placement
  /// holds at whatever size a movie is exported. One position for the
  /// whole path: captions that hop around the frame read as a mistake.
  std::array<double, 2> captionPosition() const { return m_captionPosition; }
  void setCaptionPosition(double x, double y);

  QJsonObject serialize() const;
  bool deserialize(const QJsonObject& json);

signals:
  /// The list or its timing changed. The path has to be rebuilt.
  void changed();
  /// The caption moved; a caption on screen should follow at once.
  void captionPositionChanged();

private:
  Q_DISABLE_COPY(CameraViewpoints)

  CameraViewpoints() = default;

  void rebuildInterpolator();
  /// After any change to the list: drop the flight if there is no path
  /// left, and tell everyone.
  void noteChanged();

  QList<Viewpoint> m_viewpoints;
  std::array<double, 2> m_captionPosition = { 0.5, 0.05 };
  QPointer<QObject> m_flight;
  /// A stretch of consecutive viewpoints joined by ordinary legs, and
  /// the interpolator that flies them. Orbit legs split the path into
  /// these, so the spline through one stretch never bends towards a
  /// viewpoint an orbit reaches by other means. Held by pointer rather
  /// than by value so the header does not have to pull in the
  /// interpolator to destroy it.
  struct Run
  {
    int first = 0;
    int last = 0;
    vtkSmartPointer<vtkCameraInterpolator> interpolator;
  };
  std::vector<Run> m_runs;
  bool m_interpolatorStale = true;
};

} // namespace tomviz

#endif
