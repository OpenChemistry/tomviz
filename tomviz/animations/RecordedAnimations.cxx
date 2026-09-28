/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RecordedAnimations.h"

#include "ActiveObjects.h"
#include "CameraViewpoints.h"
#include "ModuleAnimations.h"
#include "SolidityAnimation.h"
#include "pipeline/Pipeline.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ThresholdSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <vtkPiecewiseFunction.h>

#include <vtkMath.h>

#include <algorithm>
#include <cmath>

namespace tomviz {

using pipeline::ClipSink;
using pipeline::ContourSink;
using pipeline::LabelMapSink;
using pipeline::LegacyModuleSink;
using pipeline::ThresholdSink;
using pipeline::Pipeline;
using pipeline::VolumeSink;

namespace {

double lerp(double a, double b, double u)
{
  return a + (b - a) * u;
}

/// How far an effect that is on at one end of a leg and off (or set up
/// differently) at the other has got: collapsing out of the old state
/// (a fraction from 1 down to 0) or growing into the new one (0 up to
/// 1). Turning off collapses over the whole leg and turning on grows
/// over it, unless the exclusive other effect is on at the far end,
/// when each keeps to its half; a change of setup collapses in the
/// first half and grows in the second, passing through nothing.
struct Sweep
{
  bool into = false;    // growing into b, rather than collapsing out of a
  bool on = false;      // the effect is on at all
  double amount = 0.0;  // of the side's own extent, 0 to 1
};

Sweep sweepAt(double u, bool aOn, bool bOn, bool aOtherOn, bool bOtherOn)
{
  Sweep sweep;
  if (aOn && bOn) {
    sweep.on = true;
    sweep.into = u >= 0.5;
    sweep.amount = sweep.into ? 2 * u - 1 : 1 - 2 * u;
  } else if (aOn) {
    const double end = bOtherOn ? 0.5 : 1.0;
    sweep.on = u < end;
    sweep.amount = sweep.on ? 1 - u / end : 0.0;
  } else if (bOn) {
    const double start = aOtherOn ? 0.5 : 0.0;
    sweep.into = true;
    sweep.on = u > start;
    sweep.amount = sweep.on ? (u - start) / (1 - start) : 0.0;
  }
  return sweep;
}

/// Cut-out fractions that remove nothing: the corner of the volume the
/// cut-out grows from
std::array<double, 3> emptyCutOut(int corner)
{
  return { corner & 1 ? 1.0 : 0.0, corner & 2 ? 1.0 : 0.0,
           corner & 4 ? 1.0 : 0.0 };
}

/// Viewpoint indices that recorded a scene, in order.
QList<int> recordedAnchors()
{
  QList<int> anchors;
  auto& viewpoints = CameraViewpoints::instance();
  for (int i = 0; i < viewpoints.size(); ++i) {
    if (!viewpoints.at(i).scene.isEmpty()) {
      anchors.append(i);
    }
  }
  return anchors;
}

/// The module's recorded state at @a anchor. A recorded viewpoint that
/// lacks the module was saved before it existed, so the module was not
/// on screen there: the answer is a hidden copy of @a fallback, which
/// carries the values a fade needs.
SinkSnapshot snapshotAt(int anchor, int id, const SinkSnapshot& fallback)
{
  const auto& scene = CameraViewpoints::instance().at(anchor).scene;
  if (scene.sinks.contains(id)) {
    return scene.sinks[id];
  }
  SinkSnapshot hidden = fallback;
  hidden.visible = false;
  return hidden;
}

/// Some snapshot of the module, from whichever recorded viewpoint has
/// one, to seed the hidden copies with.
bool anySnapshot(const QList<int>& anchors, int id, SinkSnapshot& found)
{
  for (int anchor : anchors) {
    const auto& scene = CameraViewpoints::instance().at(anchor).scene;
    if (scene.sinks.contains(id)) {
      found = scene.sinks[id];
      return true;
    }
  }
  return false;
}

/// Every module id any recorded viewpoint mentions.
QSet<int> recordedIds(const QList<int>& anchors)
{
  QSet<int> ids;
  for (int anchor : anchors) {
    const auto& scene = CameraViewpoints::instance().at(anchor).scene;
    for (auto it = scene.sinks.cbegin(); it != scene.sinks.cend(); ++it) {
      ids.insert(it.key());
    }
  }
  return ids;
}

OpacityKey opacityKey(const SinkSnapshot& snapshot)
{
  return { snapshot.visible, snapshot.opacity.value_or(1.0) };
}

CutOutKey cutOutKey(const SinkSnapshot& snapshot)
{
  CutOutKey key;
  key.enabled = snapshot.cutOutEnabled.value_or(false);
  key.corner = snapshot.cutOutCorner.value_or(0);
  key.position = snapshot.cutOutPosition.value_or(
    std::array<double, 3>{ 0.5, 0.5, 0.5 });
  key.otherOn = snapshot.explodedEnabled.value_or(false);
  return key;
}

ExplodedKey explodedKey(const SinkSnapshot& snapshot)
{
  ExplodedKey key;
  key.enabled = snapshot.explodedEnabled.value_or(false);
  key.axis = snapshot.explodedAxis.value_or(2);
  key.direction = snapshot.explodedDirection.value_or(
    std::array<double, 3>{ 1.0, 1.0, 1.0 });
  key.chunks = snapshot.explodedChunks.value_or(4);
  key.gap = snapshot.explodedGap.value_or(0.25);
  key.offset = snapshot.explodedOffset.value_or(0);
  key.otherOn = snapshot.cutOutEnabled.value_or(false);
  return key;
}

ThresholdKey thresholdKey(const SinkSnapshot& snapshot)
{
  ThresholdKey key;
  key.lower = snapshot.thresholdLower.value_or(0.0);
  key.upper = snapshot.thresholdUpper.value_or(1.0);
  return key;
}

/// The labels named in @a b but not in @a a, as "3, 5, 10".
QString labelsOnlyIn(const QVector<double>& b, const QVector<double>& a)
{
  QStringList names;
  for (double value : b) {
    if (!a.contains(value)) {
      names.append(QString::number(value));
    }
  }
  return names.join(", ");
}

PlaneKey planeKey(const SinkSnapshot& snapshot)
{
  PlaneKey key;
  key.direction = snapshot.planeDirection.value_or(0);
  key.slice = snapshot.sliceIndex.value_or(0);
  key.center = snapshot.planeCenter.value_or(std::array<double, 3>{ 0, 0, 0 });
  key.normal = snapshot.planeNormal.value_or(std::array<double, 3>{ 0, 0, 1 });
  return key;
}

/// The authored animation type that can take a plane's leg over.
QString planeType(LegacyModuleSink* sink)
{
  return qobject_cast<ClipSink*>(sink) ? "clip" : "slice";
}

/// The curve that stands for a snapshot: its own while shown, an
/// all-zero copy of @a shape while hidden, or null when it has none.
vtkSmartPointer<vtkPiecewiseFunction> curveOf(const SinkSnapshot& snapshot,
                                              vtkPiecewiseFunction* shape)
{
  if (snapshot.visible) {
    return snapshot.scalarOpacity;
  }
  return shape ? zeroedCurve(shape) : nullptr;
}

/// Which property family a module's recorded state animates through.
enum class Family
{
  Curve,
  Opacity,
  Visibility
};

Family familyOf(LegacyModuleSink* sink, const SinkSnapshot& sample)
{
  if (ScalarOpacityAnimation::supports(sink) && sample.scalarOpacity) {
    return Family::Curve;
  }
  if (sample.opacity) {
    return Family::Opacity;
  }
  return Family::Visibility;
}

/// The animation type an authored animation must have to own a leg of
/// this recorded property.
QString authoredTypeFor(const QString& property)
{
  if (property == "curve") {
    return "scalarOpacity";
  }
  if (property == "iso") {
    return "contour";
  }
  return property;
}

/// Legs (start index) an authored animation of @a type on @a node owns.
/// An animation over the whole timeline owns every leg.
QSet<int> authoredSegments(pipeline::Node* node, const QString& type)
{
  QSet<int> segments;
  const int legs = std::max(0, CameraViewpoints::instance().size() - 1);
  for (auto* animation : ModuleAnimations::instance().animations()) {
    if (!animation || animation->recorded() || animation->baseNode != node ||
        animation->type() != type) {
      continue;
    }
    if (animation->segment < 0) {
      for (int leg = 0; leg < legs; ++leg) {
        segments.insert(leg);
      }
    } else {
      segments.insert(animation->segment);
    }
  }
  return segments;
}

/// True if any leg between the two anchors is owned by an authored one.
bool stretchIsAuthored(const QSet<int>& authored, int from, int to)
{
  for (int leg = from; leg < to; ++leg) {
    if (authored.contains(leg)) {
      return true;
    }
  }
  return false;
}

QString number(double value)
{
  return QString::number(value, 'g', 3);
}

} // namespace

AnchorSpan anchorSpanAt(const QList<int>& anchors, double t)
{
  AnchorSpan span;
  if (anchors.isEmpty()) {
    return span;
  }
  auto& viewpoints = CameraViewpoints::instance();
  int next = 0;
  while (next < anchors.size() && viewpoints.anchorTime(anchors[next]) <= t) {
    ++next;
  }
  if (next == 0) {
    span.from = span.to = anchors.first();
    span.u = 0.0;
  } else if (next == anchors.size()) {
    span.from = span.to = anchors.last();
    span.u = 1.0;
  } else {
    span.from = anchors[next - 1];
    span.to = anchors[next];
    // From leaving one anchor to reaching the next: an orbit at the
    // first holds the state recorded there until the camera moves on.
    const double start = viewpoints.departureTime(span.from);
    const double stop = viewpoints.anchorTime(span.to);
    span.u = stop > start ? std::clamp((t - start) / (stop - start), 0.0, 1.0)
                          : 1.0;
  }
  return span;
}

double effectiveOpacity(const OpacityKey& key)
{
  return key.visible ? key.opacity : 0.0;
}

// --- RecordedAnimation ---

void RecordedAnimation::onTimeChanged()
{
  if (!timeKeeper()) {
    return;
  }
  // Eased along with the camera: the stops are fixed points of the
  // remap, so the anchors still land where the camera arrives.
  applyPathTime(CameraViewpoints::instance().remapProgress(progress()));
}

void RecordedAnimation::applyPathTime(double t)
{
  if (!baseNode) {
    return;
  }
  auto& viewpoints = CameraViewpoints::instance();
  auto stops = viewpoints.stops();
  if (stops.size() >= 2) {
    int leg = 0;
    while (leg + 2 < stops.size() && stops[leg + 1] <= t) {
      ++leg;
    }
    if (excludedSegments.contains(leg)) {
      return;
    }
  }
  applySpan(anchorSpanAt(anchors(), t));
}

// --- Opacity ---

void RecordedOpacityAnimation::applySpan(const AnchorSpan& span)
{
  auto* sink = qobject_cast<LegacyModuleSink*>(baseNode.data());
  if (!sink || m_keys.isEmpty()) {
    return;
  }
  const OpacityKey& a = m_keys[span.from];
  const OpacityKey& b = m_keys[span.to];
  const double value = lerp(effectiveOpacity(a), effectiveOpacity(b), span.u);
  // Fully transparent is hidden. A hidden module keeps the opacity it
  // will have once shown, as the snapshots do, so a fade never leaves
  // it invisible-but-visible with an opacity of zero.
  const bool visible = value > 0.0;
  setSinkFlatOpacity(sink, visible ? value
                                   : (span.u < 0.5 ? a.opacity : b.opacity));
  if (sink->visibility() != visible) {
    sink->setVisibility(visible);
  }
}

// --- Visibility ---

void RecordedVisibilityAnimation::applySpan(const AnchorSpan& span)
{
  auto* sink = qobject_cast<LegacyModuleSink*>(baseNode.data());
  if (!sink || m_keys.isEmpty()) {
    return;
  }
  // Nothing to fade through, so switch halfway along the leg
  const bool visible = span.u < 0.5 ? m_keys[span.from] : m_keys[span.to];
  if (sink->visibility() != visible) {
    sink->setVisibility(visible);
  }
}

// --- Cut-out ---

RecordedCutOutAnimation::RecordedCutOutAnimation(
  VolumeSink* sink, const QMap<int, CutOutKey>& keys)
  : RecordedAnimation(sink), m_keys(keys)
{
}

void RecordedCutOutAnimation::applySpan(const AnchorSpan& span)
{
  auto* volume = qobject_cast<VolumeSink*>(baseNode.data());
  if (!volume || m_keys.isEmpty()) {
    return;
  }
  const auto& a = m_keys[span.from];
  const auto& b = m_keys[span.to];
  // The sink's setters ignore values that do not change.
  auto apply = [volume](int corner, const std::array<double, 3>& from,
                        const std::array<double, 3>& to, double u,
                        bool enabled) {
    volume->setCutOutCorner(corner);
    for (int axis = 0; axis < 3; ++axis) {
      volume->setCutOutPosition(axis, lerp(from[axis], to[axis], u));
    }
    volume->setCutOutEnabled(enabled);
  };

  if (a.enabled == b.enabled && (!a.enabled || a.corner == b.corner)) {
    // The same box, or none: the position slides, and a corner that
    // nobody sees flips halfway
    apply(span.u < 0.5 ? a.corner : b.corner, a.position, b.position,
          span.u, a.enabled);
    return;
  }
  // Switched on or off, or moved to another corner: the box shrinks
  // into its corner and grows out of the new one
  const auto sweep =
    sweepAt(span.u, a.enabled, b.enabled, a.otherOn, b.otherOn);
  if (!sweep.on) {
    // Not yet on, or already gone: the settings of the end it is off at
    const auto& side = sweep.into ? a : b;
    apply(side.corner, side.position, side.position, 0.0, false);
    return;
  }
  const auto& side = sweep.into ? b : a;
  apply(side.corner, emptyCutOut(side.corner), side.position, sweep.amount,
        true);
}

// --- Exploded view ---

RecordedExplodedAnimation::RecordedExplodedAnimation(
  VolumeSink* sink, const QMap<int, ExplodedKey>& keys)
  : RecordedAnimation(sink), m_keys(keys)
{
}

void RecordedExplodedAnimation::applySpan(const AnchorSpan& span)
{
  auto* volume = qobject_cast<VolumeSink*>(baseNode.data());
  if (!volume || m_keys.isEmpty()) {
    return;
  }
  const auto& a = m_keys[span.from];
  const auto& b = m_keys[span.to];
  auto apply = [volume](const ExplodedKey& setup, double gap, int offset,
                        bool enabled) {
    volume->setExplodedDirection(setup.direction[0], setup.direction[1],
                                 setup.direction[2]);
    volume->setExplodedAxis(setup.axis, /*refitCamera=*/false);
    volume->setExplodedChunks(setup.chunks);
    volume->setExplodedGap(gap);
    volume->setExplodedOffset(offset);
    volume->setExplodedEnabled(enabled, /*refitCamera=*/false);
  };

  const bool sameSlabs =
    a.axis == b.axis && a.chunks == b.chunks &&
    (a.axis != pipeline::kExplodedCustomAxis || a.direction == b.direction);
  if (a.enabled == b.enabled && (!a.enabled || sameSlabs)) {
    // The same slabs, or none: the gap and offset slide, and a setup
    // nobody sees changes halfway
    apply(span.u < 0.5 ? a : b, lerp(a.gap, b.gap, span.u),
          static_cast<int>(std::lround(lerp(a.offset, b.offset, span.u))),
          a.enabled);
    return;
  }
  // Switched on or off, or cut differently: the slabs close up and open
  // again in their new arrangement, which changes while they touch
  const auto sweep =
    sweepAt(span.u, a.enabled, b.enabled, a.otherOn, b.otherOn);
  if (!sweep.on) {
    const auto& side = sweep.into ? a : b;
    apply(side, side.gap, side.offset, false);
    return;
  }
  const auto& side = sweep.into ? b : a;
  apply(side, side.gap * sweep.amount, side.offset, true);
}

// --- Slice and clip planes ---

QString RecordedPlaneAnimation::type() const
{
  return planeType(qobject_cast<LegacyModuleSink*>(baseNode.data()));
}

void RecordedPlaneAnimation::applySpan(const AnchorSpan& span)
{
  auto* sink = qobject_cast<LegacyModuleSink*>(baseNode.data());
  if (!sink || m_keys.isEmpty()) {
    return;
  }
  const PlaneKey& a = m_keys[span.from];
  const PlaneKey& b = m_keys[span.to];
  if (a.direction == b.direction && !a.custom()) {
    applyPlane(sink, a.direction,
               static_cast<int>(std::lround(lerp(a.slice, b.slice, span.u))),
               a.center, a.normal);
  } else if (a.custom() && b.custom()) {
    std::array<double, 3> center, normal;
    double length = 0.0;
    for (int i = 0; i < 3; ++i) {
      center[i] = lerp(a.center[i], b.center[i], span.u);
      normal[i] = lerp(a.normal[i], b.normal[i], span.u);
      length += normal[i] * normal[i];
    }
    if (length < 1e-12) {
      // Opposite normals pass through zero; hold the nearer one
      normal = span.u < 0.5 ? a.normal : b.normal;
    }
    applyPlane(sink, 3, a.slice, center, normal);
  } else if (span.u <= 0.0 || span.u >= 1.0) {
    const PlaneKey& side = span.u <= 0.0 ? a : b;
    applyPlane(sink, side.direction, side.slice, side.center, side.normal);
  } else {
    // A new direction: the plane swings from one orientation to the
    // other as a custom plane, turning its normal about the axis the two
    // share, and settles into the recorded direction at the end
    std::array<double, 3> from = a.normal, to = b.normal;
    if (vtkMath::Normalize(from.data()) == 0.0 ||
        vtkMath::Normalize(to.data()) == 0.0) {
      const PlaneKey& side = span.u < 0.5 ? a : b;
      applyPlane(sink, side.direction, side.slice, side.center, side.normal);
      return;
    }
    // A slice looks the same from either side, so it takes the short way
    // round; a clip keeps its normal, which says which side is cut away
    double cosine = vtkMath::Dot(from.data(), to.data());
    if (cosine < 0 && !qobject_cast<ClipSink*>(sink)) {
      vtkMath::MultiplyScalar(to.data(), -1.0);
      cosine = -cosine;
    }
    std::array<double, 3> axis;
    vtkMath::Cross(from.data(), to.data(), axis.data());
    if (vtkMath::Normalize(axis.data()) < 1e-9) {
      // Parallel: nothing to turn, or a half turn about any axis in the
      // plane
      double other[3] = { 1, 0, 0 };
      if (std::abs(from[0]) > 0.9) {
        other[0] = 0, other[1] = 1;
      }
      vtkMath::Cross(from.data(), other, axis.data());
      vtkMath::Normalize(axis.data());
    }
    const double angle = std::acos(std::clamp(cosine, -1.0, 1.0)) * span.u;
    // Rodrigues: the from normal turned by angle about axis
    std::array<double, 3> across, normal, center;
    vtkMath::Cross(axis.data(), from.data(), across.data());
    for (int i = 0; i < 3; ++i) {
      normal[i] = from[i] * std::cos(angle) + across[i] * std::sin(angle);
      center[i] = lerp(a.center[i], b.center[i], span.u);
    }
    applyPlane(sink, 3, a.slice, center, normal);
  }
}

// --- Threshold range ---

void RecordedThresholdAnimation::applySpan(const AnchorSpan& span)
{
  auto* threshold = qobject_cast<ThresholdSink*>(baseNode.data());
  if (!threshold || m_keys.isEmpty()) {
    return;
  }
  const ThresholdKey& a = m_keys[span.from];
  const ThresholdKey& b = m_keys[span.to];
  const double lower = lerp(a.lower, b.lower, span.u);
  const double upper = lerp(a.upper, b.upper, span.u);
  if (threshold->lowerThreshold() != lower ||
      threshold->upperThreshold() != upper) {
    threshold->setThresholdRange(lower, upper);
  }
}

// --- Hidden labels ---

void RecordedLabelsAnimation::applySpan(const AnchorSpan& span)
{
  auto* labels = qobject_cast<LabelMapSink*>(baseNode.data());
  if (!labels || m_keys.isEmpty()) {
    return;
  }
  // Nothing to fade through, so switch halfway along the leg. The sink
  // compares before changing anything.
  labels->setHiddenLabels(span.u < 0.5 ? m_keys[span.from] : m_keys[span.to]);
}

// --- Iso value ---

void RecordedIsoAnimation::applySpan(const AnchorSpan& span)
{
  auto* contour = qobject_cast<ContourSink*>(baseNode.data());
  if (!contour || m_keys.isEmpty()) {
    return;
  }
  const double value = lerp(m_keys[span.from], m_keys[span.to], span.u);
  if (contour->isoValue() != value) {
    contour->setIsoValue(value);
  }
}

// --- Solidity ---

void RecordedSolidityAnimation::applySpan(const AnchorSpan& span)
{
  auto* volume = qobject_cast<VolumeSink*>(baseNode.data());
  if (!volume || m_keys.isEmpty()) {
    return;
  }
  const double value =
    SolidityAnimation::blend(m_keys[span.from], m_keys[span.to], span.u);
  if (volume->solidity() != value) {
    volume->setSolidity(value);
  }
}

// --- Curve ---

bool RecordedCurveAnimation::legIsExcluded(double t) const
{
  auto stops = CameraViewpoints::instance().stops();
  if (stops.size() < 2) {
    return false;
  }
  int leg = 0;
  while (leg + 2 < stops.size() && stops[leg + 1] <= t) {
    ++leg;
  }
  return m_excludedSegments.contains(leg);
}

void RecordedCurveAnimation::onTimeChanged()
{
  if (!timeKeeper()) {
    return;
  }
  const double t = CameraViewpoints::instance().remapProgress(progress());
  if (!legIsExcluded(t)) {
    applyProgress(t);
  }
}

void RecordedCurveAnimation::onPlaybackStarted()
{
  if (!legIsExcluded(0.0)) {
    applyProgress(0.0);
  }
}

void RecordedCurveAnimation::curveApplied(vtkPiecewiseFunction* current)
{
  auto* target = sink();
  if (!target || !current) {
    return;
  }
  bool any = false;
  double node[4];
  for (int i = 0; i < current->GetSize() && !any; ++i) {
    current->GetNodeValue(i, node);
    any = node[1] > 0.0;
  }
  if (target->visibility() != any) {
    target->setVisibility(any);
  }
}

// --- RecordedAnimations ---

RecordedAnimations& RecordedAnimations::instance()
{
  static RecordedAnimations animations;
  return animations;
}

void RecordedAnimations::install()
{
  if (m_installed) {
    return;
  }
  m_installed = true;
  auto resync = [this]() { sync(ActiveObjects::instance().pipeline()); };
  connect(&CameraViewpoints::instance(), &CameraViewpoints::changed, this,
          resync);
  // Authored animations take legs away from recorded ones and give
  // them back when removed. The sync itself edits the list, so it is
  // guarded against re-entering through this connection.
  connect(&ModuleAnimations::instance(), &ModuleAnimations::changed, this,
          resync);
  resync();
}

QSet<int> RecordedAnimations::recordedNodeIds() const
{
  return recordedIds(recordedAnchors());
}

void RecordedAnimations::sync(Pipeline* pipeline)
{
  if (m_syncing) {
    return;
  }
  m_syncing = true;

  auto& registry = ModuleAnimations::instance();
  for (auto* animation : registry.animations()) {
    if (animation->recorded()) {
      registry.remove(animation);
    }
  }

  const auto anchors = recordedAnchors();
  if (pipeline && anchors.size() >= 2) {
    for (int id : recordedIds(anchors)) {
      auto* sink = qobject_cast<LegacyModuleSink*>(pipeline->nodeById(id));
      SinkSnapshot sample;
      if (!sink || !anySnapshot(anchors, id, sample)) {
        continue;
      }
      const Family family = familyOf(sink, sample);
      auto* volume = qobject_cast<VolumeSink*>(sink);

      if (family == Family::Curve) {
        const auto authored = authoredSegments(sink, "scalarOpacity");
        QList<OpacityKeyframe> keyframes;
        bool differs = false;
        vtkSmartPointer<vtkPiecewiseFunction> previous;
        for (int anchor : anchors) {
          auto curve =
            curveOf(snapshotAt(anchor, id, sample), sample.scalarOpacity);
          if (!curve) {
            continue;
          }
          differs = differs || (previous && !opacityCurvesEqual(previous, curve));
          previous = curve;
          keyframes.append({ anchor, curve });
        }
        if (differs && volume) {
          registry.add(new RecordedCurveAnimation(volume, keyframes, authored));
        }
      } else if (family == Family::Opacity) {
        const auto authored = authoredSegments(sink, "opacity");
        QMap<int, OpacityKey> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = opacityKey(snapshotAt(anchor, id, sample));
          differs = differs || effectiveOpacity(keys[anchor]) !=
                                 effectiveOpacity(keys[anchors.first()]);
        }
        if (differs) {
          auto* animation = new RecordedOpacityAnimation(sink, keys);
          animation->excludedSegments = authored;
          registry.add(animation);
        }
      } else {
        QMap<int, bool> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = snapshotAt(anchor, id, sample).visible;
          differs = differs || keys[anchor] != keys[anchors.first()];
        }
        if (differs) {
          registry.add(new RecordedVisibilityAnimation(sink, keys));
        }
      }

      if (volume && sample.cutOutEnabled) {
        QMap<int, CutOutKey> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = cutOutKey(snapshotAt(anchor, id, sample));
          differs = differs || !(keys[anchor] == keys[anchors.first()]);
        }
        if (differs) {
          auto* animation = new RecordedCutOutAnimation(volume, keys);
          animation->excludedSegments = authoredSegments(sink, "cutOut");
          registry.add(animation);
        }
      }
      if (volume && sample.explodedEnabled) {
        QMap<int, ExplodedKey> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = explodedKey(snapshotAt(anchor, id, sample));
          differs = differs || !(keys[anchor] == keys[anchors.first()]);
        }
        if (differs) {
          auto* animation = new RecordedExplodedAnimation(volume, keys);
          animation->excludedSegments = authoredSegments(sink, "exploded");
          registry.add(animation);
        }
      }
      if (sample.planeDirection) {
        QMap<int, PlaneKey> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = planeKey(snapshotAt(anchor, id, sample));
          differs = differs || !(keys[anchor] == keys[anchors.first()]);
        }
        if (differs) {
          auto* animation = new RecordedPlaneAnimation(sink, keys);
          animation->excludedSegments = authoredSegments(sink, planeType(sink));
          registry.add(animation);
        }
      }
      if (volume && sample.solidity) {
        QMap<int, double> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = snapshotAt(anchor, id, sample).solidity.value_or(
            *sample.solidity);
          differs = differs || keys[anchor] != keys[anchors.first()];
        }
        if (differs) {
          auto* animation = new RecordedSolidityAnimation(volume, keys);
          animation->excludedSegments = authoredSegments(sink, "solidity");
          registry.add(animation);
        }
      }
      if (sample.isoValue) {
        QMap<int, double> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = snapshotAt(anchor, id, sample).isoValue.value_or(
            *sample.isoValue);
          differs = differs || keys[anchor] != keys[anchors.first()];
        }
        if (differs) {
          auto* animation = new RecordedIsoAnimation(sink, keys);
          animation->excludedSegments = authoredSegments(sink, "contour");
          registry.add(animation);
        }
      }
      if (sample.thresholdLower) {
        QMap<int, ThresholdKey> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = thresholdKey(snapshotAt(anchor, id, sample));
          differs = differs || !(keys[anchor] == keys[anchors.first()]);
        }
        if (differs) {
          auto* animation = new RecordedThresholdAnimation(sink, keys);
          animation->excludedSegments = authoredSegments(sink, "threshold");
          registry.add(animation);
        }
      }
      if (sample.hiddenLabels) {
        QMap<int, QVector<double>> keys;
        bool differs = false;
        for (int anchor : anchors) {
          keys[anchor] = snapshotAt(anchor, id, sample)
                           .hiddenLabels.value_or(*sample.hiddenLabels);
          differs = differs || keys[anchor] != keys[anchors.first()];
        }
        if (differs) {
          registry.add(new RecordedLabelsAnimation(sink, keys));
        }
      }
    }
  }

  m_syncing = false;
}

