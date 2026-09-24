/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePortDataMetadata_h
#define tomvizPipelinePortDataMetadata_h

#include "PortData.h"

#include <QMap>
#include <QString>

#include <memory>

class QObject;

namespace tomviz {
namespace pipeline {

class Node;
class VolumeData;
using VolumeDataPtr = std::shared_ptr<VolumeData>;

/// Build and apply a fresh segmentation-style colormap to @a vol based
/// on its current active scalars. Used for LabelMap outputs, where the
/// label set can vary between executions and inheriting from upstream
/// would yield wrong colors.
///
/// Returns true if a segmentation preset was applied.
bool applySegmentationColorMap(VolumeData& vol);

/// Bring a LabelMap payload's colors up to date with its voxel data:
/// rescan the label set, reconcile it against the label table (so a
/// color or visibility the user chose survives a re-execution), and
/// project the table onto the color and opacity maps.
///
/// Falls back to applySegmentationColorMap() when @a vol is not a
/// LabelMapData, which is what a state file written before label maps
/// had their own payload type restores.
///
/// Returns true if colors were applied.
bool applyLabelMapColors(const VolumeDataPtr& vol);

/// Copy presentation metadata (e.g. colormap, gradient opacity) from
/// each input PortData onto the matching output PortData whenever the
/// payload kind supports it and the output doesn't already carry its
/// own. Called by nodes between produce-output and publish-output so
/// downstream consumers see the inherited state from their first
/// frame.
///
/// Per-type dispatch lives entirely in this function — callers stay
/// payload-agnostic. Today: volume-typed payloads inherit colormap +
/// gradient opacity from their first volume-typed input. Adding a
/// new inheritable type means a new branch here.
///
/// The work runs on @a node's thread. ParaView SM-proxy operations underneath VolumeData
/// aren't thread-safe; this function marshals to threadOwner's thread
/// via Qt::BlockingQueuedConnection when the caller is elsewhere.
void inheritOutputMetadata(Node* node,
                           const QMap<QString, PortData>& inputs,
                           const QMap<QString, PortData>& outputs);

/// The input whose color map a new volume output of @a node copies: the
/// first of @a inputs, in the order @a node declares its input ports (so
/// the primary data before a mask or a second dataset), that has a color
/// map and is not a label map. A label map's colors are one per label
/// value and mean nothing on other data. Null when no input qualifies,
/// or @a node declines inheritance (Node::inheritsColorMap); the output
/// then starts from the default color map.
VolumeDataPtr colorMapSource(Node* node,
                             const QMap<QString, PortData>& inputs);

} // namespace pipeline
} // namespace tomviz

#endif
