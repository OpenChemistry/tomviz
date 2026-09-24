/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimatableProperties.h"

#include "ClipAnimation.h"
#include "ContourAnimation.h"
#include "CutOutAnimation.h"
#include "ExplodedAnimation.h"
#include "OpacityAnimation.h"
#include "SliceAnimation.h"
#include "SolidityAnimation.h"
#include "ThresholdAnimation.h"

#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ThresholdSink.h"
#include "pipeline/sinks/VolumeSink.h"

namespace tomviz {

using pipeline::ClipSink;
using pipeline::ContourSink;
using pipeline::Node;
using pipeline::SliceSink;
using pipeline::ThresholdSink;
using pipeline::VolumeSink;

namespace {

template <typename SinkT>
bool isA(Node* node)
{
  return qobject_cast<SinkT*>(node) != nullptr;
}

AnimatableProperty isoValue()
{
  AnimatableProperty p;
  p.id = "iso";
  p.label = [](Node*) { return QString("Iso value"); };
  p.applies = isA<ContourSink>;
  p.range = [](Node* node) {
    PropertyRange r;
    r.label = "Iso value:";
    double range[2] = { 0.0, 1.0 };
    if (auto* contour = qobject_cast<ContourSink*>(node)) {
      contour->scalarRange(range);
    }
    r.lo = range[0];
    r.hi = range[1];
    r.start = (r.hi - r.lo) / 3 + r.lo;
    r.stop = (r.hi - r.lo) * 2 / 3 + r.lo;
    return r;
  };
  p.make = [](Node* node, double start, double stop) -> ModuleAnimation* {
    auto* contour = qobject_cast<ContourSink*>(node);
    return contour ? new ContourAnimation(contour, start, stop) : nullptr;
  };
  p.matches = [](ModuleAnimation* animation, double& start, double& stop) {
    auto* contour = qobject_cast<ContourAnimation*>(animation);
    if (!contour) {
      return false;
    }
    start = contour->startValue;
    stop = contour->stopValue;
    return true;
  };
  return p;
}

// A slice and a clip both move by index while axis aligned and by
// signed distance from the center of the data along their normal while
// custom, so the two entries share their shape.
template <typename SinkT, typename AnimationT>
AnimatableProperty planeProperty(const QString& id)
{
  AnimatableProperty p;
  p.id = id;
  p.label = [](Node* node) {
    auto* sink = qobject_cast<SinkT*>(node);
    return sink && !sink->isOrtho() ? QString("Position")
                                    : QString("Slice index");
  };
  p.applies = isA<SinkT>;
  p.range = [](Node* node) {
    PropertyRange r;
    auto* sink = qobject_cast<SinkT*>(node);
    if (!sink || sink->isOrtho()) {
      r.label = "Slice:";
      r.decimals = 0;
      r.hi = sink ? sink->maxSlice() : 0;
      r.stop = r.hi;
    } else {
      // A plane at an arbitrary angle has no slices to count
      r.label = "Position:";
      sink->planeDistanceRange(r.lo, r.hi);
      r.start = r.lo;
      r.stop = r.hi;
    }
    return r;
  };
  p.make = [](Node* node, double start, double stop) -> ModuleAnimation* {
    auto* sink = qobject_cast<SinkT*>(node);
    if (!sink) {
      return nullptr;
    }
    auto unit = sink->isOrtho() ? AnimationT::Slice : AnimationT::Distance;
    return new AnimationT(sink, start, stop, unit);
  };
  p.matches = [](ModuleAnimation* animation, double& start, double& stop) {
    auto* plane = qobject_cast<AnimationT*>(animation);
    if (!plane) {
      return false;
    }
    start = plane->startValue;
    stop = plane->stopValue;
    return true;
  };
  return p;
}

AnimatableProperty threshold(ThresholdAnimation::End end)
{
  AnimatableProperty p;
  const bool lower = end == ThresholdAnimation::Lower;
  p.id = lower ? "thresholdLower" : "thresholdUpper";
  p.label = [lower](Node*) {
    return QString(lower ? "Lower threshold" : "Upper threshold");
  };
  p.applies = isA<ThresholdSink>;
  p.range = [lower](Node* node) {
    PropertyRange r;
    r.label = lower ? "Lower:" : "Upper:";
    double range[2] = { 0.0, 1.0 };
    auto* sink = qobject_cast<ThresholdSink*>(node);
    if (sink) {
      sink->scalarRange(range);
    }
    r.lo = range[0];
    r.hi = range[1];
    // Growing the segmentation out from where it is now, to everything
    // (lower end) or to the brightest voxels (upper end)
    if (lower) {
      r.start = sink ? sink->lowerThreshold() : r.lo;
      r.stop = r.lo;
    } else {
      r.start = sink ? sink->upperThreshold() : r.hi;
      r.stop = r.hi;
    }
    return r;
  };
  p.make = [end](Node* node, double start, double stop) -> ModuleAnimation* {
    auto* sink = qobject_cast<ThresholdSink*>(node);
    return sink ? new ThresholdAnimation(sink, start, stop, end) : nullptr;
  };
  p.matches = [end](ModuleAnimation* animation, double& start, double& stop) {
    auto* sweep = qobject_cast<ThresholdAnimation*>(animation);
    if (!sweep || sweep->end != end) {
      return false;
    }
    start = sweep->startValue;
    stop = sweep->stopValue;
    return true;
  };
  return p;
}

AnimatableProperty opacity()
{
  AnimatableProperty p;
  p.id = "opacity";
  p.label = [](Node*) { return QString("Opacity"); };
  p.applies = OpacityAnimation::supports;
  p.range = [](Node* node) {
    PropertyRange r;
    r.label = "Opacity:";
    // Fading out from wherever the module sits now is the useful
    // default; the user can invert it by swapping the two values.
    r.start = OpacityAnimation::opacityOf(node);
    r.stop = 0.0;
    return r;
  };
  p.make = [](Node* node, double start, double stop) -> ModuleAnimation* {
    return OpacityAnimation::supports(node)
             ? new OpacityAnimation(node, start, stop)
             : nullptr;
  };
  p.matches = [](ModuleAnimation* animation, double& start, double& stop) {
    auto* fade = qobject_cast<OpacityAnimation*>(animation);
    if (!fade) {
      return false;
    }
    start = fade->startValue;
    stop = fade->stopValue;
    return true;
  };
  return p;
}

AnimatableProperty solidity()
{
  AnimatableProperty p;
  p.id = "solidity";
  p.label = [](Node*) { return QString("Solidity"); };
  p.applies = isA<VolumeSink>;
  p.range = [](Node* node) {
    PropertyRange r;
    r.label = "Solidity:";
    // Zero would make the volume vanish and cannot be set
    r.lo = 0.01;
    r.hi = 1.0;
    auto* volume = qobject_cast<VolumeSink*>(node);
    r.start = volume ? volume->solidity() : 1.0;
    r.stop = r.start >= 0.5 ? 0.1 : 1.0;
    return r;
  };
  p.make = [](Node* node, double start, double stop) -> ModuleAnimation* {
    auto* volume = qobject_cast<VolumeSink*>(node);
    return volume ? new SolidityAnimation(volume, start, stop) : nullptr;
  };
  p.matches = [](ModuleAnimation* animation, double& start, double& stop) {
    auto* sweep = qobject_cast<SolidityAnimation*>(animation);
    if (!sweep) {
      return false;
    }
    start = sweep->startValue;
    stop = sweep->stopValue;
    return true;
  };
  return p;
}

AnimatableProperty exploded(ExplodedAnimation::Unit unit)
{
  AnimatableProperty p;
  p.id = unit == ExplodedAnimation::Gap      ? "explodedGap"
         : unit == ExplodedAnimation::Chunks ? "explodedChunks"
                                             : "explodedOffset";
  p.label = [unit](Node*) {
    return QString(unit == ExplodedAnimation::Gap      ? "Exploded gap"
                   : unit == ExplodedAnimation::Chunks ? "Exploded chunks"
                                                       : "Exploded offset");
  };
  p.applies = isA<VolumeSink>;
  p.range = [unit](Node* node) {
    PropertyRange r;
    auto* volume = qobject_cast<VolumeSink*>(node);
    if (unit == ExplodedAnimation::Gap) {
      // Pulling the slabs apart from closed to wherever the panel has
      // the gap set is the useful default.
      r.label = "Gap:";
      r.start = 0.0;
      r.stop = volume ? volume->explodedGap() : 0.25;
    } else if (unit == ExplodedAnimation::Chunks) {
      r.label = "Chunks:";
      r.decimals = 0;
      r.lo = 2;
      r.hi = 16;
      r.start = 2;
      r.stop = volume ? volume->explodedChunks() : 4;
    } else {
      // Sliding the cuts from one end of their travel to the other
      r.label = "Offset:";
      r.decimals = 0;
      const int limit = volume ? volume->explodedOffsetLimit() : 0;
      r.lo = -limit;
      r.hi = limit;
      r.start = -limit;
      r.stop = limit;
    }
    return r;
  };
  p.make = [unit](Node* node, double start, double stop) -> ModuleAnimation* {
    auto* volume = qobject_cast<VolumeSink*>(node);
    return volume ? new ExplodedAnimation(volume, start, stop, unit) : nullptr;
  };
  p.matches = [unit](ModuleAnimation* animation, double& start, double& stop) {
    auto* sweep = qobject_cast<ExplodedAnimation*>(animation);
    if (!sweep || sweep->unit != unit) {
      return false;
    }
    start = sweep->startValue;
    stop = sweep->stopValue;
    return true;
  };
  return p;
}

AnimatableProperty cutOut(int axis)
{
  AnimatableProperty p;
  const QChar name('X' + axis);
  p.id = QString("cutOut") + name;
  p.label = [name](Node*) { return QString("Cut-out ") + name; };
  p.applies = isA<VolumeSink>;
  p.range = [axis](Node* node) {
    PropertyRange r;
    r.label = QString("Cut-out ") + QChar('X' + axis) + ":";
    auto* volume = qobject_cast<VolumeSink*>(node);
    // Sweep the cut from where it sits to the far side of the volume
    r.start = volume ? volume->cutOutPosition(axis) : 0.5;
    r.stop = r.start < 0.5 ? 1.0 : 0.0;
    return r;
  };
  p.make = [axis](Node* node, double start, double stop) -> ModuleAnimation* {
    auto* volume = qobject_cast<VolumeSink*>(node);
    return volume ? new CutOutAnimation(volume, start, stop, axis) : nullptr;
  };
  p.matches = [axis](ModuleAnimation* animation, double& start, double& stop) {
    auto* sweep = qobject_cast<CutOutAnimation*>(animation);
    if (!sweep || sweep->axis != axis) {
      return false;
    }
    start = sweep->startValue;
    stop = sweep->stopValue;
    return true;
  };
  return p;
}

} // namespace

const QList<AnimatableProperty>& animatableProperties()
{
  static const QList<AnimatableProperty> properties = {
    isoValue(),
    planeProperty<SliceSink, SliceAnimation>("slice"),
    planeProperty<ClipSink, ClipAnimation>("clip"),
    threshold(ThresholdAnimation::Lower),
    threshold(ThresholdAnimation::Upper),
    opacity(),
    solidity(),
    exploded(ExplodedAnimation::Gap),
    exploded(ExplodedAnimation::Chunks),
    exploded(ExplodedAnimation::Offset),
    cutOut(0),
    cutOut(1),
    cutOut(2),
  };
  return properties;
}

const AnimatableProperty* animatableProperty(const QString& id)
{
  for (const auto& property : animatableProperties()) {
    if (property.id == id) {
      return &property;
    }
  }
  return nullptr;
}

const AnimatableProperty* animatablePropertyOf(ModuleAnimation* animation,
                                               double& start, double& stop)
{
  if (!animation) {
    return nullptr;
  }
  for (const auto& property : animatableProperties()) {
    if (property.matches(animation, start, stop)) {
      return &property;
    }
  }
  return nullptr;
}

bool hasAnimatableProperties(pipeline::Node* node)
{
  for (const auto& property : animatableProperties()) {
    if (property.applies(node)) {
      return true;
    }
  }
  return false;
}

} // namespace tomviz
