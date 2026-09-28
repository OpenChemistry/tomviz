/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationSceneGuard.h"

#include "ActiveObjects.h"
#include "CameraViewpoints.h"
#include "ModuleAnimations.h"
#include "SceneSnapshot.h"
#include "Utilities.h"
#include "pipeline/Pipeline.h"

#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerManagerModel.h>
#include <pqTimeKeeper.h>

#include <vtkCamera.h>
#include <vtkSMRenderViewProxy.h>

#include <vtkSMAnimationScene.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMViewProxy.h>

#include <QTimer>

#include <algorithm>

namespace tomviz {

namespace {

pqAnimationScene* activeScene()
{
  auto* core = pqPVApplicationCore::instance();
  auto* manager = core ? core->animationManager() : nullptr;
  return manager ? manager->getActiveScene() : nullptr;
}

vtkSMAnimationScene* sceneObject(pqAnimationScene* scene)
{
  auto* proxy = scene ? scene->getProxy() : nullptr;
  return proxy
           ? vtkSMAnimationScene::SafeDownCast(proxy->GetClientSideObject())
           : nullptr;
}

// Stop() only raises a flag the play loop honours after the current
// tick, and a scene time set inside that tick is ignored, so the rewind
// waits for the loop to unwind. The scene is the context, so a scene
// replaced meanwhile (a state load) is simply not rewound.
void rewindOnceStopped(pqAnimationScene* scene)
{
  QTimer::singleShot(0, scene, [scene]() {
    auto* object = sceneObject(scene);
    if (!object) {
      return;
    }
    if (object->GetInPlay()) {
      rewindOnceStopped(scene);
      return;
    }
    scene->setAnimationTime(object->GetStartTime());
  });
}

} // namespace

pqRenderView* animationRenderView()
{
  if (auto* renderView = ActiveObjects::instance().activePqRenderView()) {
    return renderView;
  }
  auto* core = pqApplicationCore::instance();
  auto* model = core ? core->getServerManagerModel() : nullptr;
  if (!model) {
    return nullptr;
  }
  auto renderViews = model->findItems<pqRenderView*>();
  return renderViews.isEmpty() ? nullptr : renderViews.first();
}

void interruptAnimationPlayback(bool rewind)
{
  auto* scene = activeScene();
  auto* object = sceneObject(scene);
  if (!object || !object->GetInPlay()) {
    return;
  }
  scene->getProxy()->InvokeCommand("Stop");
  if (rewind) {
    rewindOnceStopped(scene);
  }
}

AnimationSceneGuard::AnimationSceneGuard(QObject* parent) : QObject(parent)
{
  auto* core = pqPVApplicationCore::instance();
  auto* manager = core ? core->animationManager() : nullptr;
  if (manager) {
    // Loading a state file replaces the scene, so follow the active one.
    connect(manager, &pqAnimationManager::activeSceneChanged, this,
            &AnimationSceneGuard::follow);
    follow(manager->getActiveScene());
  }

  auto& viewpoints = CameraViewpoints::instance();
  connect(&viewpoints, &CameraViewpoints::changed, this, [this]() {
    interruptAnimationPlayback();
    syncFlight();
    syncFrames();
  });
  connect(&ModuleAnimations::instance(), &ModuleAnimations::changed, this,
          []() { interruptAnimationPlayback(); });
  // A flight dies with its view; the next view picks the path up.
  connect(&ActiveObjects::instance(),
          qOverload<vtkSMViewProxy*>(&ActiveObjects::viewChanged), this,
          [this]() { syncFlight(); });
  syncFlight();
}

void AnimationSceneGuard::syncFlight()
{
  if (CameraViewpoints::instance().syncFlight(animationRenderView())) {
    ensureAnimationFrames();
  }
}

void AnimationSceneGuard::syncFrames()
{
  const int frames = CameraViewpoints::instance().totalFrames();
  if (frames > 0) {
    setAnimationNumberOfFrames(frames);
  }
}

void AnimationSceneGuard::follow(pqAnimationScene* scene)
{
  if (scene == m_scene) {
    return;
  }
  if (m_scene) {
    disconnect(m_scene.data(), nullptr, this, nullptr);
  }
  m_scene = scene;
  if (!scene) {
    return;
  }
  // Both run before the player reads the scene time or starts ticking
  connect(scene, &pqAnimationScene::beginPlay, this,
          [this, scene](vtkObject*, unsigned long, void*, void* reversed) {
            provideDefaultAnimation();
            syncFrames();
            rewindIfAtEnd(scene, reversed && *static_cast<bool*>(reversed));
          });
}

void AnimationSceneGuard::provideDefaultAnimation()
{
  auto& viewpoints = CameraViewpoints::instance();
  auto& active = ActiveObjects::instance();
  auto* pipeline = active.pipeline();
  auto* timeKeeper = active.activeTimeKeeper();
  // Nothing set up, but something to look at. A lone plain viewpoint is
  // a path in the making and is left alone; a time series plays its own
  // steps.
  if (viewpoints.size() != 0 || !ModuleAnimations::instance().isEmpty() ||
      !pipeline || pipeline->nodes().isEmpty() ||
      (timeKeeper && !timeKeeper->getTimeSteps().empty())) {
    return;
  }
  auto* view = animationRenderView();
  auto* proxy = view ? view->getRenderViewProxy() : nullptr;
  auto* camera = proxy ? proxy->GetActiveCamera() : nullptr;
  if (!camera) {
    return;
  }
  Viewpoint viewpoint;
  viewpoint.readFrom(camera);
  viewpoint.name = "Camera Orbit";
  viewpoint.orbitTurns = 1;
  // As long as the animation was going to be
  if (auto* scene = activeScene()) {
    viewpoint.orbitFrames = std::max(
      2, vtkSMPropertyHelper(scene->getProxy(), "NumberOfFrames").GetAsInt());
  }
  // Recorded like a viewpoint the user adds, so a visualization added
  // later fades in on the leg that first has it
  viewpoint.scene = SceneSnapshot::capture(pipeline);
  // Appending arms the flight through the change handler
  viewpoints.append(viewpoint);
}

void AnimationSceneGuard::rewindIfAtEnd(pqAnimationScene* scene, bool reversed)
{
  auto* object = sceneObject(scene);
  if (!object) {
    return;
  }
  const double start = object->GetStartTime();
  const double end = object->GetEndTime();
  const double time = object->GetSceneTime();
  if (end <= start) {
    return;
  }
  // Rounding from summing a frame's worth at a time is far below this;
  // a frame is not.
  const double tolerance = 1e-9 * (end - start);
  const bool atEnd = reversed ? time <= start + tolerance
                              : time >= end - tolerance;
  if (!atEnd) {
    return;
  }
  // Through the proxy, so its AnimationTime property agrees with what
  // the scene now says; outside a tick the scene applies it at once.
  scene->setAnimationTime(reversed ? end : start);
}

} // namespace tomviz
