/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleAnimations.h"

#include "ActiveObjects.h"
#include "CameraViewpoints.h"
#include "ClipAnimation.h"
#include "ContourAnimation.h"
#include "CutOutAnimation.h"
#include "ExplodedAnimation.h"
#include "ModuleAnimation.h"
#include "OpacityAnimation.h"
#include "ScalarOpacityAnimation.h"
#include "SliceAnimation.h"
#include "SolidityAnimation.h"
#include "ThresholdAnimation.h"

#include "pipeline/Pipeline.h"

#include <QDebug>
#include <QHash>
#include <QJsonArray>

namespace tomviz {

namespace {

// Rebuild one saved animation. Returns null for an entry naming a type
// we no longer have, or a node that cannot carry that animation.
ModuleAnimation* buildAnimation(const QString& type, pipeline::Node* node,
                                const QJsonObject& json)
{
  double start = json["start"].toDouble();
  double stop = json["stop"].toDouble();

  if (type == "contour") {
    if (auto* sink = qobject_cast<pipeline::ContourSink*>(node)) {
      return new ContourAnimation(sink, start, stop);
    }
  } else if (type == "solidity") {
    if (auto* sink = qobject_cast<pipeline::VolumeSink*>(node)) {
      return new SolidityAnimation(sink, start, stop);
    }
  } else if (type == "slice") {
    if (auto* sink = qobject_cast<pipeline::SliceSink*>(node)) {
      // Files from before custom slices could be swept carry no unit
      auto unit = json["unit"].toString() == "distance"
                    ? SliceAnimation::Distance
                    : SliceAnimation::Slice;
      if ((unit == SliceAnimation::Slice) != sink->isOrtho()) {
        qWarning() << "Dropping slice animation: the slice changed"
                   << "orientation since it was saved.";
        return nullptr;
      }
      return new SliceAnimation(sink, start, stop, unit);
    }
  } else if (type == "clip") {
    if (auto* sink = qobject_cast<pipeline::ClipSink*>(node)) {
      auto unit = json["unit"].toString() == "distance"
                    ? ClipAnimation::Distance
                    : ClipAnimation::Slice;
      // A clip that changed orientation since it was saved cannot use
      // the saved unit: slice indices are meaningless once the plane is
      // off axis, and a distance sweep would be ignored while it is on
      // one. Drop it rather than move the plane somewhere arbitrary.
      if ((unit == ClipAnimation::Slice) != sink->isOrtho()) {
        qWarning() << "Dropping clip animation: the clip changed"
                   << "orientation since it was saved.";
        return nullptr;
      }
      return new ClipAnimation(sink, start, stop, unit);
    }
  } else if (type == "scalarOpacity") {
    auto* sink = qobject_cast<pipeline::VolumeSink*>(node);
    if (sink && ScalarOpacityAnimation::supports(node)) {
      QList<OpacityKeyframe> keyframes;
      for (const auto& value : json["keyframes"].toArray()) {
        auto entry = value.toObject();
        OpacityKeyframe keyframe;
        keyframe.anchor = entry["anchor"].toInt();
        keyframe.curve = vtkSmartPointer<vtkPiecewiseFunction>::New();
        tomviz::deserialize(keyframe.curve.Get(), entry["curve"].toObject());
        keyframes.append(keyframe);
      }
      if (!keyframes.isEmpty()) {
        return new ScalarOpacityAnimation(sink, keyframes);
      }
    }
  } else if (type == "opacity") {
    if (OpacityAnimation::supports(node)) {
      return new OpacityAnimation(node, start, stop);
    }
  } else if (type == "exploded") {
    if (auto* sink = qobject_cast<pipeline::VolumeSink*>(node)) {
      const QString name = json["unit"].toString();
      auto unit = name == "chunks"   ? ExplodedAnimation::Chunks
                  : name == "offset" ? ExplodedAnimation::Offset
                                     : ExplodedAnimation::Gap;
      return new ExplodedAnimation(sink, start, stop, unit);
    }
  } else if (type == "cutOut") {
    if (auto* sink = qobject_cast<pipeline::VolumeSink*>(node)) {
      return new CutOutAnimation(sink, start, stop, json["axis"].toInt(0));
    }
  } else if (type == "threshold") {
    if (auto* sink = qobject_cast<pipeline::ThresholdSink*>(node)) {
      auto end = json["end"].toString() == "upper" ? ThresholdAnimation::Upper
                                                   : ThresholdAnimation::Lower;
      return new ThresholdAnimation(sink, start, stop, end);
    }
  }

  return nullptr;
}

} // anonymous namespace

ModuleAnimations& ModuleAnimations::instance()
{
  static ModuleAnimations animations;
  return animations;
}

void ModuleAnimations::add(ModuleAnimation* animation)
{
  if (!animation) {
    return;
  }

  prune();
  m_animations.append(animation);
  emit changed();
}

void ModuleAnimations::remove(ModuleAnimation* animation)
{
  prune();
  for (int i = 0; i < m_animations.size(); ++i) {
    if (m_animations[i] == animation) {
      m_animations[i]->deleteLater();
      m_animations.removeAt(i);
      emit changed();
      return;
    }
  }
}

QList<ModuleAnimation*> ModuleAnimations::animations()
{
  prune();

  QList<ModuleAnimation*> alive;
  for (const auto& animation : m_animations) {
    alive.append(animation.data());
  }
  return alive;
}

bool ModuleAnimations::isEmpty()
{
  prune();
  return m_animations.isEmpty();
}

void ModuleAnimations::clear()
{
  if (m_animations.isEmpty()) {
    return;
  }

  for (const auto& animation : m_animations) {
    if (animation) {
      animation->deleteLater();
    }
  }
  m_animations.clear();
  emit changed();
}

void ModuleAnimations::prune()
{
  for (int i = m_animations.size() - 1; i >= 0; --i) {
    if (m_animations[i].isNull()) {
      m_animations.removeAt(i);
    }
  }
}

void ModuleAnimations::pinExportQuality()
{
  using Curve = vtkSmartPointer<vtkPiecewiseFunction>;
  // Every sequence of curves a volume will blend through: the explicit
  // opacity morphs, plus the curves recorded with the viewpoints.
  QHash<pipeline::VolumeSink*, QList<QList<Curve>>> sequences;
  for (const auto& animation : m_animations) {
    auto* morph = qobject_cast<ScalarOpacityAnimation*>(animation.data());
    auto* sink = morph ? morph->sink() : nullptr;
    if (!sink || morph->keyframes().isEmpty()) {
      continue;
    }
    QList<Curve> curves;
    for (const auto& keyframe : morph->keyframes()) {
      curves.append(keyframe.curve);
    }
    sequences[sink].append(curves);
  }
  if (auto* pipeline = ActiveObjects::instance().pipeline()) {
    QHash<pipeline::VolumeSink*, QList<Curve>> recorded;
    for (const auto& viewpoint : CameraViewpoints::instance().viewpoints()) {
      for (auto it = viewpoint.scene.sinks.cbegin();
           it != viewpoint.scene.sinks.cend(); ++it) {
        auto* sink =
          qobject_cast<pipeline::VolumeSink*>(pipeline->nodeById(it.key()));
        if (sink && it.value().scalarOpacity) {
          recorded[sink].append(it.value().scalarOpacity);
        }
      }
    }
    for (auto it = recorded.cbegin(); it != recorded.cend(); ++it) {
      sequences[it.key()].append(it.value());
    }
  }

  for (auto it = sequences.cbegin(); it != sequences.cend(); ++it) {
    auto* sink = it.key();
    double range[2] = { 0.0, 1.0 };
    auto volume = sink->volumeData();
    if (volume && volume->isValid()) {
      auto volumeRange = volume->colorMapRange();
      range[0] = volumeRange[0];
      range[1] = volumeRange[1];
    }

    // The most expensive curve is not necessarily any of the captures - a
    // spike dissolving into a ramp is widest somewhere in the middle - so
    // walk every blend rather than just comparing the keyframes.
    vtkNew<vtkPiecewiseFunction> worst;
    double worstCost = -1.0;
    auto consider = [&](vtkPiecewiseFunction* curve) {
      const double cost = pipeline::VolumeSink::opacityRenderCost(curve);
      if (cost > worstCost) {
        worstCost = cost;
        worst->DeepCopy(curve);
      }
    };
    const int steps = 10;
    for (const auto& curves : it.value()) {
      consider(curves.first());
      for (int pair = 0; pair + 1 < curves.size(); ++pair) {
        for (int i = 1; i <= steps; ++i) {
          vtkNew<vtkPiecewiseFunction> sample;
          interpolateOpacity(curves[pair], curves[pair + 1],
                             static_cast<double>(i) / steps, range, sample);
          consider(sample);
        }
      }
    }
    sink->setWorstCaseOpacity(worst);
  }
}

void ModuleAnimations::releaseExportQuality()
{
  for (const auto& animation : m_animations) {
    auto* morph = qobject_cast<ScalarOpacityAnimation*>(animation.data());
    if (auto* sink = morph ? morph->sink() : nullptr) {
      sink->setWorstCaseOpacity(nullptr);
    }
  }
}

QJsonObject ModuleAnimations::serialize(pipeline::Pipeline* pipeline) const
{
  QJsonArray array;
  for (const auto& animation : m_animations) {
    // Recorded animations are rebuilt from the viewpoints on load
    if (!pipeline || !animation || !animation->baseNode ||
        animation->recorded()) {
      continue;
    }

    auto type = animation->type();
    if (type.isEmpty()) {
      continue;
    }

    auto entry = animation->serialize();
    entry["type"] = type;
    entry["node"] = pipeline->nodeId(animation->baseNode);
    entry["segment"] = animation->segment;
    array.append(entry);
  }

  QJsonObject json;
  json["modules"] = array;
  return json;
}

void ModuleAnimations::deserialize(const QJsonObject& json,
                                   pipeline::Pipeline* pipeline)
{
  clear();

  if (!pipeline || !json["modules"].isArray()) {
    return;
  }

  for (const auto& value : json["modules"].toArray()) {
    auto entry = value.toObject();
    auto* node = pipeline->nodeById(entry["node"].toInt(-1));
    if (!node) {
      qWarning() << "Dropping a" << entry["type"].toString()
                 << "animation: its visualization is not in this state"
                 << "file.";
      continue;
    }

    if (auto* animation =
          buildAnimation(entry["type"].toString(), node, entry)) {
      animation->segment = entry["segment"].toInt(-1);
      m_animations.append(animation);
    }
  }

  emit changed();
}

} // namespace tomviz
