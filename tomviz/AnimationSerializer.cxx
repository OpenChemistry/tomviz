/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationSerializer.h"

#include "ActiveObjects.h"
#include "Utilities.h"
#include "animations/CameraViewpoints.h"
#include "animations/ModuleAnimations.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>

#include <vtkCamera.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMRenderViewProxy.h>

namespace tomviz {

namespace {

vtkSMProxy* animationSceneProxy()
{
  auto* core = pqPVApplicationCore::instance();
  auto* manager = core ? core->animationManager() : nullptr;
  auto* scene = manager ? manager->getActiveScene() : nullptr;
  return scene ? scene->getProxy() : nullptr;
}

} // anonymous namespace

void AnimationSerializer::save(QJsonObject& doc)
{
  auto animation = CameraViewpoints::instance().serialize();

  auto modules =
    ModuleAnimations::instance().serialize(ActiveObjects::instance().pipeline());
  animation["modules"] = modules["modules"];

  if (auto* scene = animationSceneProxy()) {
    animation["numberOfFrames"] =
      vtkSMPropertyHelper(scene, "NumberOfFrames").GetAsInt();
  }

  doc["animation"] = animation;
}

void AnimationSerializer::restore(const QJsonObject& doc,
                                  pipeline::Pipeline* pipeline)
{
  auto animation = doc["animation"].toObject();

  auto& viewpoints = CameraViewpoints::instance();
  if (!viewpoints.deserialize(animation)) {
    viewpoints.clear();
  }

  ModuleAnimations::instance().deserialize(animation, pipeline);

  auto* renderView = ActiveObjects::instance().activePqRenderView();
  auto* viewProxy = renderView ? renderView->getRenderViewProxy() : nullptr;
  auto* camera = viewProxy ? viewProxy->GetActiveCamera() : nullptr;
  // Files from before orbits were viewpoints flagged a ParaView orbit
  // cue instead; the same spin is now a viewpoint that orbits.
  if (animation["cameraOrbit"].toBool() && viewpoints.size() == 0 &&
      camera) {
    Viewpoint viewpoint;
    viewpoint.readFrom(camera);
    viewpoint.name = "Camera Orbit";
    viewpoint.orbitTurns = 1;
    viewpoints.append(viewpoint);
  }
  // The camera flies whenever there is a path.
  viewpoints.stopFlight();
  viewpoints.syncFlight(renderView);

  // Loading a state file builds a fresh animation scene, which the view
  // restore leaves on the frame count used for newly loaded data. A file
  // that recorded its own count knows better; one that did not keeps it.
  // With a path the count is what its legs and orbits add up to; the
  // saved total only applies without one.
  int numberOfFrames = animation["numberOfFrames"].toInt(0);
  if (viewpoints.isPath()) {
    setAnimationNumberOfFrames(viewpoints.totalFrames());
  } else if (numberOfFrames > 0) {
    setAnimationNumberOfFrames(numberOfFrames);
  }
}

} // namespace tomviz
