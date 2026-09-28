/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PortDataMetadata.h"

#include "ColorMap.h"
#include "InputPort.h"
#include "Node.h"
#include "PortType.h"
#include "ThreadUtils.h"
#include "data/LabelMapData.h"
#include "data/VolumeData.h"

#include <QObject>

#include <vtkPiecewiseFunction.h>

namespace tomviz {
namespace pipeline {

bool applySegmentationColorMap(VolumeData& vol)
{
  // Always build a fresh colormap from the output's actual scalars --
  // the label set can change between executions, so inheriting from
  // upstream or reusing a prior preset would yield wrong colors.
  vol.initColorMap();
  auto* scalars = vol.scalars();
  auto preset = tomviz::buildSegmentationPreset(scalars);
  bool applied = !preset.isEmpty();
  if (applied) {
    // The segmentation preset's node positions are per-label data
    // coordinates - rescaling them into the (possibly default [0, 1])
    // current range would clamp every label to one color.
    tomviz::applyPresetToProxy(preset, vol.colorMap(),
                               /*rescaleToCurrentRange=*/false);
  }

  // Label 0 is background -- make it transparent. All other labels
  // are fully opaque so the segmented regions render as solid.
  auto* opacity = vol.scalarOpacity();
  if (opacity) {
    auto range = vol.scalarRange();
    double maxVal = range[1] > 0.5 ? range[1] : 1.0;
    opacity->RemoveAllPoints();
    opacity->AddPoint(0.0, 0.0);
    opacity->AddPoint(0.5, 1.0);
    opacity->AddPoint(maxVal, 1.0);
    opacity->Modified();
  }

  // Push the client-side edits to the SM proxy. Without this, a later
  // rescale (per-node post-execution, or the user's "Reset data range")
  // rescales the proxy's stale default points and wipes the opacity
  // back to a linear ramp.
  vol.syncColorMapToProxy();

  return applied;
}

bool applyLabelMapColors(const VolumeDataPtr& vol)
{
  if (!vol) {
    return false;
  }
  auto labelMap = labelMapData(vol);
  if (!labelMap) {
    return applySegmentationColorMap(*vol);
  }
  if (!labelMap->labelsSupported()) {
    return false;
  }
  labelMap->refreshLabels();
  if (labelMap->labels().isEmpty()) {
    return false;
  }
  labelMap->applyLabels();
  return true;
}

namespace {

/// Volume-specific inheritance: copy colormap + gradient opacity from
/// colorMapSource() onto each volume-typed output that has none. LabelMap
/// outputs are handled separately: their colors come from their own
/// label table, reconciled against the scalars they just produced, never
/// inherited from upstream.
void inheritVolumeMetadata(Node* node, const QMap<QString, PortData>& inputs,
                           const QMap<QString, PortData>& outputs)
{
  const VolumeDataPtr source = colorMapSource(node, inputs);
  for (auto outIt = outputs.constBegin(); outIt != outputs.constEnd();
       ++outIt) {
    if (!isVolumeType(outIt.value().type())) {
      continue;
    }
    VolumeDataPtr outVolume;
    try {
      outVolume = outIt.value().value<VolumeDataPtr>();
    } catch (const std::bad_any_cast&) {
      continue;
    }
    if (!outVolume) {
      continue;
    }
    if (outIt.value().type() == PortType::LabelMap) {
      applyLabelMapColors(outVolume);
      continue;
    }
    if (outVolume->hasColorMap()) {
      // Already initialized (e.g. the producer reused an existing
      // VolumeData) — leave its colormap alone.
      continue;
    }
    if (source) {
      outVolume->initColorMap();
      outVolume->copyColorMapFrom(*source);
    }
  }
}

} // namespace

VolumeDataPtr colorMapSource(Node* node, const QMap<QString, PortData>& inputs)
{
  if (node && !node->inheritsColorMap()) {
    return nullptr;
  }
  // Declaration order first: the primary input before a mask or a
  // second dataset (a QMap would put "mask" before "volume").
  QStringList order;
  if (node) {
    for (auto* port : node->inputPorts()) {
      order.append(port->name());
    }
  }
  for (const auto& name : inputs.keys()) {
    if (!order.contains(name)) {
      order.append(name);
    }
  }
  for (const auto& name : order) {
    if (!inputs.contains(name)) {
      continue;
    }
    const PortData& input = inputs.value(name);
    if (!isVolumeType(input.type()) || input.type() == PortType::LabelMap) {
      continue;
    }
    VolumeDataPtr volume;
    try {
      volume = input.value<VolumeDataPtr>();
    } catch (const std::bad_any_cast&) {
      continue;
    }
    if (volume && volume->hasColorMap() && !labelMapData(volume)) {
      return volume;
    }
  }
  return nullptr;
}

void inheritOutputMetadata(Node* node,
                           const QMap<QString, PortData>& inputs,
                           const QMap<QString, PortData>& outputs)
{
  auto apply = [&]() {
    inheritVolumeMetadata(node, inputs, outputs);
    // Future payload types with inheritable metadata: dispatch here.
  };

  runOnThread(node, apply);
}

} // namespace pipeline
} // namespace tomviz
