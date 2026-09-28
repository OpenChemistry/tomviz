/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SceneSnapshot.h"

#include "OpacityInterpolation.h"
#include "ScalarOpacityAnimation.h"
#include "Utilities.h"
#include "pipeline/Pipeline.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/SegmentSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ThresholdSink.h"
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
using pipeline::LabelMapSink;
using pipeline::LegacyModuleSink;
using pipeline::Pipeline;
using pipeline::SegmentSink;
using pipeline::SliceSink;
using pipeline::ThresholdSink;
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
  } else if (auto* threshold = qobject_cast<ThresholdSink*>(sink)) {
    return threshold->opacity();
  } else if (auto* segment = qobject_cast<SegmentSink*>(sink)) {
    return segment->opacity();
  } else if (auto* labels = qobject_cast<LabelMapSink*>(sink)) {
    // A label map's volume representation has no flat opacity; the
    // surface one is what fades
    return labels->surfaceOpacity();
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
  } else if (auto* threshold = qobject_cast<ThresholdSink*>(sink)) {
    threshold->setOpacity(value);
  } else if (auto* segment = qobject_cast<SegmentSink*>(sink)) {
    segment->setOpacity(value);
  } else if (auto* labels = qobject_cast<LabelMapSink*>(sink)) {
    labels->setSurfaceOpacity(value);
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
    snapshot.solidity = volume->solidity();
    snapshot.cutOutEnabled = volume->cutOutEnabled();
    snapshot.cutOutCorner = volume->cutOutCorner();
    snapshot.cutOutPosition = { { volume->cutOutPosition(0),
                                  volume->cutOutPosition(1),
                                  volume->cutOutPosition(2) } };
    snapshot.explodedEnabled = volume->explodedEnabled();
    snapshot.explodedAxis = volume->explodedAxis();
    snapshot.explodedDirection = volume->explodedDirection();
    snapshot.explodedChunks = volume->explodedChunks();
    snapshot.explodedGap = volume->explodedGap();
    snapshot.explodedOffset = volume->explodedOffset();
  }
  double center[3], normal[3];
  if (auto* slice = qobject_cast<SliceSink*>(sink)) {
    snapshot.planeDirection = static_cast<int>(slice->direction());
    snapshot.sliceIndex = slice->slice();
    slice->planeCenter(center);
    slice->planeNormal(normal);
    snapshot.planeCenter = { { center[0], center[1], center[2] } };
    snapshot.planeNormal = { { normal[0], normal[1], normal[2] } };
  } else if (auto* clip = qobject_cast<ClipSink*>(sink)) {
    snapshot.planeDirection = static_cast<int>(clip->direction());
    snapshot.sliceIndex = clip->slice();
    clip->planeCenter(center);
    // What setPlaneNormal takes back; the world normal differs once the
    // volume is moved or turned
    clip->planeNormalInData(normal);
    snapshot.planeCenter = { { center[0], center[1], center[2] } };
    snapshot.planeNormal = { { normal[0], normal[1], normal[2] } };
  } else if (auto* contour = qobject_cast<ContourSink*>(sink)) {
    snapshot.isoValue = contour->isoValue();
  } else if (auto* threshold = qobject_cast<ThresholdSink*>(sink)) {
    snapshot.thresholdLower = threshold->lowerThreshold();
    snapshot.thresholdUpper = threshold->upperThreshold();
  }
  if (auto* labels = qobject_cast<LabelMapSink*>(sink)) {
    snapshot.hiddenLabels = labels->hiddenLabels();
  }
  return snapshot;
}

