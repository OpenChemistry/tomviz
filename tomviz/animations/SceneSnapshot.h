/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSceneSnapshot_h
#define tomvizSceneSnapshot_h

#include <vtkSmartPointer.h>

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QVector>

#include <array>
#include <optional>

class vtkPiecewiseFunction;

namespace tomviz {

namespace pipeline {
class Pipeline;
class LegacyModuleSink;
} // namespace pipeline

/// The whitelisted state of one visualization module that a viewpoint
/// records: visibility, the flat opacity of surface/plane modules (and
/// a label map's surface), the scalar opacity curve and solidity of a
/// volume, the volume cut-out and exploded view, where a slice or clip plane sits,
/// a contour's iso value, a threshold's range, and which labels of a
/// label map are hidden. Fields a module does not have stay unset.
struct SinkSnapshot
{
  bool visible = true;
  std::optional<double> opacity;
  vtkSmartPointer<vtkPiecewiseFunction> scalarOpacity;
  /// Volumes: the solidity.
  std::optional<double> solidity;
  std::optional<bool> cutOutEnabled;
  std::optional<int> cutOutCorner;
  std::optional<std::array<double, 3>> cutOutPosition;
  std::optional<bool> explodedEnabled;
  std::optional<int> explodedAxis;
  /// The custom direction, kept even while an axis is selected so
  /// switching back finds it.
  std::optional<std::array<double, 3>> explodedDirection;
  std::optional<int> explodedChunks;
  std::optional<double> explodedGap;
  std::optional<int> explodedOffset;
  /// Slice and clip planes: the direction (0..2 axis aligned, 3 custom),
  /// the slice index while axis aligned, and the plane while custom.
  std::optional<int> planeDirection;
  std::optional<int> sliceIndex;
  std::optional<std::array<double, 3>> planeCenter;
  std::optional<std::array<double, 3>> planeNormal;
  /// Contours: the iso value.
  std::optional<double> isoValue;
  /// Threshold visualizations: the range shown.
  std::optional<double> thresholdLower;
  std::optional<double> thresholdUpper;
  /// Label maps: the labels the user hid, ascending.
  std::optional<QVector<double>> hiddenLabels;

  static SinkSnapshot capture(pipeline::LegacyModuleSink* sink);
  QJsonObject serialize() const;
  static SinkSnapshot deserialize(const QJsonObject& json);
};

/// Module state recorded with a camera viewpoint, keyed by pipeline node
/// id so it can be matched up again after a state file round trip.
struct SceneSnapshot
{
  QHash<int, SinkSnapshot> sinks;
  /// Whether anything was recorded at all. A viewpoint saved with
  /// recording on but no visualizations yet has recorded that there
  /// were none, which is not the same as having recorded nothing.
  bool recorded = false;

  bool isEmpty() const { return !recorded; }

  static SceneSnapshot capture(pipeline::Pipeline* pipeline);
  /// Put every recorded module back the way it was, as "Go To" does.
  /// Of the modules in @a known (those some viewpoint recorded), any
  /// this snapshot lacks is hidden, since it was not on screen when the
  /// snapshot was taken. An empty snapshot (recording was off) touches
  /// nothing, and neither does a null @a known.
  void apply(pipeline::Pipeline* pipeline,
             const QSet<int>* known = nullptr) const;

  QJsonObject serialize() const;
  static SceneSnapshot deserialize(const QJsonObject& json);
};

/// Flat opacity of the surface and plane modules that have one.
std::optional<double> sinkFlatOpacity(pipeline::LegacyModuleSink* sink);
void setSinkFlatOpacity(pipeline::LegacyModuleSink* sink, double value);

/// Node-for-node equality of two opacity curves.
bool opacityCurvesEqual(vtkPiecewiseFunction* a, vtkPiecewiseFunction* b);

/// The same curve with every opacity at zero: the hidden end of a fade.
vtkSmartPointer<vtkPiecewiseFunction> zeroedCurve(vtkPiecewiseFunction* curve);

/// Put a slice or clip plane where a snapshot recorded it: the slice
/// index while axis aligned, the plane itself while custom.
void applyPlane(pipeline::LegacyModuleSink* sink, int direction,
                int sliceIndex, const std::array<double, 3>& center,
                const std::array<double, 3>& normal);

} // namespace tomviz

#endif
