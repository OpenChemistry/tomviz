/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRecordedAnimations_h
#define tomvizRecordedAnimations_h

#include "ModuleAnimation.h"
#include "ScalarOpacityAnimation.h"
#include "SceneSnapshot.h"
#include "pipeline/sinks/ExplodedGeometry.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <array>
#include <optional>

namespace tomviz {

namespace pipeline {
class Pipeline;
class VolumeSink;
} // namespace pipeline

/// The animations that play the module state recorded with the camera
/// viewpoints, and the list of rows that shows it.
///
/// The viewpoints' snapshots are the only record. Everything here is
/// derived from them: whenever the viewpoints change, the recorded
/// animations in ModuleAnimations are thrown away and rebuilt, and the
/// Animation Helper lists one row per property that differs between two
/// consecutive recorded viewpoints. Removing such a row edits the later
/// viewpoint so the property keeps its earlier value, which is why the
/// row stays gone. An animation the user authored for the same
/// visualization and property takes over the legs it runs on; the
/// recorded animation skips those legs and the list shows the user's
/// row instead.

/// One property of one visualization changing between two recorded
/// viewpoints, as the Animation Helper lists it.
struct RecordedChange
{
  int nodeId = -1;
  /// "opacity", "visibility", "curve", "cutOut", "exploded", "slice",
  /// "clip", "iso", "threshold" or "labels".
  QString property;
  /// The Animation Helper property that authors this kind of change,
  /// when the change is one the controls can show: "explodedGap",
  /// "cutOutX", "thresholdLower" and so on. Empty means the property
  /// itself, or nothing authorable (visibility, labels).
  QString controlProperty;
  /// Viewpoint indices. Viewpoints between them recorded nothing.
  int fromAnchor = 0;
  int toAnchor = 1;
  /// e.g. "fades in to 0.6" or "cut-out moves".
  QString description;
  /// For a change with two plain numbers behind it (opacity, slice
  /// index, iso value), the value at each end, so the Animation Helper
  /// can show it in its controls.
  std::optional<double> startValue;
  std::optional<double> stopValue;
};

/// Where @a t (path time in [0, 1]) falls among the recorded anchors:
/// the surrounding anchors and the fraction between them. Before the
/// first anchor both are the first; after the last, both are the last.
struct AnchorSpan
{
  int from = 0;
  int to = 0;
  double u = 0.0;
};
AnchorSpan anchorSpanAt(const QList<int>& anchors, double t);

/// A recorded module state at one viewpoint. Hidden modules carry the
/// values they will have once shown, so a fade has somewhere to go.
struct OpacityKey
{
  bool visible = true;
  double opacity = 1.0;
};
struct CutOutKey
{
  bool enabled = false;
  int corner = 0;
  std::array<double, 3> position = { 0.5, 0.5, 0.5 };
  bool operator==(const CutOutKey& o) const
  {
    return enabled == o.enabled && corner == o.corner && position == o.position;
  }
};
/// A Threshold visualization's range at one viewpoint.
struct ThresholdKey
{
  double lower = 0.0;
  double upper = 1.0;
  bool operator==(const ThresholdKey& o) const
  {
    return lower == o.lower && upper == o.upper;
  }
};

struct ExplodedKey
{
  bool enabled = false;
  int axis = 2;
  std::array<double, 3> direction = { 1.0, 1.0, 1.0 };
  int chunks = 4;
  double gap = 0.25;
  int offset = 0;
  bool operator==(const ExplodedKey& o) const
  {
    // The direction only shows while the axis is custom
    return enabled == o.enabled && axis == o.axis &&
           (axis != pipeline::kExplodedCustomAxis ||
            direction == o.direction) &&
           chunks == o.chunks && gap == o.gap && offset == o.offset;
  }
};

/// Where a slice or clip plane sits: the slice index while axis
/// aligned, the plane itself while custom (direction 3).
struct PlaneKey
{
  int direction = 0;
  int slice = 0;
  std::array<double, 3> center = { 0.0, 0.0, 0.0 };
  std::array<double, 3> normal = { 0.0, 0.0, 1.0 };
  bool custom() const { return direction == 3; }
  bool operator==(const PlaneKey& o) const
  {
    if (direction != o.direction) {
      return false;
    }
    return custom() ? (center == o.center && normal == o.normal)
                    : slice == o.slice;
  }
};

/// Effective opacity of a key: zero while hidden.
double effectiveOpacity(const OpacityKey& key);

/// Base of the animations built from the recorded snapshots: keyframed
/// by viewpoint anchor, eased along with the camera, and skipping any
/// leg an authored animation owns.
class RecordedAnimation : public ModuleAnimation
{
  Q_OBJECT

public:
  RecordedAnimation(pipeline::Node* node) : ModuleAnimation(node) {}

  bool recorded() const override { return true; }

  /// Legs (viewpoint index of the leg's start) this animation leaves to
  /// an authored animation.
  QSet<int> excludedSegments;

  void onTimeChanged() override;
  void onPlaybackStarted() override { applyPathTime(0.0); }

  /// Apply the state for path time @a t in [0, 1]. Public so it can be
  /// driven without a time keeper.
  void applyPathTime(double t);

protected:
  virtual void applySpan(const AnchorSpan& span) = 0;
  virtual QList<int> anchors() const = 0;
};

class RecordedOpacityAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedOpacityAnimation(pipeline::Node* node,
                           const QMap<int, OpacityKey>& keys)
    : RecordedAnimation(node), m_keys(keys)
  {
  }
  QString type() const override { return "opacity"; }
  QString describeParameters() const override { return "recorded opacity"; }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, OpacityKey> m_keys;
};

class RecordedVisibilityAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedVisibilityAnimation(pipeline::Node* node, const QMap<int, bool>& keys)
    : RecordedAnimation(node), m_keys(keys)
  {
  }
  QString type() const override { return "visibility"; }
  QString describeParameters() const override
  {
    return "recorded visibility";
  }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, bool> m_keys;
};

class RecordedCutOutAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedCutOutAnimation(pipeline::VolumeSink* sink,
                          const QMap<int, CutOutKey>& keys);
  QString type() const override { return "cutOut"; }
  QString describeParameters() const override { return "recorded cut-out"; }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, CutOutKey> m_keys;
};

class RecordedExplodedAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedExplodedAnimation(pipeline::VolumeSink* sink,
                            const QMap<int, ExplodedKey>& keys);
  QString type() const override { return "exploded"; }
  QString describeParameters() const override
  {
    return "recorded exploded view";
  }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, ExplodedKey> m_keys;
};

/// A slice or clip plane moving between recorded positions: the index
/// slides while both ends share an axis, a custom plane slides and
/// turns, and anything else switches halfway.
class RecordedPlaneAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedPlaneAnimation(pipeline::Node* node, const QMap<int, PlaneKey>& keys)
    : RecordedAnimation(node), m_keys(keys)
  {
  }
  /// "slice" for a slice, "clip" for a clip: the authored types that
  /// take a leg over.
  QString type() const override;
  QString describeParameters() const override { return "recorded plane"; }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, PlaneKey> m_keys;
};

class RecordedIsoAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedIsoAnimation(pipeline::Node* node, const QMap<int, double>& keys)
    : RecordedAnimation(node), m_keys(keys)
  {
  }
  QString type() const override { return "contour"; }
  QString describeParameters() const override { return "recorded iso value"; }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, double> m_keys;
};

class RecordedThresholdAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedThresholdAnimation(pipeline::Node* node,
                             const QMap<int, ThresholdKey>& keys)
    : RecordedAnimation(node), m_keys(keys)
  {
  }
  QString type() const override { return "threshold"; }
  QString describeParameters() const override
  {
    return "recorded threshold range";
  }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, ThresholdKey> m_keys;
};

/// The labels of a label map hidden at each viewpoint. There is nothing
/// to fade through, so the set switches halfway along the leg.
class RecordedLabelsAnimation : public RecordedAnimation
{
  Q_OBJECT

public:
  RecordedLabelsAnimation(pipeline::Node* node,
                          const QMap<int, QVector<double>>& keys)
    : RecordedAnimation(node), m_keys(keys)
  {
  }
  QString type() const override { return "labels"; }
  QString describeParameters() const override
  {
    return "recorded label visibility";
  }

protected:
  void applySpan(const AnchorSpan& span) override;
  QList<int> anchors() const override { return m_keys.keys(); }

private:
  QMap<int, QVector<double>> m_keys;
};

/// The recorded opacity curves of a volume. A viewpoint where the
/// volume was hidden contributes an all-zero curve, and the volume is
/// shown exactly while the blended curve has any opacity.
class RecordedCurveAnimation : public ScalarOpacityAnimation
{
  Q_OBJECT

public:
  RecordedCurveAnimation(pipeline::VolumeSink* sink,
                         const QList<OpacityKeyframe>& keyframes,
                         const QSet<int>& excludedSegments)
    : ScalarOpacityAnimation(sink, keyframes),
      m_excludedSegments(excludedSegments)
  {
  }
  bool recorded() const override { return true; }
  QString describeParameters() const override
  {
    return "recorded opacity curve";
  }
  void onTimeChanged() override;
  void onPlaybackStarted() override;

protected:
  void curveApplied(vtkPiecewiseFunction* current) override;

private:
  bool legIsExcluded(double t) const;
  QSet<int> m_excludedSegments;
};

/// Keeps ModuleAnimations in step with the viewpoints and answers for
/// the Animation Helper.
class RecordedAnimations : public QObject
{
  Q_OBJECT

public:
  static RecordedAnimations& instance();

  /// Follow the viewpoints and the authored animations of the active
  /// pipeline from now on. Safe to call more than once.
  void install();

  /// Rebuild the recorded animations in ModuleAnimations from the
  /// viewpoints, against the visualizations of @a pipeline.
  void sync(pipeline::Pipeline* pipeline);

  /// The rows to list: every property that differs between two
  /// consecutive recorded viewpoints, except on legs an authored
  /// animation owns.
  QList<RecordedChange> changes(pipeline::Pipeline* pipeline) const;

  /// Edit the later viewpoint of @a change so the property keeps its
  /// value from the earlier one. The viewpoints then re-sync.
  void remove(const RecordedChange& change, pipeline::Pipeline* pipeline);

  /// Every visualization some viewpoint recorded. Go To hides those a
  /// particular viewpoint lacks; ones no viewpoint knows are left alone.
  QSet<int> recordedNodeIds() const;

private:
  RecordedAnimations() = default;
  Q_DISABLE_COPY(RecordedAnimations)

  bool m_installed = false;
  bool m_syncing = false;
};

} // namespace tomviz

#endif
