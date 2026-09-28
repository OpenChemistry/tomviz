/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLabelMapData_h
#define tomvizPipelineLabelMapData_h

#include "VolumeData.h"

#include "PortType.h"

#include <QColor>
#include <QPair>
#include <QString>
#include <QVector>

#include <vtkType.h>

namespace tomviz {
namespace pipeline {

/// One label of a label map: the scalar value that identifies it in the
/// voxel data, plus the presentation state the user controls.
struct LabelEntry
{
  double value = 0.0;
  /// User-assigned name. Empty means "never named"; displayName()
  /// supplies the fallback shown in the UI.
  QString name;
  QColor color;
  bool visible = true;
  /// Number of voxels carrying this value, from the last scan.
  vtkIdType voxelCount = 0;

  /// What the UI shows in the name column.
  QString displayName() const;
};

/// The set of labels present in a label map, in ascending value order.
///
/// This is the authoritative record of per-label color and visibility.
/// The color and scalar-opacity transfer functions are derived from it
/// (see LabelMapData::applyLabels), never the other way around: a
/// transfer function cannot carry a label's name, cannot distinguish
/// "hidden by the user" from "faded out by an opacity edit", and is
/// rebuilt from scratch by presets and rescales.
class LabelTable
{
public:
  /// Upper bound on the number of distinct labels tracked. Beyond this
  /// the scan stops and isTruncated() reports true. A particle
  /// segmentation runs to tens of thousands of labels (a superlattice
  /// at HXN started at 8k), so the bound is generous; past a few
  /// thousand the volume representation's lookup texture, sized from
  /// the 0.5 node gap, is clamped to what the driver allows and
  /// neighbouring bands start to blend, but the surface representation
  /// and the table are unaffected.
  static constexpr int maxLabels = 65536;

  const QVector<LabelEntry>& entries() const { return m_entries; }
  int count() const { return m_entries.size(); }
  bool isEmpty() const { return m_entries.isEmpty(); }

  /// Index of the entry with the given scalar value, or -1.
  int indexOfValue(double value) const;

  const LabelEntry& at(int index) const { return m_entries.at(index); }

  /// Presentation setters. No-ops for an out-of-range @a index.
  void setName(int index, const QString& name);
  void setColor(int index, const QColor& color);
  void setVisible(int index, bool visible);
  void setAllVisible(bool visible);

  /// True when the last scan gave up at maxLabels, so the table holds a
  /// prefix of the labels actually present rather than all of them.
  bool isTruncated() const { return m_truncated; }

  /// Replace the entries with @a scanned (value, voxel count) pairs in
  /// ascending value order, carrying over the name, color and
  /// visibility of every label whose value survives. Labels that are
  /// new get the next unused palette color, and are visible unless
  /// their value is 0, the conventional background label.
  void reconcile(const QVector<QPair<double, vtkIdType>>& scanned,
                 bool truncated);

  QJsonObject serialize() const;
  bool deserialize(const QJsonObject& json);

private:
  QVector<LabelEntry> m_entries;
  bool m_truncated = false;
  /// Monotonic palette cursor. Colors are never handed out twice within
  /// one label map's lifetime, so a label added after another was
  /// removed cannot inherit the removed label's color.
  int m_nextColorIndex = 0;
};

/// A VolumeData whose scalars are categorical: each distinct value is a
/// label rather than a sample of a continuous quantity.
///
/// Payloads are always shared as VolumeDataPtr (PortData holds them in a
/// std::any, and every reader does an exact-type std::any_cast), so this
/// subclass is reached with labelMapData() below rather than by casting
/// the std::any.
class LabelMapData : public VolumeData
{
public:
  LabelMapData();
  explicit LabelMapData(vtkSmartPointer<vtkImageData> imageData);
  ~LabelMapData() override;

  const LabelTable& labels() const { return m_labels; }
  LabelTable& labels() { return m_labels; }

  /// Rescan the active scalars for distinct values and reconcile the
  /// label table against them. Cheap to call repeatedly: the scan is
  /// skipped while the image, its modification time and the active
  /// array all match the previous one. Pass @a force to rescan anyway.
  ///
  /// Does nothing for floating-point scalars, which have no meaningful
  /// set of distinct values; labelsSupported() reports that case.
  void refreshLabels(bool force = false);

  /// Whether the active scalars can be treated as labels at all
  /// (integer-typed, single-component).
  bool labelsSupported() const;

  /// Take on the label table of @a previous - the payload this one is
  /// replacing on the same output port - then reconcile it against our
  /// own scalars. This is what makes a color or a checkbox the user set
  /// survive the producing node running again: every execution
  /// publishes a brand new payload, which would otherwise start from
  /// default colors with everything visible.
  ///
  /// Touches no ParaView proxies, so it is safe to call from a worker
  /// thread. Projecting the result onto the transfer functions is a
  /// separate, GUI-thread step (applyLabels).
  void adoptLabelsFrom(const LabelMapData& previous);

  /// Write the label table onto this data's own color and opacity maps.
  /// Calls initColorMap() first, so it is safe on a fresh instance.
  void applyLabels();

  /// Write @a table onto an arbitrary color transfer function proxy
  /// (used for a sink's detached color map). Both the RGB points and
  /// the ScalarOpacityFunction sub-proxy are rewritten.
  static void applyLabelsToProxy(const LabelTable& table,
                                 vtkSMProxy* colorMap);

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

private:
  LabelTable m_labels;

  // Scan cache key. The image pointer alone is not enough: an
  // intermediate update swaps a fresh vtkImageData into the same
  // VolumeData, and a Python operator can mutate one in place.
  vtkImageData* m_scannedImage = nullptr;
  vtkMTimeType m_scannedMTime = 0;
  QString m_scannedArray;
};

using LabelMapDataPtr = std::shared_ptr<LabelMapData>;

/// Construct the VolumeData flavor matching @a type: a LabelMapData for
/// PortType::LabelMap, a plain VolumeData otherwise. Returned as a
/// VolumeDataPtr because that is the only type PortData payloads use.
VolumeDataPtr makeVolumeData(vtkSmartPointer<vtkImageData> imageData,
                             PortType type);

/// The LabelMapData behind @a data, or nullptr when @a data is a plain
/// volume.
LabelMapDataPtr labelMapData(const VolumeDataPtr& data);

/// True when @a data could be shown as a label map even though its port
/// is not typed as one - a segmentation read from a TIFF or an EMD,
/// which arrives as an ordinary volume because the reader has no way to
/// know what the numbers mean.
///
/// Both halves are free to check: integral values are guaranteed by the
/// scalar type rather than found by scanning, and the range is already
/// cached on the data array. The range test is a conservative reject
/// rather than a definition - it passes any 8-bit volume, since one
/// cannot span more than 256 values, so it rules out continuous 16- and
/// 32-bit data and little else. Being wrong here only offers a menu
/// entry the user is free to ignore.
///
/// A label map whose values are sparse (0, 5000, 60000) is rejected
/// even though it holds three labels, because the values are all this
/// can see without scanning. Those need an operator that types the port
/// properly.
bool canInterpretAsLabelMap(const VolumeDataPtr& data);

} // namespace pipeline
} // namespace tomviz

#endif