void applyPlane(LegacyModuleSink* sink, int direction, int sliceIndex,
                const std::array<double, 3>& center,
                const std::array<double, 3>& normal)
{
  if (auto* slice = qobject_cast<SliceSink*>(sink)) {
    auto dir = static_cast<SliceSink::Direction>(direction);
    if (slice->direction() != dir) {
      slice->setDirection(dir);
    }
    if (dir == SliceSink::Custom) {
      slice->setPlaneCenter(center[0], center[1], center[2]);
      slice->setPlaneNormal(normal[0], normal[1], normal[2]);
    } else if (slice->slice() != sliceIndex) {
      slice->setSlice(sliceIndex);
    }
  } else if (auto* clip = qobject_cast<ClipSink*>(sink)) {
    auto dir = static_cast<ClipSink::Direction>(direction);
    if (clip->direction() != dir) {
      clip->setDirection(dir);
    }
    if (dir == ClipSink::Custom) {
      clip->setPlaneOrigin(center[0], center[1], center[2]);
      clip->setPlaneNormal(normal[0], normal[1], normal[2]);
    } else if (clip->slice() != sliceIndex) {
      clip->setSlice(sliceIndex);
    }
  }
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
    if (explodedDirection) {
      const auto& d = *explodedDirection;
      exploded["direction"] = QJsonArray{ d[0], d[1], d[2] };
    }
    exploded["chunks"] = explodedChunks.value_or(4);
    exploded["gap"] = explodedGap.value_or(0.25);
    exploded["offset"] = explodedOffset.value_or(0);
    json["exploded"] = exploded;
  }
  if (planeDirection) {
    QJsonObject plane;
    plane["direction"] = *planeDirection;
    plane["slice"] = sliceIndex.value_or(0);
    auto c = planeCenter.value_or(std::array<double, 3>{ 0.0, 0.0, 0.0 });
    auto n = planeNormal.value_or(std::array<double, 3>{ 0.0, 0.0, 1.0 });
    plane["center"] = QJsonArray{ c[0], c[1], c[2] };
    plane["normal"] = QJsonArray{ n[0], n[1], n[2] };
    json["plane"] = plane;
  }
  if (isoValue) {
    json["iso"] = *isoValue;
  }
  if (solidity) {
    json["solidity"] = *solidity;
  }
  if (thresholdLower && thresholdUpper) {
    json["threshold"] =
      QJsonObject{ { "lower", *thresholdLower }, { "upper", *thresholdUpper } };
  }
  if (hiddenLabels) {
    QJsonArray labels;
    for (double value : *hiddenLabels) {
      labels.append(value);
    }
    json["hiddenLabels"] = labels;
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
    auto direction = exploded["direction"].toArray();
    if (direction.size() == 3) {
      snapshot.explodedDirection = { { direction[0].toDouble(),
                                       direction[1].toDouble(),
                                       direction[2].toDouble() } };
    }
    snapshot.explodedChunks = exploded["chunks"].toInt(4);
    snapshot.explodedGap = exploded["gap"].toDouble(0.25);
    snapshot.explodedOffset = exploded["offset"].toInt(0);
  }
  if (json.contains("plane")) {
    auto plane = json["plane"].toObject();
    snapshot.planeDirection = plane["direction"].toInt();
    snapshot.sliceIndex = plane["slice"].toInt();
    auto center = plane["center"].toArray();
    auto normal = plane["normal"].toArray();
    if (center.size() == 3 && normal.size() == 3) {
      snapshot.planeCenter = { { center[0].toDouble(), center[1].toDouble(),
                                 center[2].toDouble() } };
      snapshot.planeNormal = { { normal[0].toDouble(), normal[1].toDouble(),
                                 normal[2].toDouble() } };
    }
  }
  if (json.contains("iso")) {
    snapshot.isoValue = json["iso"].toDouble();
  }
  if (json.contains("solidity")) {
    snapshot.solidity = json["solidity"].toDouble();
  }
  if (json.contains("threshold")) {
    auto threshold = json["threshold"].toObject();
    snapshot.thresholdLower = threshold["lower"].toDouble();
    snapshot.thresholdUpper = threshold["upper"].toDouble();
  }
  if (json.contains("hiddenLabels")) {
    QVector<double> labels;
    for (const auto& value : json["hiddenLabels"].toArray()) {
      labels.append(value.toDouble());
    }
    snapshot.hiddenLabels = labels;
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
  scene.recorded = true;
  for (auto* node : pipeline->nodes()) {
    if (auto* sink = qobject_cast<LegacyModuleSink*>(node)) {
      scene.sinks.insert(pipeline->nodeId(sink), SinkSnapshot::capture(sink));
    }
  }
  return scene;
}

