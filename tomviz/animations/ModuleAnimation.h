/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleAnimation_h
#define tomvizModuleAnimation_h

#include "ActiveObjects.h"
#include "CameraViewpoints.h"
#include "pipeline/Node.h"

#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqPVApplicationCore.h>
#include <pqTimeKeeper.h>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

namespace tomviz {

class ModuleAnimation : public QObject
{
  Q_OBJECT

public:
  QPointer<pipeline::Node> baseNode;

  /// The leg of the camera path this animation runs during, or -1 to run
  /// over the whole timeline. Binding to a leg is what lets one
  /// visualization change while the camera flies from one viewpoint to
  /// the next, and another change on the leg after that.
  int segment = -1;

  ModuleAnimation(pipeline::Node* node) : baseNode(node)
  {
    setupConnections();
  }

  virtual void setupConnections()
  {
    if (timeKeeper()) {
      connect(timeKeeper(), &pqTimeKeeper::timeChanged, this,
              &ModuleAnimation::onTimeChanged);
    }

    if (baseNode) {
      connect(baseNode.data(), &QObject::destroyed, this,
              &QObject::deleteLater);
    }

    // Playback leaves whatever the last tick applied. Animations that
    // override module state (e.g. the scalar opacity morph) use the
    // playback-ended hook to hand control back to the user's settings.
    // The scene is replaced on state load, so follow the active one.
    auto* core = pqPVApplicationCore::instance();
    auto* manager = core ? core->animationManager() : nullptr;
    if (manager) {
      connect(manager, &pqAnimationManager::activeSceneChanged, this,
              [this]() { bindToScene(); });
    }
    bindToScene();
  }

  /// Called when animation playback starts, before the first frame. The
  /// time keeper only announces changes, so a playback that begins where
  /// the clock already sits never ticks its first frame; an animation
  /// that must show its starting state overrides this. Default does
  /// nothing.
  virtual void onPlaybackStarted() {}

  /// Called when animation playback finishes. Default does nothing.
  virtual void onPlaybackEnded() {}

  virtual ActiveObjects& activeObjects() { return ActiveObjects::instance(); }
  virtual pqTimeKeeper* timeKeeper()
  {
    return activeObjects().activeTimeKeeper();
  }

  virtual double time()
  {
    if (!timeKeeper()) {
      return 0;
    }

    return timeKeeper()->getTime();
  }

  virtual QList<double> timeSteps()
  {
    if (!timeKeeper()) {
      return { 0 };
    }

    auto timeSteps = timeKeeper()->getTimeSteps();
    if (timeSteps.empty()) {
      timeSteps.append(0);
      timeSteps.append(1);
    }

    return timeSteps;
  }

  virtual double timeStart() { return timeSteps().front(); }
  virtual double timeStop() { return timeSteps().back(); }

  virtual double progress()
  {
    double start = timeStart();
    double stop = timeStop();
    if (stop <= start) {
      // A single time step (or none) has no range to animate over.
      return 0;
    }
    double elapsed = (time() - start) / (stop - start);
    if (segment < 0) {
      return elapsed;
    }

    return CameraViewpoints::instance().segmentProgress(elapsed, segment);
  }

  virtual void onTimeChanged() {}

  /// A short phrase for the animation list, e.g. "iso value 120 to 400".
  virtual QString describeParameters() const { return {}; }

  /// True for an animation derived from the state recorded with the
  /// camera viewpoints (see RecordedAnimations). Rebuilt from the
  /// viewpoints rather than saved, and never authored by hand.
  virtual bool recorded() const { return false; }

  /// The name this animation is saved under in a state file. Empty means
  /// it is not saved, because whatever created it rebuilds it instead.
  virtual QString type() const { return QString(); }

  /// The parameters needed to rebuild this animation. The node it runs
  /// on is written by the caller, which is the only one holding the ids.
  virtual QJsonObject serialize() const { return QJsonObject(); }

private:
  /// Track the active animation scene's endPlay so onPlaybackEnded()
  /// fires no matter which scene is current.
  void bindToScene()
  {
    auto* core = pqPVApplicationCore::instance();
    auto* manager = core ? core->animationManager() : nullptr;
    auto* scene = manager ? manager->getActiveScene() : nullptr;
    if (scene == m_boundScene) {
      return;
    }
    if (m_boundScene) {
      disconnect(m_boundScene.data(), nullptr, this, nullptr);
    }
    m_boundScene = scene;
    if (scene) {
      connect(scene, &pqAnimationScene::beginPlay, this,
              [this]() { onPlaybackStarted(); });
      connect(scene, &pqAnimationScene::endPlay, this,
              [this]() { onPlaybackEnded(); });
    }
  }

  QPointer<pqAnimationScene> m_boundScene;
};

} // namespace tomviz

#endif
