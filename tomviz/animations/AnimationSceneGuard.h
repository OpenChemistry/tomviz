/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAnimationSceneGuard_h
#define tomvizAnimationSceneGuard_h

#include <QObject>
#include <QPointer>

class pqAnimationScene;
class pqRenderView;

namespace tomviz {

/// The render view animations play in: the active one, or the first
/// there is when the active view is something else (a chart).
pqRenderView* animationRenderView();

/// Stop a running playback and, once the play loop has unwound, rewind
/// it to the start. For edits that change what the animation means:
/// the spot it was at no longer corresponds to anything, and Play then
/// shows the edit from the top. A paused animation is left where it is.
/// Without @a rewind the playback just stops where it is, for an action
/// that puts the camera somewhere on purpose and must not be undone by
/// the first frame.
void interruptAnimationPlayback(bool rewind = true);

/// Keeps the animation machinery consistent whatever the user does,
/// whether or not the Animation Helper was ever opened.
///
/// - The camera flies the viewpoint path whenever there is one, in the
///   render view that is current, and stops when the path goes; a view
///   destroyed and recreated picks the flight up again.
/// - A change to the path or to the module animations interrupts a
///   playback (see interruptAnimationPlayback).
/// - Play with nothing set up to animate turns the current view into a
///   Camera Orbit viewpoint first, so a fresh dataset spins out of the
///   box and the viewpoint list stays empty until then.
/// - While there is a path, the scene's frame count is what its legs
///   and orbits add up to.
/// - A finished animation is rewound before it plays again. ParaView's
///   player only rewinds when the scene time is at or past the end, but
///   its sequence player reaches the last frame by summing frame-sized
///   steps, and for some counts (50 is one) that lands a rounding error
///   short of the end; every later Play would then start there, tick
///   once at an unchanged time and stop. The player announces a playback
///   before it reads the scene time, so a rewind done then is what it
///   starts from.
class AnimationSceneGuard : public QObject
{
  Q_OBJECT

public:
  explicit AnimationSceneGuard(QObject* parent = nullptr);

private:
  void follow(pqAnimationScene* scene);
  void rewindIfAtEnd(pqAnimationScene* scene, bool reversed);
  void provideDefaultAnimation();
  void syncFlight();
  void syncFrames();

  QPointer<pqAnimationScene> m_scene;
};

} // namespace tomviz

#endif