QList<RecordedChange> RecordedAnimations::changes(Pipeline* pipeline) const
{
  QList<RecordedChange> rows;
  const auto anchors = recordedAnchors();
  if (!pipeline || anchors.size() < 2) {
    return rows;
  }

  // Rows come out grouped by leg, then by module in pipeline order, so
  // the list reads as a story rather than a hash walk.
  QList<int> ids;
  for (auto* node : pipeline->nodes()) {
    if (auto* sink = qobject_cast<LegacyModuleSink*>(node)) {
      int id = pipeline->nodeId(sink);
      SinkSnapshot unused;
      if (anySnapshot(anchors, id, unused)) {
        ids.append(id);
      }
    }
  }

  for (int step = 0; step + 1 < anchors.size(); ++step) {
    const int from = anchors[step];
    const int to = anchors[step + 1];
    for (int id : ids) {
      auto* sink = qobject_cast<LegacyModuleSink*>(pipeline->nodeById(id));
      SinkSnapshot sample;
      if (!sink || !anySnapshot(anchors, id, sample)) {
        continue;
      }
      const auto a = snapshotAt(from, id, sample);
      const auto b = snapshotAt(to, id, sample);
      auto* volume = qobject_cast<VolumeSink*>(sink);
      auto row = [&](const QString& property, const QString& description,
                     std::optional<double> start = std::nullopt,
                     std::optional<double> stop = std::nullopt,
                     const QString& controlProperty = QString()) {
        if (stretchIsAuthored(authoredSegments(sink, authoredTypeFor(property)),
                              from, to)) {
          return;
        }
        rows.append({ id, property, controlProperty, from, to, description,
                      start, stop });
      };

      const Family family = familyOf(sink, sample);
      if (family == Family::Curve) {
        auto curveA = curveOf(a, sample.scalarOpacity);
        auto curveB = curveOf(b, sample.scalarOpacity);
        if (curveA && curveB && !opacityCurvesEqual(curveA, curveB)) {
          row("curve", !a.visible   ? "fades in"
                       : !b.visible ? "fades out"
                                    : "opacity curve changes");
        }
      } else if (family == Family::Opacity) {
        const double start = effectiveOpacity(opacityKey(a));
        const double stop = effectiveOpacity(opacityKey(b));
        if (start != stop) {
          row("opacity",
              start == 0.0 ? "fades in to " + number(stop)
              : stop == 0.0 ? "fades out from " + number(start)
                            : "opacity " + number(start) + " to " + number(stop),
              start, stop);
        }
      } else if (a.visible != b.visible) {
        row("visibility", b.visible ? "shown" : "hidden");
      }

      if (volume && sample.cutOutEnabled) {
        const auto ka = cutOutKey(a);
        const auto kb = cutOutKey(b);
        if (!(ka == kb)) {
          // A move along one axis is something the controls can show
          int moved = -1;
          int movedCount = 0;
          for (int axis = 0; axis < 3; ++axis) {
            if (ka.position[axis] != kb.position[axis]) {
              moved = axis;
              ++movedCount;
            }
          }
          if (ka.enabled != kb.enabled) {
            row("cutOut", kb.enabled ? "cut-out on" : "cut-out off");
          } else if (movedCount == 1 && ka.corner == kb.corner) {
            row("cutOut",
                QString("cut-out %1 %2 to %3")
                  .arg(QChar('X' + moved))
                  .arg(number(ka.position[moved]))
                  .arg(number(kb.position[moved])),
                ka.position[moved], kb.position[moved],
                QString("cutOut") + QChar('X' + moved));
          } else {
            row("cutOut", "cut-out moves");
          }
        }
      }
      if (volume && sample.explodedEnabled) {
        const auto ka = explodedKey(a);
        const auto kb = explodedKey(b);
        if (!(ka == kb)) {
          if (ka.enabled != kb.enabled) {
            row("exploded",
                kb.enabled ? "exploded view on" : "exploded view off");
          } else if (ka.axis == kb.axis && ka.direction == kb.direction &&
                     ka.chunks == kb.chunks && ka.offset == kb.offset) {
            row("exploded",
                "exploded gap " + number(ka.gap) + " to " + number(kb.gap),
                ka.gap, kb.gap, "explodedGap");
          } else if (ka.axis == kb.axis && ka.direction == kb.direction &&
                     ka.gap == kb.gap && ka.offset == kb.offset) {
            row("exploded",
                QString("exploded chunks %1 to %2")
                  .arg(ka.chunks)
                  .arg(kb.chunks),
                ka.chunks, kb.chunks, "explodedChunks");
          } else if (ka.axis == kb.axis && ka.direction == kb.direction &&
                     ka.gap == kb.gap && ka.chunks == kb.chunks) {
            row("exploded",
                QString("exploded offset %1 to %2 voxels")
                  .arg(ka.offset)
                  .arg(kb.offset),
                ka.offset, kb.offset, "explodedOffset");
          } else {
            row("exploded", "exploded view changes");
          }
        }
      }
      if (sample.planeDirection) {
        const auto ka = planeKey(a);
        const auto kb = planeKey(b);
        if (!(ka == kb)) {
          const QString property = planeType(sink);
          if (ka.direction == kb.direction && !ka.custom()) {
            row(property,
                property + " " + QString::number(ka.slice) + " to " +
                  QString::number(kb.slice),
                ka.slice, kb.slice);
          } else if (ka.custom() && kb.custom()) {
            row(property, "plane moves");
          } else {
            row(property, "plane direction changes");
          }
        }
      }
      if (volume && sample.solidity) {
        const double start = a.solidity.value_or(*sample.solidity);
        const double stop = b.solidity.value_or(*sample.solidity);
        if (start != stop) {
          row("solidity", "solidity " + number(start) + " to " + number(stop),
              start, stop);
        }
      }
      if (sample.isoValue) {
        const double start = a.isoValue.value_or(*sample.isoValue);
        const double stop = b.isoValue.value_or(*sample.isoValue);
        if (start != stop) {
          row("iso", "iso value " + number(start) + " to " + number(stop),
              start, stop);
        }
      }
      if (sample.thresholdLower) {
        const auto ka = thresholdKey(a);
        const auto kb = thresholdKey(b);
        if (!(ka == kb)) {
          if (ka.upper == kb.upper) {
            row("threshold",
                "lower threshold " + number(ka.lower) + " to " +
                  number(kb.lower),
                ka.lower, kb.lower, "thresholdLower");
          } else if (ka.lower == kb.lower) {
            row("threshold",
                "upper threshold " + number(ka.upper) + " to " +
                  number(kb.upper),
                ka.upper, kb.upper, "thresholdUpper");
          } else {
            row("threshold", "threshold range changes");
          }
        }
      }
      if (sample.hiddenLabels) {
        const auto la = a.hiddenLabels.value_or(*sample.hiddenLabels);
        const auto lb = b.hiddenLabels.value_or(*sample.hiddenLabels);
        if (la != lb) {
          const QString hidden = labelsOnlyIn(lb, la);
          const QString shown = labelsOnlyIn(la, lb);
          QString description;
          if (!hidden.isEmpty()) {
            description = "labels " + hidden + " hidden";
          }
          if (!shown.isEmpty()) {
            description += (description.isEmpty() ? "labels " : ", ") +
                           shown + " shown";
          }
          row("labels", description);
        }
      }
    }
  }
  return rows;
}

