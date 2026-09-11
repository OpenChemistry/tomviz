/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SceneSnapshot.h"

#include "ModuleAnimation.h"
#include "ModuleAnimations.h"
#include "OpacityInterpolation.h"
#include "ScalarOpacityAnimation.h"
#include "Utilities.h"
#include "pipeline/Pipeline.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSMProxy.h>

#include <QJsonArray>
#include <QSet>

#include <cmath>

namespace tomviz {

using pipeline::ClipSink;
using pipeline::ContourSink;
using pipeline::LegacyModuleSink;
using pipeline::Pipeline;
using pipeline::SliceSink;
using pipeline::VolumeSink;

namespace {

// Flat opacity getters/setters for the module types that have one
std::optional<double> flatOpacity(LegacyModuleSink* sink)
{
  if (auto* contour = qobject_cast<ContourSink*>(sink)) {
    return contour->opacity();
  } else if (auto* slice = qobject_cast<SliceSink*>(sink)) {
    return slice->opacity();
  } else if (auto* clip = qobject_cast<ClipSink*>(sink)) {
    return clip->opacity();
  }
  return std::nullopt;
}

void setFlatOpacity(LegacyModuleSink* sink, double value)
{
  if (auto* contour = qobject_cast<ContourSink*>(sink)) {
    contour->setOpacity(value);
  } else if (auto* slice = qobject_cast<SliceSink*>(sink)) {
    slice->setOpacity(value);
  } else if (auto* clip = qobject_cast<ClipSink*>(sink)) {
    clip->setOpacity(value);
  }
}

vtkPiecewiseFunction* liveOpacityCurve(VolumeSink* volume)
{
  auto* proxy = volume->opacityMap();
  return proxy ? vtkPiecewiseFunction::SafeDownCast(proxy->GetClientSideObject())
               : nullptr;
}

void colorMapRange(VolumeSink* volume, double range[2])
{
  range[0] = 0.0;
  range[1] = 1.0;
  auto data = volume->volumeData();
  if (data && data->isValid()) {
    auto r = data->colorMapRange();
    range[0] = r[0];
    range[1] = r[1];
  }
}

bool curvesEqual(vtkPiecewiseFunction* a, vtkPiecewiseFunction* b)
{
  if (!a || !b || a->GetSize() != b->GetSize()) {
    return false;
  }
  double pa[4], pb[4];
  for (int i = 0; i < a->GetSize(); ++i) {
    a->GetNodeValue(i, pa);
    b->GetNodeValue(i, pb);
    for (int k = 0; k < 4; ++k) {
      if (std::abs(pa[k] - pb[k]) > 1e-12) {
        return false;
      }
    }
  }
  return true;
}

// The same curve with every opacity at zero: the "hidden" end of a fade
vtkSmartPointer<vtkPiecewiseFunction> zeroed(vtkPiecewiseFunction* curve)
{
  auto out = vtkSmartPointer<vtkPiecewiseFunction>::New();
  double p[4];
  for (int i = 0; i < curve->GetSize(); ++i) {
    curve->GetNodeValue(i, p);
    out->AddPoint(p[0], 0.0, p[2], p[3]);
  }
  return out;
}

bool hasExplicitAnimation(LegacyModuleSink* sink, const QString& type)
{
  for (auto* animation : ModuleAnimations::instance().animations()) {
    if (animation && animation->baseNode == sink && animation->type() == type) {
      return true;
    }
  }
  return false;
}

double lerp(double a, double b, double u)
{
  return a + (b - a) * u;
}

} // namespace

SinkSnapshot SinkSnapshot::capture(LegacyModuleSink* sink)
{
  SinkSnapshot snapshot;
  snapshot.visible = sink->visibility();
  snapshot.opacity = flatOpacity(sink);
  if (auto* volume = qobject_cast<VolumeSink*>(sink)) {
    if (ScalarOpacityAnimation::supports(volume)) {
      if (auto* live = liveOpacityCurve(volume)) {
        snapshot.scalarOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
        snapshot.scalarOpacity->DeepCopy(live);
        // The editor's end placeholders are chart furniture, not data
        removePlaceholderNodes(snapshot.scalarOpacity);
      }
    }
    snapshot.cutOutEnabled = volume->cutOutEnabled();
    snapshot.cutOutCorner = volume->cutOutCorner();
    snapshot.cutOutPosition = { { volume->cutOutPosition(0),
                                  volume->cutOutPosition(1),
                                  volume->cutOutPosition(2) } };
    snapshot.explodedEnabled = volume->explodedEnabled();
    snapshot.explodedAxis = volume->explodedAxis();
    snapshot.explodedChunks = volume->explodedChunks();
    snapshot.explodedGap = volume->explodedGap();
  }
  return snapshot;
}

QJsonObject SinkSnapshot::serialize() const
{
  QJsonObject json;
  json["visible"] = visible;
  if (opacity) {
    json["opacity"] = *opacity;
  }
  if (scalarOpacity) {
    json["scalarOpacity"] = tomviz::serialize(scalarOpacity.Get());
  }
  if (cutOutEnabled) {
    QJsonObject cutOut;
    cutOut["enabled"] = *cutOutEnabled;
    cutOut["corner"] = cutOutCorner.value_or(0);
    auto pos = cutOutPosition.value_or(std::array<double, 3>{ 0.5, 0.5, 0.5 });
    cutOut["position"] = QJsonArray{ pos[0], pos[1], pos[2] };
    json["cutOut"] = cutOut;
  }
  if (explodedEnabled) {
    QJsonObject exploded;
    exploded["enabled"] = *explodedEnabled;
    exploded["axis"] = explodedAxis.value_or(2);
    exploded["chunks"] = explodedChunks.value_or(4);
    exploded["gap"] = explodedGap.value_or(0.25);
    json["exploded"] = exploded;
  }
  return json;
}

SinkSnapshot SinkSnapshot::deserialize(const QJsonObject& json)
{
  SinkSnapshot snapshot;
  snapshot.visible = json["visible"].toBool(true);
  if (json.contains("opacity")) {
    snapshot.opacity = json["opacity"].toDouble();
  }
  if (json.contains("scalarOpacity")) {
    snapshot.scalarOpacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
    tomviz::deserialize(snapshot.scalarOpacity.Get(),
                        json["scalarOpacity"].toObject());
  }
  if (json.contains("exploded")) {
    auto exploded = json["exploded"].toObject();
    snapshot.explodedEnabled = exploded["enabled"].toBool();
    snapshot.explodedAxis = exploded["axis"].toInt(2);
    snapshot.explodedChunks = exploded["chunks"].toInt(4);
    snapshot.explodedGap = exploded["gap"].toDouble(0.25);
  }
  if (json.contains("cutOut")) {
    auto cutOut = json["cutOut"].toObject();
    snapshot.cutOutEnabled = cutOut["enabled"].toBool();
    snapshot.cutOutCorner = cutOut["corner"].toInt();
    auto pos = cutOut["position"].toArray();
    if (pos.size() == 3) {
      snapshot.cutOutPosition = { { pos[0].toDouble(), pos[1].toDouble(),
                                    pos[2].toDouble() } };
    }
  }
  return snapshot;
}

SceneSnapshot SceneSnapshot::capture(Pipeline* pipeline)
{
  SceneSnapshot scene;
  if (!pipeline) {
    return scene;
  }
  for (auto* node : pipeline->nodes()) {
    if (auto* sink = qobject_cast<LegacyModuleSink*>(node)) {
      scene.sinks.insert(pipeline->nodeId(sink), SinkSnapshot::capture(sink));
    }
  }
  return scene;
}

void SceneSnapshot::apply(Pipeline* pipeline) const
{
  if (!pipeline) {
    return;
  }
  for (auto it = sinks.cbegin(); it != sinks.cend(); ++it) {
    auto* sink = qobject_cast<LegacyModuleSink*>(pipeline->nodeById(it.key()));
    if (!sink) {
      continue;
    }
    const auto& snapshot = it.value();
    if (snapshot.opacity) {
      setFlatOpacity(sink, *snapshot.opacity);
    }
    if (auto* volume = qobject_cast<VolumeSink*>(sink)) {
      if (snapshot.scalarOpacity) {
        if (auto* live = liveOpacityCurve(volume)) {
          double range[2];
          colorMapRange(volume, range);
          live->DeepCopy(snapshot.scalarOpacity);
          addPlaceholderNodes(live, range);
          live->Modified();
        }
      }
      if (snapshot.cutOutEnabled) {
        volume->setCutOutCorner(snapshot.cutOutCorner.value_or(0));
        if (snapshot.cutOutPosition) {
          for (int axis = 0; axis < 3; ++axis) {
            volume->setCutOutPosition(axis, (*snapshot.cutOutPosition)[axis]);
          }
        }
        volume->setCutOutEnabled(*snapshot.cutOutEnabled);
      }
      if (snapshot.explodedEnabled) {
        volume->setExplodedAxis(snapshot.explodedAxis.value_or(2));
        volume->setExplodedChunks(snapshot.explodedChunks.value_or(4));
        volume->setExplodedGap(snapshot.explodedGap.value_or(0.25));
        volume->setExplodedEnabled(*snapshot.explodedEnabled);
      }
    }
    if (sink->visibility() != snapshot.visible) {
      sink->setVisibility(snapshot.visible);
    }
  }
}

QJsonObject SceneSnapshot::serialize() const
{
  QJsonObject json;
  for (auto it = sinks.cbegin(); it != sinks.cend(); ++it) {
    json[QString::number(it.key())] = it.value().serialize();
  }
  return json;
}

SceneSnapshot SceneSnapshot::deserialize(const QJsonObject& json)
{
  SceneSnapshot scene;
  for (auto it = json.begin(); it != json.end(); ++it) {
    bool ok = false;
    int id = it.key().toInt(&ok);
    if (ok) {
      scene.sinks.insert(id, SinkSnapshot::deserialize(it.value().toObject()));
    }
  }
  return scene;
}

void applySceneTransition(Pipeline* pipeline, const SceneSnapshot& from,
                          const SceneSnapshot& to, double u,
                          QSet<int>& overriddenVolumes)
{
  if (!pipeline) {
    return;
  }
  u = std::clamp(u, 0.0, 1.0);

  auto ids = QSet<int>(from.sinks.keyBegin(), from.sinks.keyEnd());
  ids.unite(QSet<int>(to.sinks.keyBegin(), to.sinks.keyEnd()));
  for (int id : ids) {
    auto* sink = qobject_cast<LegacyModuleSink*>(pipeline->nodeById(id));
    if (!sink) {
      continue;
    }
    // A module recorded at only one end holds that state
    const SinkSnapshot& a = from.sinks.contains(id) ? from.sinks[id] : to.sinks[id];
    const SinkSnapshot& b = to.sinks.contains(id) ? to.sinks[id] : from.sinks[id];

    bool visibilityChanges = a.visible != b.visible;
    bool fades = false;

    // Flat opacity, fading through zero when the module appears or goes
    if (a.opacity && b.opacity && !hasExplicitAnimation(sink, "opacity")) {
      double start = a.visible ? *a.opacity : 0.0;
      double stop = b.visible ? *b.opacity : 0.0;
      if (start != stop) {
        setFlatOpacity(sink, lerp(start, stop, u));
        fades = true;
      }
    }

    if (auto* volume = qobject_cast<VolumeSink*>(sink)) {
      if (a.scalarOpacity && b.scalarOpacity &&
          !hasExplicitAnimation(sink, "scalarOpacity")) {
        auto start = a.visible ? a.scalarOpacity : zeroed(b.scalarOpacity);
        auto stop = b.visible ? b.scalarOpacity : zeroed(a.scalarOpacity);
        if (!curvesEqual(start, stop)) {
          double range[2];
          colorMapRange(volume, range);
          vtkNew<vtkPiecewiseFunction> blend;
          interpolateOpacity(start, stop, u, range, blend);
          volume->setAnimatedScalarOpacity(blend);
          overriddenVolumes.insert(id);
          fades = true;
        }
      }
      if (a.cutOutEnabled && b.cutOutEnabled) {
        if (a.cutOutPosition && b.cutOutPosition &&
            *a.cutOutPosition != *b.cutOutPosition) {
          for (int axis = 0; axis < 3; ++axis) {
            volume->setCutOutPosition(
              axis, lerp((*a.cutOutPosition)[axis], (*b.cutOutPosition)[axis], u));
          }
        }
        int corner = u < 0.5 ? a.cutOutCorner.value_or(0) : b.cutOutCorner.value_or(0);
        if (a.cutOutCorner != b.cutOutCorner && volume->cutOutCorner() != corner) {
          volume->setCutOutCorner(corner);
        }
        bool enabled = u < 0.5 ? *a.cutOutEnabled : *b.cutOutEnabled;
        if (*a.cutOutEnabled != *b.cutOutEnabled &&
            volume->cutOutEnabled() != enabled) {
          volume->setCutOutEnabled(enabled);
        }
      }
      if (a.explodedEnabled && b.explodedEnabled) {
        // The gap slides; everything else switches halfway
        if (a.explodedGap && b.explodedGap && *a.explodedGap != *b.explodedGap) {
          volume->setExplodedGap(lerp(*a.explodedGap, *b.explodedGap, u));
        }
        const SinkSnapshot& side = u < 0.5 ? a : b;
        if (a.explodedAxis != b.explodedAxis && side.explodedAxis &&
            volume->explodedAxis() != *side.explodedAxis) {
          volume->setExplodedAxis(*side.explodedAxis);
        }
        if (a.explodedChunks != b.explodedChunks && side.explodedChunks &&
            volume->explodedChunks() != *side.explodedChunks) {
          volume->setExplodedChunks(*side.explodedChunks);
        }
        if (*a.explodedEnabled != *b.explodedEnabled &&
            volume->explodedEnabled() != *side.explodedEnabled) {
          volume->setExplodedEnabled(*side.explodedEnabled);
        }
      }
    }

    if (visibilityChanges) {
      // With a fade the module stays shown until it is fully transparent
      bool visible = fades ? (u >= 1.0 ? b.visible : (a.visible || b.visible))
                           : (u < 0.5 ? a.visible : b.visible);
      if (sink->visibility() != visible) {
        sink->setVisibility(visible);
      }
    }
  }
}

} // namespace tomviz