void SceneSnapshot::apply(Pipeline* pipeline, const QSet<int>* known) const
{
  if (!pipeline) {
    return;
  }
  // A module some viewpoint recorded but this one lacks was not on
  // screen when this one was saved. Modules no viewpoint knows are left
  // alone, and so is everything when this viewpoint recorded nothing.
  if (!sinks.isEmpty() && known) {
    for (auto* node : pipeline->nodes()) {
      auto* sink = qobject_cast<LegacyModuleSink*>(node);
      if (!sink) {
        continue;
      }
      const int id = pipeline->nodeId(sink);
      if (known->contains(id) && !sinks.contains(id) && sink->visibility()) {
        sink->setVisibility(false);
      }
    }
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
        if (snapshot.explodedDirection) {
          const auto& d = *snapshot.explodedDirection;
          volume->setExplodedDirection(d[0], d[1], d[2]);
        }
        // The camera was just placed at the viewpoint, or is flying;
        // neither switch may refit it.
        volume->setExplodedAxis(snapshot.explodedAxis.value_or(2),
                                /*refitCamera=*/false);
        volume->setExplodedChunks(snapshot.explodedChunks.value_or(4));
        volume->setExplodedGap(snapshot.explodedGap.value_or(0.25));
        volume->setExplodedOffset(snapshot.explodedOffset.value_or(0));
        volume->setExplodedEnabled(*snapshot.explodedEnabled,
                                   /*refitCamera=*/false);
      }
    }
    if (snapshot.planeDirection && snapshot.planeCenter &&
        snapshot.planeNormal) {
      applyPlane(sink, *snapshot.planeDirection, snapshot.sliceIndex.value_or(0),
                 *snapshot.planeCenter, *snapshot.planeNormal);
    }
    if (snapshot.solidity) {
      if (auto* volume = qobject_cast<VolumeSink*>(sink)) {
        volume->setSolidity(*snapshot.solidity);
      }
    }
    if (snapshot.isoValue) {
      if (auto* contour = qobject_cast<ContourSink*>(sink)) {
        if (contour->isoValue() != *snapshot.isoValue) {
          contour->setIsoValue(*snapshot.isoValue);
        }
      }
    }
    if (snapshot.thresholdLower && snapshot.thresholdUpper) {
      if (auto* threshold = qobject_cast<ThresholdSink*>(sink)) {
        if (threshold->lowerThreshold() != *snapshot.thresholdLower ||
            threshold->upperThreshold() != *snapshot.thresholdUpper) {
          threshold->setThresholdRange(*snapshot.thresholdLower,
                                       *snapshot.thresholdUpper);
        }
      }
    }
    if (snapshot.hiddenLabels) {
      if (auto* labels = qobject_cast<LabelMapSink*>(sink)) {
        labels->setHiddenLabels(*snapshot.hiddenLabels);
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
  if (!recorded) {
    return json;
  }
  json["recorded"] = true;
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
  // Files from before the flag recorded a scene exactly when it listed
  // modules
  scene.recorded = json["recorded"].toBool(!scene.sinks.isEmpty());
  return scene;
}

std::optional<double> sinkFlatOpacity(LegacyModuleSink* sink)
{
  return flatOpacity(sink);
}

void setSinkFlatOpacity(LegacyModuleSink* sink, double value)
{
  setFlatOpacity(sink, value);
}

bool opacityCurvesEqual(vtkPiecewiseFunction* a, vtkPiecewiseFunction* b)
{
  return curvesEqual(a, b);
}

vtkSmartPointer<vtkPiecewiseFunction> zeroedCurve(vtkPiecewiseFunction* curve)
{
  return zeroed(curve);
}

} // namespace tomviz
