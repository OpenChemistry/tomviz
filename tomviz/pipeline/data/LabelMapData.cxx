/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelMapData.h"

#include "ColorMap.h"
#include "Utilities.h"

#include <vtkArrayDispatch.h>
#include <vtkColorTransferFunction.h>
#include <vtkDataArray.h>
#include <vtkDataArrayRange.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace tomviz {
namespace pipeline {

namespace {

// Half-width of each label's constant-color band in the transfer
// functions. Two nodes per label at value -/+ this give every integer
// label its own flat band under plain linear interpolation, with the
// ramps between bands sitting at non-integer values no voxel occupies.
// Must match the spacing buildSegmentationPreset() uses, and must stay
// at 0.25 so the smallest node gap is 0.5: VTK sizes the GPU lookup
// texture from that gap.
constexpr double kBandHalfWidth = 0.25;

/// Collect distinct values and their voxel counts from a single
/// component of @a array, stopping once maxLabels distinct values have
/// been seen.
struct ScanWorker
{
  std::unordered_map<double, vtkIdType> counts;
  bool truncated = false;

  template <typename ArrayT>
  void operator()(ArrayT* array)
  {
    const auto range = vtk::DataArrayValueRange<1>(array);
    for (auto value : range) {
      double key = static_cast<double>(value);
      auto it = counts.find(key);
      if (it != counts.end()) {
        ++it->second;
        continue;
      }
      if (static_cast<int>(counts.size()) >= LabelTable::maxLabels) {
        // Keep counting the labels we already know about, but stop
        // admitting new ones so a pathological input (a float array
        // mislabeled as a label map, say) can't exhaust memory.
        truncated = true;
        continue;
      }
      counts.emplace(key, 1);
    }
  }
};

/// True when @a array can be interpreted as a set of labels. Floating
/// point rules itself out: its values are samples of a continuous
/// quantity, and enumerating them is meaningless.
bool isLabelArray(vtkDataArray* array)
{
  if (!array || array->GetNumberOfComponents() != 1 ||
      array->GetNumberOfTuples() == 0) {
    return false;
  }
  const int dataType = array->GetDataType();
  return dataType != VTK_FLOAT && dataType != VTK_DOUBLE;
}

/// Build the (position, value) node list shared by the color and
/// opacity transfer functions: two nodes per label bracketing its
/// value, with the outermost edges pinned exactly to the first and last
/// label so that a rescale to the data range is an identity.
QVector<QPair<double, int>> bandNodes(const LabelTable& table)
{
  QVector<QPair<double, int>> nodes;
  const int last = table.count() - 1;
  nodes.reserve(2 * table.count());
  for (int i = 0; i <= last; ++i) {
    double value = table.at(i).value;
    double low = (i == 0) ? 0.0 : -kBandHalfWidth;
    double high = (i == last) ? 0.0 : kBandHalfWidth;
    nodes.append({ value + low, i });
    nodes.append({ value + high, i });
  }
  return nodes;
}

/// Rebuild @a ctf and @a opacity from the table's color bands.
///
/// In bulk, not a point at a time: every insert re-sorts the function
/// and fires ModifiedEvent, and the histogram widget answers each one
/// with a render and a copy into the proxy. Two points per label made
/// that thousands of renders for a segmentation of a few thousand
/// particles. AddRGBPoints refuses to check for duplicates, so the one
/// case bandNodes produces them (a single-entry table, where both pins
/// land on the value) is folded here.
void fillLabelFunctions(const LabelTable& table,
                        vtkColorTransferFunction* ctf,
                        vtkPiecewiseFunction* opacity)
{
  const auto nodes = bandNodes(table);

  vtkNew<vtkDoubleArray> xs;
  vtkNew<vtkDoubleArray> rgb;
  rgb->SetNumberOfComponents(3);
  std::vector<double> opacityPoints;
  opacityPoints.reserve(2 * nodes.size());
  for (const auto& node : nodes) {
    if (xs->GetNumberOfValues() > 0 &&
        xs->GetValue(xs->GetNumberOfValues() - 1) == node.first) {
      continue;
    }
    const auto& entry = table.at(node.second);
    xs->InsertNextValue(node.first);
    rgb->InsertNextTuple3(entry.color.redF(), entry.color.greenF(),
                          entry.color.blueF());
    opacityPoints.push_back(node.first);
    opacityPoints.push_back(entry.visible ? 1.0 : 0.0);
  }

  ctf->RemoveAllPoints();
  const bool duplicates = ctf->GetAllowDuplicateScalars();
  ctf->AllowDuplicateScalarsOn();
  ctf->AddRGBPoints(xs, rgb);
  ctf->SetAllowDuplicateScalars(duplicates);
  opacity->FillFromDataPointer(static_cast<int>(opacityPoints.size() / 2),
                               opacityPoints.data());
}

} // namespace

QString LabelEntry::displayName() const
{
  if (!name.isEmpty()) {
    return name;
  }
  if (value == 0.0) {
    return QObject::tr("Background");
  }
  return QObject::tr("Label %1").arg(value);
}

int LabelTable::indexOfValue(double value) const
{
  for (int i = 0; i < m_entries.size(); ++i) {
    if (m_entries[i].value == value) {
      return i;
    }
  }
  return -1;
}

void LabelTable::setName(int index, const QString& name)
{
  if (index >= 0 && index < m_entries.size()) {
    m_entries[index].name = name;
  }
}

void LabelTable::setColor(int index, const QColor& color)
{
  if (index >= 0 && index < m_entries.size()) {
    m_entries[index].color = color;
  }
}

void LabelTable::setVisible(int index, bool visible)
{
  if (index >= 0 && index < m_entries.size()) {
    m_entries[index].visible = visible;
  }
}

void LabelTable::setAllVisible(bool visible)
{
  for (auto& entry : m_entries) {
    entry.visible = visible;
  }
}

void LabelTable::reconcile(
  const QVector<QPair<double, vtkIdType>>& scanned, bool truncated)
{
  QVector<LabelEntry> updated;
  updated.reserve(scanned.size());

  // Tens of thousands of labels are routine, so index the old table
  // once rather than scan it per scanned value.
  std::unordered_map<double, int> existingIndex;
  existingIndex.reserve(m_entries.size());
  for (int i = 0; i < m_entries.size(); ++i) {
    existingIndex.emplace(m_entries[i].value, i);
  }

  for (const auto& pair : scanned) {
    auto found = existingIndex.find(pair.first);
    if (found != existingIndex.end()) {
      LabelEntry entry = m_entries[found->second];
      entry.voxelCount = pair.second;
      updated.append(entry);
      continue;
    }
    LabelEntry entry;
    entry.value = pair.first;
    entry.voxelCount = pair.second;
    entry.color = tomviz::segmentationLabelColor(m_nextColorIndex++);
    // 0 is the conventional background label: showing it would wrap
    // every other label in an opaque block.
    entry.visible = pair.first != 0.0;
    updated.append(entry);
  }

  m_entries = updated;
  m_truncated = truncated;
}

QJsonObject LabelTable::serialize() const
{
  QJsonArray labels;
  for (const auto& entry : m_entries) {
    QJsonObject obj;
    obj["value"] = entry.value;
    if (!entry.name.isEmpty()) {
      obj["name"] = entry.name;
    }
    obj["color"] = QJsonArray{ entry.color.redF(), entry.color.greenF(),
                               entry.color.blueF() };
    obj["visible"] = entry.visible;
    labels.append(obj);
  }

  QJsonObject json;
  json["labels"] = labels;
  json["nextColorIndex"] = m_nextColorIndex;
  return json;
}

bool LabelTable::deserialize(const QJsonObject& json)
{
  if (!json.contains("labels")) {
    return true;
  }

  m_entries.clear();
  for (const auto& value : json.value("labels").toArray()) {
    QJsonObject obj = value.toObject();
    LabelEntry entry;
    entry.value = obj.value("value").toDouble();
    entry.name = obj.value("name").toString();
    entry.visible = obj.value("visible").toBool(true);
    auto rgb = obj.value("color").toArray();
    if (rgb.size() == 3) {
      entry.color = QColor::fromRgbF(rgb.at(0).toDouble(),
                                     rgb.at(1).toDouble(),
                                     rgb.at(2).toDouble());
    }
    m_entries.append(entry);
  }
  // Voxel counts are not saved: they are a property of the data, which
  // the next refreshLabels() rescan supplies. Default the palette
  // cursor past the restored labels so a label added after the load
  // can't be handed a color one of them already has.
  m_nextColorIndex =
    json.value("nextColorIndex").toInt(m_entries.size());
  return true;
}

LabelMapData::LabelMapData() = default;

LabelMapData::LabelMapData(vtkSmartPointer<vtkImageData> imageData)
  : VolumeData(imageData)
{}

LabelMapData::~LabelMapData() = default;

bool LabelMapData::labelsSupported() const
{
  return isLabelArray(scalars());
}

void LabelMapData::refreshLabels(bool force)
{
  auto* array = scalars();
  if (!isLabelArray(array)) {
    return;
  }

  auto* image = imageData();
  QString arrayName = array->GetName()
                        ? QString::fromUtf8(array->GetName())
                        : QString();
  if (!force && image == m_scannedImage &&
      image->GetMTime() == m_scannedMTime && arrayName == m_scannedArray) {
    return;
  }

  ScanWorker worker;
  // Integer arrays only, which is what isLabelArray() already
  // guarantees; the fallback covers array subclasses the dispatcher
  // does not have a compiled path for.
  using Dispatcher = vtkArrayDispatch::DispatchByValueType<
    vtkArrayDispatch::Integrals>;
  if (!Dispatcher::Execute(array, worker)) {
    worker(array);
  }

  QVector<QPair<double, vtkIdType>> scanned;
  scanned.reserve(static_cast<int>(worker.counts.size()));
  for (const auto& entry : worker.counts) {
    scanned.append({ entry.first, entry.second });
  }
  std::sort(scanned.begin(), scanned.end(),
            [](const QPair<double, vtkIdType>& a,
               const QPair<double, vtkIdType>& b) {
              return a.first < b.first;
            });

  m_labels.reconcile(scanned, worker.truncated);

  m_scannedImage = image;
  m_scannedMTime = image ? image->GetMTime() : 0;
  m_scannedArray = arrayName;
}

void LabelMapData::adoptLabelsFrom(const LabelMapData& previous)
{
  m_labels = previous.m_labels;
  // The adopted table describes the previous payload's label set, not
  // ours, so it has to be reconciled even when our scan cache is warm.
  refreshLabels(/*force=*/true);
}

void LabelMapData::applyLabels()
{
  initColorMap();
  auto* ctf = colorTransferFunction();
  auto* opacity = scalarOpacity();
  if (!ctf || !opacity || m_labels.isEmpty()) {
    return;
  }

  fillLabelFunctions(m_labels, ctf, opacity);

  // Without this the proxy keeps its stale default points, and the next
  // rescale (per-node post-execution, or the user's "Reset data range")
  // rescales those and wipes what we just wrote.
  syncColorMapToProxy();
}

void LabelMapData::applyLabelsToProxy(const LabelTable& table,
                                      vtkSMProxy* colorMap)
{
  if (!colorMap || table.isEmpty()) {
    return;
  }
  auto* ctf =
    vtkColorTransferFunction::SafeDownCast(colorMap->GetClientSideObject());
  auto* omap =
    vtkSMPropertyHelper(colorMap, "ScalarOpacityFunction").GetAsProxy();
  auto* opacity =
    omap ? vtkPiecewiseFunction::SafeDownCast(omap->GetClientSideObject())
         : nullptr;
  if (!ctf || !opacity) {
    return;
  }

  // Same as applyLabels, on a detached map: fill the VTK objects in
  // bulk, then record the points on the proxies. Pushing them instead
  // would replay AddPoint per point, quadratic in the label count.
  fillLabelFunctions(table, ctf, opacity);
  recordProxyValues(colorMap, "RGBPoints", ctf->GetDataPointer(),
                    static_cast<unsigned int>(ctf->GetSize() * 4));
  colorMap->UpdateVTKObjects();
  const int n = opacity->GetSize();
  std::vector<double> points(4 * n);
  for (int i = 0; i < n; ++i) {
    opacity->GetNodeValue(i, points.data() + 4 * i);
  }
  recordProxyValues(omap, "Points", points.data(),
                    static_cast<unsigned int>(points.size()));
  omap->UpdateVTKObjects();
}

QJsonObject LabelMapData::serialize() const
{
  auto json = VolumeData::serialize();
  json["labelMap"] = m_labels.serialize();
  return json;
}

bool LabelMapData::deserialize(const QJsonObject& json)
{
  if (!VolumeData::deserialize(json)) {
    return false;
  }
  if (json.contains("labelMap")) {
    m_labels.deserialize(json.value("labelMap").toObject());
    // The restored table describes the data as it was when the state
    // was saved: it carries no voxel counts, and the labels it lists
    // may not be the ones this data actually holds. Reconcile even if
    // the scan cache is warm, which it is whenever anything looked at
    // the labels before the state arrived.
    refreshLabels(/*force=*/true);
    // The saved colorOpacityMap was applied by the base class, but the
    // label table outranks it: reproject so a state file written before
    // a label edit landed can't win over the table.
    applyLabels();
  }
  return true;
}

VolumeDataPtr makeVolumeData(vtkSmartPointer<vtkImageData> imageData,
                             PortType type)
{
  if (type == PortType::LabelMap) {
    return std::make_shared<LabelMapData>(imageData);
  }
  return std::make_shared<VolumeData>(imageData);
}

bool canInterpretAsLabelMap(const VolumeDataPtr& data)
{
  if (!data || !data->isValid() || !isLabelArray(data->scalars())) {
    return false;
  }

  auto range = data->scalarRange();
  // Inclusive of both ends: 0..maxLabels-1 is maxLabels distinct values.
  return range[1] - range[0] + 1 <= LabelTable::maxLabels;
}

LabelMapDataPtr labelMapData(const VolumeDataPtr& data)
{
  return std::dynamic_pointer_cast<LabelMapData>(data);
}

} // namespace pipeline
} // namespace tomviz
