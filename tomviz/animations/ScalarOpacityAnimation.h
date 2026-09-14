/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizScalarOpacityAnimation_h
#define tomvizScalarOpacityAnimation_h

#include "CameraViewpoints.h"
#include "ModuleAnimation.h"
#include "OpacityInterpolation.h"

#include "Utilities.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <QJsonArray>

#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSmartPointer.h>

namespace tomviz {

/// An opacity curve captured for one point of the animation. The anchor
/// is a viewpoint index, resolved to a time through the camera path's
/// stops, so retiming a leg moves the curve change with it. With no
/// camera path, anchor 0 is the start of the animation and anything
/// later is the end.
struct OpacityKeyframe
{
  int anchor = 0;
  vtkSmartPointer<vtkPiecewiseFunction> curve;
};

/// Morphs a volume through a sequence of opacity curves, one keyed to
/// each chosen viewpoint, so a feature can dissolve away leg by leg.
///
/// One animation holds the whole sequence: separate morphs on the same
/// volume would each write its opacity every tick and the last writer
/// would win. The blend goes to the sink separately rather than into
/// the curve the histogram editor owns, which an interrupted morph
/// would otherwise leave replaced by a sampled one.
class ScalarOpacityAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  ScalarOpacityAnimation(pipeline::VolumeSink* sink,
                         const QList<OpacityKeyframe>& keyframes)
    : ModuleAnimation(sink)
  {
    for (const auto& keyframe : keyframes) {
      if (!keyframe.curve || keyframe.curve->GetSize() == 0) {
        continue;
      }
      OpacityKeyframe copy;
      copy.anchor = keyframe.anchor;
      copy.curve = vtkSmartPointer<vtkPiecewiseFunction>::New();
      copy.curve->DeepCopy(keyframe.curve);
      m_keyframes.append(copy);
    }
    std::sort(m_keyframes.begin(), m_keyframes.end(),
              [](const OpacityKeyframe& a, const OpacityKeyframe& b) {
                return a.anchor < b.anchor;
              });
  }

  ~ScalarOpacityAnimation() override
  {
    // Hand the volume back to the curve in the histogram editor.
    if (auto* target = sink()) {
      target->setAnimatedScalarOpacity(nullptr);
    }
  }

  void onPlaybackEnded() override
  {
    // While the animation exists its blend owns the volume's opacity;
    // when playback stops, hand the volume back to the histogram
    // editor's curve. Without this the override stays installed and
    // editing the transfer function appears to do nothing.
    if (auto* target = sink()) {
      target->setAnimatedScalarOpacity(nullptr);
    }
  }

  /// True if @a node has a scalar opacity worth animating. Only volume
  /// rendering reads one; the surface sinks have a flat opacity instead,
  /// which OpacityAnimation covers. Label maps render through the same
  /// sink but rebuild their transfer functions from the label table, so
  /// a morph would fight that projection every tick.
  static bool supports(pipeline::Node* node)
  {
    return qobject_cast<pipeline::VolumeSink*>(node) &&
           !qobject_cast<pipeline::LabelMapSink*>(node);
  }

  pipeline::VolumeSink* sink()
  {
    return qobject_cast<pipeline::VolumeSink*>(baseNode.data());
  }

  const QList<OpacityKeyframe>& keyframes() const { return m_keyframes; }

  QString type() const override { return "scalarOpacity"; }

  QString describeParameters() const override
  {
    return QString("opacity curve, %1 keyframes").arg(m_keyframes.size());
  }

  QJsonObject serialize() const override
  {
    QJsonArray array;
    for (const auto& keyframe : m_keyframes) {
      QJsonObject entry;
      entry["anchor"] = keyframe.anchor;
      entry["curve"] = tomviz::serialize(keyframe.curve.Get());
      array.append(entry);
    }
    QJsonObject json;
    json["keyframes"] = array;
    return json;
  }

  void onTimeChanged() override
  {
    if (timeKeeper()) {
      applyProgress(progress());
    }
  }

  // Frame 0 is not announced when the clock already sits there
  void onPlaybackStarted() override { applyProgress(0.0); }

  /// Apply the blend for progress @a t in [0, 1]. Public so it can be
  /// driven without a time keeper.
  void applyProgress(double t)
  {
    auto* target = sink();
    if (!target || m_keyframes.isEmpty()) {
      return;
    }

    auto& viewpoints = CameraViewpoints::instance();

    // The keyframe pair whose window contains the current time. Before
    // the first keyframe the first curve holds; after the last, the last.
    int next = 0;
    while (next < m_keyframes.size() &&
           viewpoints.anchorTime(m_keyframes[next].anchor) <= t) {
      ++next;
    }

    if (next == 0) {
      m_current->DeepCopy(m_keyframes.first().curve);
    } else if (next == m_keyframes.size()) {
      m_current->DeepCopy(m_keyframes.last().curve);
    } else {
      const auto& from = m_keyframes[next - 1];
      const auto& to = m_keyframes[next];
      const double start = viewpoints.anchorTime(from.anchor);
      const double stop = viewpoints.anchorTime(to.anchor);
      const double u = stop > start ? (t - start) / (stop - start) : 1.0;

      // Both curves are read over the volume's own window, which is what
      // makes two curves captured at different times comparable.
      double range[2] = { 0.0, 1.0 };
      auto volume = target->volumeData();
      if (volume && volume->isValid()) {
        auto volumeRange = volume->colorMapRange();
        range[0] = volumeRange[0];
        range[1] = volumeRange[1];
      }

      interpolateOpacity(from.curve, to.curve, u, range, m_current);
    }

    target->setAnimatedScalarOpacity(m_current);
    curveApplied(m_current);
  }

protected:
  /// The blend just handed to the volume. A subclass that ties the
  /// volume's visibility to its curve reads it here.
  virtual void curveApplied(vtkPiecewiseFunction*) {}

private:
  QList<OpacityKeyframe> m_keyframes;
  vtkNew<vtkPiecewiseFunction> m_current;
};

} // namespace tomviz

#endif
