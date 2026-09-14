/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RecordedAnimations.h"

#include "ActiveObjects.h"
#include "CameraViewpoints.h"
#include "ModuleAnimations.h"
#include "pipeline/Pipeline.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <vtkPiecewiseFunction.h>

#include <algorithm>
#include <cmath>

namespace tomviz {

using pipeline::LegacyModuleSink;
using pipeline::Pipeline;
using pipeline::VolumeSink;

namespace {

double lerp(double a, double b, double u)
{
  return a + (b - a) * u;
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
  return key;
}

ExplodedKey explodedKey(const SinkSnapshot& snapshot)
{
  ExplodedKey key;
  key.enabled = snapshot.explodedEnabled.value_or(false);
  key.axis = snapshot.explodedAxis.value_or(2);
  key.chunks = snapshot.explodedChunks.value_or(4);
  key.gap = snapshot.explodedGap.value_or(0.25);
  return key;
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
    const double start = viewpoints.anchorTime(span.from);
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
  // The position slides; the corner and the switch flip halfway. The
  // sink's setters ignore values that do not change.
  volume->setCutOutCorner(span.u < 0.5 ? a.corner : b.corner);
  for (int axis = 0; axis < 3; ++axis) {
    volume->setCutOutPosition(axis,
                              lerp(a.position[axis], b.position[axis], span.u));
  }
  volume->setCutOutEnabled(span.u < 0.5 ? a.enabled : b.enabled);
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
  const auto& side = span.u < 0.5 ? a : b;
  volume->setExplodedAxis(side.axis);
  volume->setExplodedChunks(side.chunks);
  volume->setExplodedGap(lerp(a.gap, b.gap, span.u));
  volume->setExplodedEnabled(side.enabled);
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
      auto row = [&](const QString& property, const QString& description) {
        if (stretchIsAuthored(authoredSegments(sink, authoredTypeFor(property)),
                              from, to)) {
          return;
        }
        rows.append({ id, property, from, to, description });
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
          row("opacity", start == 0.0 ? "fades in to " + number(stop)
                         : stop == 0.0 ? "fades out from " + number(start)
                                       : "opacity " + number(start) + " to " +
                                           number(stop));
        }
      } else if (a.visible != b.visible) {
        row("visibility", b.visible ? "shown" : "hidden");
      }

      if (volume && sample.cutOutEnabled) {
        const auto ka = cutOutKey(a);
        const auto kb = cutOutKey(b);
        if (!(ka == kb)) {
          row("cutOut", ka.enabled != kb.enabled
                          ? (kb.enabled ? "cut-out on" : "cut-out off")
                          : "cut-out moves");
        }
      }
      if (volume && sample.explodedEnabled) {
        const auto ka = explodedKey(a);
        const auto kb = explodedKey(b);
        if (!(ka == kb)) {
          row("exploded",
              ka.enabled != kb.enabled
                ? (kb.enabled ? "exploded view on" : "exploded view off")
              : ka.gap != kb.gap ? "exploded gap " + number(ka.gap) + " to " +
                                     number(kb.gap)
                                 : "exploded view changes");
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
    later.explodedChunks = earlier.explodedChunks;
    later.explodedGap = earlier.explodedGap;
  } else {
    return;
  }
  viewpoints.replace(change.toAnchor, viewpoint);
}

} // namespace tomviz
