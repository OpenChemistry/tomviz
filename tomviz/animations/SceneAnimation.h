/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSceneAnimation_h
#define tomvizSceneAnimation_h

#include "CameraViewpoints.h"
#include "ModuleAnimation.h"
#include "SceneSnapshot.h"
#include "pipeline/Pipeline.h"
#include "pipeline/sinks/VolumeSink.h"

#include <QPointer>
#include <QSet>

namespace tomviz {

/// Carries the module state recorded with the viewpoints along the
/// camera path: between two viewpoints every recorded module moves from
/// its state at one to its state at the other, in step with the camera.
/// Lives exactly as long as the camera flight it accompanies.
class SceneAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  SceneAnimation(pipeline::Pipeline* pipeline)
    : ModuleAnimation(nullptr), m_pipeline(pipeline)
  {
  }

  ~SceneAnimation() override { releaseVolumes(); }

  void onTimeChanged() override { applyProgress(progress()); }

  // Frame 0 is not announced when the clock already sits there
  void onPlaybackStarted() override { applyProgress(0.0); }

  void onPlaybackEnded() override { releaseVolumes(); }

private:
  void applyProgress(double p)
  {
    if (!timeKeeper() || !m_pipeline) {
      return;
    }
    auto& viewpoints = CameraViewpoints::instance();
    if (viewpoints.size() < 2) {
      return;
    }
    const double t = viewpoints.remapProgress(p);
    auto stops = viewpoints.stops();
    int leg = 0;
    while (leg + 2 < stops.size() && stops[leg + 1] <= t) {
      ++leg;
    }
    const double start = stops[leg];
    const double stop = stops[leg + 1];
    const double u = stop > start ? (t - start) / (stop - start) : 1.0;
    applySceneTransition(m_pipeline, viewpoints.at(leg).scene,
                         viewpoints.at(leg + 1).scene, u, m_overriddenVolumes);
  }

  // Blended curves override the editor's only while playing
  void releaseVolumes()
  {
    if (m_pipeline) {
      for (int id : m_overriddenVolumes) {
        if (auto* volume =
              qobject_cast<pipeline::VolumeSink*>(m_pipeline->nodeById(id))) {
          volume->setAnimatedScalarOpacity(nullptr);
        }
      }
    }
    m_overriddenVolumes.clear();
  }

  QPointer<pipeline::Pipeline> m_pipeline;
  QSet<int> m_overriddenVolumes;
};

} // namespace tomviz

#endif
