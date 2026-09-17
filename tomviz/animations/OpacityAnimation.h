/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOpacityAnimation_h
#define tomvizOpacityAnimation_h

#include "ModuleAnimation.h"

#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/SegmentSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ThresholdSink.h"

namespace tomviz {

/// Ramps a visualization's opacity from one value to another, so a
/// module can fade in or out over the course of the animation.
///
/// The sinks that have an opacity are unrelated types that happen to
/// share a setter name rather than a base class declaring one, so the
/// target is resolved per type.
class OpacityAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  double startValue = 1;
  double stopValue = 1;

  OpacityAnimation(pipeline::Node* node, double start, double stop)
    : ModuleAnimation(node), startValue(start), stopValue(stop)
  {
  }

  /// True if `node` has an opacity this animation can drive: the flat
  /// opacity of the surface and plane modules, and a label map's surface.
  static bool supports(pipeline::Node* node)
  {
    return qobject_cast<pipeline::ContourSink*>(node) ||
           qobject_cast<pipeline::SliceSink*>(node) ||
           qobject_cast<pipeline::ClipSink*>(node) ||
           qobject_cast<pipeline::ThresholdSink*>(node) ||
           qobject_cast<pipeline::SegmentSink*>(node) ||
           qobject_cast<pipeline::LabelMapSink*>(node);
  }

  /// The current opacity of `node`, or 1 if it has none.
  static double opacityOf(pipeline::Node* node)
  {
    if (auto* contour = qobject_cast<pipeline::ContourSink*>(node)) {
      return contour->opacity();
    }
    if (auto* slice = qobject_cast<pipeline::SliceSink*>(node)) {
      return slice->opacity();
    }
    if (auto* clip = qobject_cast<pipeline::ClipSink*>(node)) {
      return clip->opacity();
    }
    if (auto* threshold = qobject_cast<pipeline::ThresholdSink*>(node)) {
      return threshold->opacity();
    }
    if (auto* segment = qobject_cast<pipeline::SegmentSink*>(node)) {
      return segment->opacity();
    }
    if (auto* labels = qobject_cast<pipeline::LabelMapSink*>(node)) {
      return labels->surfaceOpacity();
    }
    return 1;
  }

  QString type() const override { return "opacity"; }

  QString describeParameters() const override
  {
    return QString("opacity %1 to %2").arg(startValue).arg(stopValue);
  }

  QJsonObject serialize() const override
  {
    return { { "start", startValue }, { "stop", stopValue } };
  }

  void onTimeChanged() override
  {
    if (!timeKeeper() || !baseNode) {
      return;
    }

    double value = (stopValue - startValue) * progress() + startValue;
    setOpacity(baseNode.data(), value);
  }

private:
  static void setOpacity(pipeline::Node* node, double value)
  {
    if (auto* contour = qobject_cast<pipeline::ContourSink*>(node)) {
      contour->setOpacity(value);
    } else if (auto* slice = qobject_cast<pipeline::SliceSink*>(node)) {
      slice->setOpacity(value);
    } else if (auto* clip = qobject_cast<pipeline::ClipSink*>(node)) {
      clip->setOpacity(value);
    } else if (auto* threshold = qobject_cast<pipeline::ThresholdSink*>(node)) {
      threshold->setOpacity(value);
    } else if (auto* segment = qobject_cast<pipeline::SegmentSink*>(node)) {
      segment->setOpacity(value);
    } else if (auto* labels = qobject_cast<pipeline::LabelMapSink*>(node)) {
      labels->setSurfaceOpacity(value);
    }
  }
};

} // namespace tomviz

#endif