void RecordedAnimations::remove(const RecordedChange& change,
                                Pipeline* pipeline)
{
  auto& viewpoints = CameraViewpoints::instance();
  if (!pipeline || change.fromAnchor < 0 || change.toAnchor < 0 ||
      change.toAnchor >= viewpoints.size() ||
      change.fromAnchor >= viewpoints.size()) {
    return;
  }
  const auto anchors = recordedAnchors();
  SinkSnapshot sample;
  if (!anySnapshot(anchors, change.nodeId, sample)) {
    return;
  }
  const auto earlier = snapshotAt(change.fromAnchor, change.nodeId, sample);

  // The later viewpoint takes the earlier value of just this property,
  // so the leg no longer changes it and later legs start from there.
  Viewpoint viewpoint = viewpoints.at(change.toAnchor);
  auto& sinks = viewpoint.scene.sinks;
  if (!sinks.contains(change.nodeId)) {
    SinkSnapshot hidden = sample;
    hidden.visible = false;
    sinks.insert(change.nodeId, hidden);
  }
  SinkSnapshot& later = sinks[change.nodeId];
  if (change.property == "opacity") {
    later.visible = earlier.visible;
    later.opacity = earlier.opacity;
  } else if (change.property == "visibility") {
    later.visible = earlier.visible;
  } else if (change.property == "curve") {
    later.visible = earlier.visible;
    later.scalarOpacity = nullptr;
    if (earlier.scalarOpacity) {
      later.scalarOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
      later.scalarOpacity->DeepCopy(earlier.scalarOpacity);
    }
  } else if (change.property == "cutOut") {
    later.cutOutEnabled = earlier.cutOutEnabled;
    later.cutOutCorner = earlier.cutOutCorner;
    later.cutOutPosition = earlier.cutOutPosition;
  } else if (change.property == "exploded") {
    later.explodedEnabled = earlier.explodedEnabled;
    later.explodedAxis = earlier.explodedAxis;
    later.explodedDirection = earlier.explodedDirection;
    later.explodedChunks = earlier.explodedChunks;
    later.explodedGap = earlier.explodedGap;
    later.explodedOffset = earlier.explodedOffset;
  } else if (change.property == "slice" || change.property == "clip") {
    later.planeDirection = earlier.planeDirection;
    later.sliceIndex = earlier.sliceIndex;
    later.planeCenter = earlier.planeCenter;
    later.planeNormal = earlier.planeNormal;
  } else if (change.property == "iso") {
    later.isoValue = earlier.isoValue;
  } else if (change.property == "solidity") {
    later.solidity = earlier.solidity;
  } else if (change.property == "threshold") {
    later.thresholdLower = earlier.thresholdLower;
    later.thresholdUpper = earlier.thresholdUpper;
  } else if (change.property == "labels") {
    later.hiddenLabels = earlier.hiddenLabels;
  } else {
    return;
  }
  viewpoints.replace(change.toAnchor, viewpoint);
}

} // namespace tomviz
