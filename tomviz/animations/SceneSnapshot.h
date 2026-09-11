/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSceneSnapshot_h
#define tomvizSceneSnapshot_h

#include <vtkSmartPointer.h>

#include <QHash>
#include <QJsonObject>

#include <array>
#include <optional>

class vtkPiecewiseFunction;

namespace tomviz {

namespace pipeline {
class Pipeline;
class LegacyModuleSink;
} // namespace pipeline

/// The whitelisted state of one visualization module that a viewpoint
/// records: visibility, the flat opacity of surface/plane modules, the
/// scalar opacity curve of a volume, and the volume cut-out. Fields a
/// module does not have stay unset.
struct SinkSnapshot
{
  bool visible = true;
  std::optional<double> opacity;
  vtkSmartPointer<vtkPiecewiseFunction> scalarOpacity;
  std::optional<bool> cutOutEnabled;
  std::optional<int> cutOutCorner;
  std::optional<std::array<double, 3>> cutOutPosition;
  std::optional<bool> explodedEnabled;
  std::optional<int> explodedAxis;
  std::optional<int> explodedChunks;
  std::optional<double> explodedGap;

  static SinkSnapshot capture(pipeline::LegacyModuleSink* sink);
  QJsonObject serialize() const;
  static SinkSnapshot deserialize(const QJsonObject& json);
};

/// Module state recorded with a camera viewpoint, keyed by pipeline node
/// id so it can be matched up again after a state file round trip.
struct SceneSnapshot
{
  QHash<int, SinkSnapshot> sinks;

  bool isEmpty() const { return sinks.isEmpty(); }

  static SceneSnapshot capture(pipeline::Pipeline* pipeline);
  /// Put every recorded module back the way it was, as "Go To" does.
  void apply(pipeline::Pipeline* pipeline) const;

  QJsonObject serialize() const;
  static SceneSnapshot deserialize(const QJsonObject& json);
};

/// Move every recorded module a fraction @a u of the way from @a from to
/// @a to. Values that are identical at both ends are left alone, so a
/// module the user never changed between two viewpoints stays under
/// their control; a module with an explicit ModuleAnimation for a
/// property keeps that animation. Modules that appear or disappear fade
/// through their opacity where they have one and otherwise switch
/// halfway. Volumes whose curve was touched are added to
/// @a overriddenVolumes so the caller can hand them back afterwards.
void applySceneTransition(pipeline::Pipeline* pipeline,
                          const SceneSnapshot& from, const SceneSnapshot& to,
                          double u, QSet<int>& overriddenVolumes);

} // namespace tomviz

#endif
