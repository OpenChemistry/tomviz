/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCameraAnimation_h
#define tomvizCameraAnimation_h

#include "CameraViewpoints.h"
#include "ModuleAnimation.h"

#include <pqRenderView.h>

#include <vtkCamera.h>
#include <vtkCoordinate.h>
#include <vtkNew.h>
#include <vtkRenderer.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>

namespace tomviz {

/// Flies the camera through the saved viewpoints as the animation plays.
///
/// The camera orbit is a real ParaView camera cue, but this is not: a
/// keyframe cue interpolates at a constant rate, and the whole point of
/// the viewpoint path is that each segment can have its own length and
/// ease in and out. So the camera is driven directly, like the other
/// tomviz animations. Only one of the two should exist at a time, or
/// they fight over the camera on every tick.
class CameraAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  CameraAnimation(pqRenderView* view) : ModuleAnimation(nullptr), m_view(view)
  {
    if (m_view) {
      connect(m_view.data(), &QObject::destroyed, this, &QObject::deleteLater);
    }
    // Viewpoint captions: readable on any background, centered where the
    // path says (the bottom middle unless the user moved them)
    auto* text = m_caption->GetTextProperty();
    text->SetFontSize(22);
    text->SetBold(true);
    text->SetColor(1.0, 1.0, 1.0);
    text->SetShadow(true);
    text->SetJustificationToCentered();
    text->SetVerticalJustificationToCentered();
    m_caption->GetPositionCoordinate()
      ->SetCoordinateSystemToNormalizedViewport();
    placeCaption();
    m_caption->SetVisibility(0);
    // Nudging the position in the Animation Helper moves a caption that
    // is already on screen, so the user sees where it lands.
    connect(&CameraViewpoints::instance(),
            &CameraViewpoints::captionPositionChanged, this, [this]() {
              placeCaption();
              if (m_caption->GetVisibility() && m_view) {
                m_view->render();
              }
            });
  }

  void onPlaybackEnded() override
  {
    // A caption belongs to the path; nothing should linger once it stops
    if (hideCaption() && m_view) {
      m_view->render();
    }
  }

  ~CameraAnimation() override
  {
    if (m_captionAdded) {
      if (auto* ren = renderer()) {
        ren->RemoveViewProp(m_caption);
      }
    }
  }

  void onTimeChanged() override { applyProgress(progress()); }

  // Frame 0 is not announced when the clock already sits there
  void onPlaybackStarted() override { applyProgress(0.0); }

private:
  void applyProgress(double p)
  {
    if (!timeKeeper() || !m_view) {
      return;
    }

    auto& viewpoints = CameraViewpoints::instance();
    if (!viewpoints.isPath()) {
      hideCaption();
      return;
    }

    auto* proxy = m_view->getRenderViewProxy();
    auto* camera = proxy ? proxy->GetActiveCamera() : nullptr;
    if (!camera) {
      return;
    }

    const double t = viewpoints.remapProgress(p);
    viewpoints.interpolate(t, camera);
    updateCaption(t);

    // The interpolated clipping range is blended from the saved
    // viewpoints, which can clip the data at positions in between.
    if (auto* renderer = proxy->GetRenderer()) {
      renderer->ResetCameraClippingRange();
    }

    m_view->render();
  }

  vtkRenderer* renderer() const
  {
    auto* proxy = m_view ? m_view->getRenderViewProxy() : nullptr;
    return proxy ? proxy->GetRenderer() : nullptr;
  }

  // Show the label of the viewpoint the path is leaving: the last one
  // whose time is at or before t. Hidden while that label is empty.
  void updateCaption(double t)
  {
    auto& viewpoints = CameraViewpoints::instance();
    auto stops = viewpoints.stops();
    int current = 0;
    while (current + 1 < stops.size() && current + 1 < viewpoints.size() &&
           stops[current + 1] <= t) {
      ++current;
    }
    QString label =
      current < viewpoints.size() ? viewpoints.at(current).label : QString();
    auto* ren = renderer();
    if (!ren) {
      return;
    }
    if (!m_captionAdded) {
      ren->AddViewProp(m_caption);
      m_captionAdded = true;
    }
    m_caption->SetInput(label.toUtf8().constData());
    m_caption->SetVisibility(label.isEmpty() ? 0 : 1);
  }

  void placeCaption()
  {
    const auto position = CameraViewpoints::instance().captionPosition();
    m_caption->SetPosition(position[0], position[1]);
  }

  // True when the caption was showing
  bool hideCaption()
  {
    bool wasVisible = m_caption->GetVisibility() != 0;
    m_caption->SetVisibility(0);
    return wasVisible;
  }

  QPointer<pqRenderView> m_view;
  vtkNew<vtkTextActor> m_caption;
  bool m_captionAdded = false;
};

} // namespace tomviz

#endif
