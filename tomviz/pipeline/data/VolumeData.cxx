/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeData.h"

#include "ComputeHistogram.h"
#include "Utilities.h"

#include <vtkColorTransferFunction.h>
#include <vtkDataArray.h>
#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkTypeInt32Array.h>
#include <vtkTypeInt8Array.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <array>


namespace tomviz {
namespace pipeline {

VolumeData::VolumeData() = default;

VolumeData::VolumeData(vtkSmartPointer<vtkImageData> imageData)
  : m_imageData(imageData)
{
  // Extract units from field data if available (legacy format stores a
  // vtkStringArray named "units" where value 0 is the unit string).
  if (m_imageData) {
    auto* fd = m_imageData->GetFieldData();
    if (fd && fd->HasArray("units")) {
      auto* arr =
        vtkStringArray::SafeDownCast(fd->GetAbstractArray("units"));
      if (arr && arr->GetNumberOfValues() > 0) {
        m_units = QString::fromStdString(arr->GetValue(0));
      }
    }

    // Seed the rename-history map with identity entries for each scalar
    // array. renameScalarArray() transfers entries as names change; the
    // serialize path then emits the {originalName: currentName} inverse.
    if (auto* pd = m_imageData->GetPointData()) {
      for (int i = 0; i < pd->GetNumberOfArrays(); ++i) {
        auto* arr = pd->GetArray(i);
        if (arr && arr->GetName()) {
          QString name = QString::fromUtf8(arr->GetName());
          m_currentToOriginal.insert(name, name);
        }
      }
    }
  }
}

VolumeData::~VolumeData()
{
  // initColorMap() registers the CTF in the session proxy manager under a
  // unique, counter-based name. The proxy manager holds its own reference,
  // so dropping our vtkSmartPointer isn't enough — without this unregister
  // the proxy lives until app exit and every load/reset cycle adds another.
  if (!m_colorMap) {
    return;
  }

  // UnRegisterProxy mutates the ParaView session proxy manager, which is
  // not thread-safe and must run on the GUI thread. A VolumeData is a port
  // payload whose lifetime is governed by shared_ptr refcounting, and the
  // pipeline executor can drop the last ref on the worker thread (inflight
  // eviction, clearHandle, the OnDisk deleter) — destroying us there.
  // Touching the proxy manager off the GUI thread races
  // pqServerManagerObserver / pqServerManagerModel and crashes in
  // onProxyRegistered. Marshal the unregister to the GUI thread; the proxy
  // manager's own reference keeps the captured proxy alive until the queued
  // call runs.
  vtkSmartPointer<vtkSMProxy> proxy = m_colorMap;
  auto unregister = [proxy]() {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(proxy);
  };

  auto* app = QCoreApplication::instance();
  if (!app || QThread::currentThread() == app->thread()) {
    unregister();
  } else {
    QMetaObject::invokeMethod(app, unregister, Qt::QueuedConnection);
  }
}

vtkImageData* VolumeData::imageData() const
{
  return m_imageData;
}

void VolumeData::setImageData(vtkSmartPointer<vtkImageData> data)
{
  m_imageData = data;
}

bool VolumeData::isValid() const
{
  return m_imageData != nullptr;
}

std::array<int, 3> VolumeData::dimensions() const
{
  std::array<int, 3> dims = { 0, 0, 0 };
  if (m_imageData) {
    m_imageData->GetDimensions(dims.data());
  }
  return dims;
}

std::array<double, 3> VolumeData::spacing() const
{
  std::array<double, 3> s = { 1.0, 1.0, 1.0 };
  if (m_imageData) {
    m_imageData->GetSpacing(s.data());
  }
  return s;
}

void VolumeData::setSpacing(double x, double y, double z)
{
  if (m_imageData) {
    m_imageData->SetSpacing(x, y, z);
  }
}

std::array<double, 3> VolumeData::origin() const
{
  std::array<double, 3> o = { 0.0, 0.0, 0.0 };
  if (m_imageData) {
    m_imageData->GetOrigin(o.data());
  }
  return o;
}

void VolumeData::setOrigin(double x, double y, double z)
{
  if (m_imageData) {
    m_imageData->SetOrigin(x, y, z);
  }
}

std::array<double, 3> VolumeData::displayPosition() const
{
  return m_displayPosition;
}

void VolumeData::setDisplayPosition(double x, double y, double z)
{
  m_displayPosition = { x, y, z };
}

std::array<double, 3> VolumeData::displayOrientation() const
{
  return m_displayOrientation;
}

void VolumeData::setDisplayOrientation(double x, double y, double z)
{
  m_displayOrientation = { x, y, z };
}

std::array<int, 6> VolumeData::extent() const
{
  std::array<int, 6> e = { 0, 0, 0, 0, 0, 0 };
  if (m_imageData) {
    m_imageData->GetExtent(e.data());
  }
  return e;
}

std::array<double, 6> VolumeData::bounds() const
{
  std::array<double, 6> b = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  if (m_imageData) {
    m_imageData->GetBounds(b.data());
  }
  return b;
}

vtkDataArray* VolumeData::scalars() const
{
  if (m_imageData) {
    return m_imageData->GetPointData()->GetScalars();
  }
  return nullptr;
}

QStringList VolumeData::scalarNames() const
{
  QStringList names;
  if (!m_imageData) {
    return names;
  }
  auto* pd = m_imageData->GetPointData();
  if (!pd) {
    return names;
  }
  const int n = pd->GetNumberOfArrays();
  names.reserve(n);
  for (int i = 0; i < n; ++i) {
    names << QString::fromUtf8(pd->GetArrayName(i));
  }
  return names;
}

QString VolumeData::activeScalarName() const
{
  auto* s = scalars();
  if (!s || !s->GetName()) {
    return {};
  }
  return QString::fromUtf8(s->GetName());
}

void VolumeData::renameScalarArray(const QString& oldName,
                                    const QString& newName)
{
  if (oldName.isEmpty() || newName.isEmpty() || oldName == newName ||
      !m_imageData) {
    return;
  }
  auto* pd = m_imageData->GetPointData();
  if (!pd) {
    return;
  }
  if (pd->HasArray(newName.toUtf8().constData())) {
    return; // target name already in use
  }
  auto* arr = pd->GetArray(oldName.toUtf8().constData());
  if (!arr) {
    return; // nothing named oldName
  }

  const bool wasActive =
    pd->GetScalars() == arr; // keep active-state across rename

  // Preserve the original name so subsequent serializes still emit the
  // correct {originalName: currentName} entry.
  const QString original =
    m_currentToOriginal.value(oldName, oldName);

  arr->SetName(newName.toUtf8().constData());
  m_currentToOriginal.remove(oldName);
  m_currentToOriginal.insert(newName, original);

  if (wasActive) {
    pd->SetActiveScalars(newName.toUtf8().constData());
  }
}

QString VolumeData::originalScalarName(const QString& currentName) const
{
  return m_currentToOriginal.value(currentName, currentName);
}

int VolumeData::numberOfComponents() const
{
  auto* s = scalars();
  return s ? s->GetNumberOfComponents() : 0;
}

std::array<double, 2> VolumeData::scalarRange() const
{
  std::array<double, 2> range = { 0.0, 0.0 };
  auto* s = scalars();
  if (s) {
    s->GetFiniteRange(range.data(), -1);
  }
  return range;
}

double VolumeData::scalarPercentile(double fraction) const
{
  auto* s = scalars();
  if (!s) {
    return 0.0;
  }
  auto range = scalarRange();
  double result = range[0];
  switch (s->GetDataType()) {
    vtkTemplateMacro(
      result = ComputePercentile(
        reinterpret_cast<VTK_TT*>(s->GetVoidPointer(0)),
        s->GetNumberOfTuples(), s->GetNumberOfComponents(), range.data(),
        fraction));
    default:
      break;
  }
  return result;
}

double VolumeData::thresholdSeed() const
{
  auto* s = scalars();
  if (!s || s->GetNumberOfTuples() <= 0) {
    return 0.0;
  }
  constexpr double kBudget = 250000.0;
  const double count = static_cast<double>(s->GetNumberOfTuples());
  const double fraction = std::max(0.95, 1.0 - kBudget / count);
  auto range = scalarRange();
  double result = range[0];
  switch (s->GetDataType()) {
    vtkTemplateMacro(
      result = ComputePercentile(
        reinterpret_cast<VTK_TT*>(s->GetVoidPointer(0)),
        s->GetNumberOfTuples(), s->GetNumberOfComponents(), range.data(),
        fraction, /*excludeMinimum=*/true));
    default:
      break;
  }
  return result;
}

std::array<double, 2> VolumeData::colorMapRange() const
{
  if (m_timeSteps.isEmpty()) {
    return scalarRange();
  }
  return m_timeSeriesRange;
}

QString VolumeData::label() const
{
  return m_label;
}

void VolumeData::setLabel(const QString& label)
{
  m_label = label;
}

bool VolumeData::hasColorMap() const
{
  return m_ctf != nullptr;
}

void VolumeData::initColorMap()
{
  if (m_ctf) {
    return; // already initialized
  }

  auto* mgr = vtkSMProxyManager::GetProxyManager();
  if (!mgr) {
    return;
  }
  auto* pxm = mgr->GetActiveSessionProxyManager();
  if (!pxm) {
    return;
  }

  static unsigned int counter = 0;
  ++counter;

  vtkNew<vtkSMTransferFunctionManager> tfmgr;
  m_colorMap = tfmgr->GetColorTransferFunction(
    QString("VolumeDataColorMap%1").arg(counter).toLatin1().data(), pxm);

  // Cache client-side VTK objects for direct manipulation
  if (m_colorMap) {
    m_ctf = vtkColorTransferFunction::SafeDownCast(
      m_colorMap->GetClientSideObject());

    auto* omap =
      vtkSMPropertyHelper(m_colorMap, "ScalarOpacityFunction").GetAsProxy();
    if (omap) {
      m_opacity =
        vtkPiecewiseFunction::SafeDownCast(omap->GetClientSideObject());
    }
  }
}

vtkSMProxy* VolumeData::colorMap()
{
  if (!m_colorMap) {
    initColorMap();
  }
  return m_colorMap;
}

vtkSMProxy* VolumeData::opacityMap()
{
  auto* cmap = colorMap();
  if (!cmap) {
    return nullptr;
  }
  return vtkSMPropertyHelper(cmap, "ScalarOpacityFunction").GetAsProxy();
}

vtkColorTransferFunction* VolumeData::colorTransferFunction() const
{
  return m_ctf;
}

vtkPiecewiseFunction* VolumeData::scalarOpacity() const
{
  return m_opacity;
}

vtkPiecewiseFunction* VolumeData::gradientOpacity() const
{
  return m_gradientOpacity;
}

void VolumeData::syncColorMapToProxy()
{
  if (!m_colorMap) {
    return;
  }

  // The functions were edited directly, so the VTK objects already
  // hold these points; the proxies only need to record them, for state
  // files and for vtkSMTransferFunctionProxy::RescaleTransferFunction,
  // which reads the property. Without that the proxy keeps its stale
  // default points and the next rescale wipes what was just written.
  // Recording rather than pushing matters: a push replays AddRGBPoint
  // per point, quadratic in their number, and a label map carries two
  // per label. Use a contiguous 4*N buffer for the opacity Points,
  // matching what RGBPoints does on the CTF side; per-element Set()
  // leaves the property out of sync.
  if (m_ctf && m_ctf->GetSize() > 0) {
    recordProxyValues(m_colorMap, "RGBPoints", m_ctf->GetDataPointer(),
                      m_ctf->GetSize() * 4);
  }
  m_colorMap->UpdateVTKObjects();

  auto* omap =
    vtkSMPropertyHelper(m_colorMap, "ScalarOpacityFunction").GetAsProxy();
  if (omap && m_opacity && m_opacity->GetSize() > 0) {
    const int n = m_opacity->GetSize();
    std::vector<double> buffer(4 * n);
    for (int i = 0; i < n; ++i) {
      m_opacity->GetNodeValue(i, buffer.data() + 4 * i);
    }
    recordProxyValues(omap, "Points", buffer.data(),
                      static_cast<unsigned int>(buffer.size()));
    omap->UpdateVTKObjects();
  }
}

void VolumeData::rescaleColorMap()
{
  if (!m_colorMap) {
    return;
  }

  auto range = colorMapRange();
  double r[2] = { range[0], range[1] };

  vtkSMTransferFunctionProxy::RescaleTransferFunction(m_colorMap, r);

  auto* omap =
    vtkSMPropertyHelper(m_colorMap, "ScalarOpacityFunction").GetAsProxy();
  if (omap) {
    vtkSMTransferFunctionProxy::RescaleTransferFunction(omap, r);
  }
}

void VolumeData::copyColorMapFrom(const VolumeData& source)
{
  if (!m_ctf || !source.m_ctf) {
    return;
  }

  // Copy color transfer function control points
  auto* srcCTF = source.m_ctf;
  if (srcCTF->GetSize() > 0) {
    m_ctf->RemoveAllPoints();
    int n = srcCTF->GetSize();
    double* data = srcCTF->GetDataPointer();
    for (int i = 0; i < n; ++i) {
      double* p = data + i * 4; // X, R, G, B
      m_ctf->AddRGBPoint(p[0], p[1], p[2], p[3]);
    }
    m_ctf->Modified();
  }

  // Copy opacity function control points
  auto* srcPWF = source.m_opacity;
  if (m_opacity && srcPWF && srcPWF->GetSize() > 0) {
    m_opacity->RemoveAllPoints();
    int n = srcPWF->GetSize();
    for (int i = 0; i < n; ++i) {
      double val[4]; // X, Y, Midpoint, Sharpness
      srcPWF->GetNodeValue(i, val);
      m_opacity->AddPoint(val[0], val[1], val[2], val[3]);
    }
    m_opacity->Modified();
  }

  // Copy gradient opacity
  auto* srcGrad = source.gradientOpacity();
  if (srcGrad && srcGrad->GetSize() > 0) {
    m_gradientOpacity->RemoveAllPoints();
    int n = srcGrad->GetSize();
    for (int i = 0; i < n; ++i) {
      double val[4]; // X, Y, Midpoint, Sharpness
      srcGrad->GetNodeValue(i, val);
      m_gradientOpacity->AddPoint(val[0], val[1], val[2], val[3]);
    }
    m_gradientOpacity->Modified();
  }

  // Sync the VTK object state into the SM proxy so that proxy-level
  // operations (e.g. rescaleColorMap, RescaleTransferFunction) see
  // the copied values rather than the stale defaults.
  syncColorMapToProxy();
}

void VolumeData::copyAndRescaleColorMapFrom(const VolumeData& source)
{
  copyColorMapFrom(source);
  rescaleColorMap();
}

QString VolumeData::units() const
{
  return m_units;
}

void VolumeData::setUnits(const QString& units)
{
  m_units = units;
}

bool VolumeData::hasTiltAngles() const
{
  return hasTiltAngles(m_imageData);
}

QVector<double> VolumeData::tiltAngles() const
{
  return getTiltAngles(m_imageData);
}

void VolumeData::setTiltAngles(const QVector<double>& angles)
{
  setTiltAngles(m_imageData, angles);
}

bool VolumeData::hasTiltAngles(vtkImageData* image)
{
  if (!image || !image->GetFieldData()) {
    return false;
  }
  auto* arr = image->GetFieldData()->GetArray("tilt_angles");
  return arr != nullptr && arr->GetNumberOfTuples() > 0;
}

QVector<double> VolumeData::getTiltAngles(vtkImageData* image)
{
  QVector<double> result;
  if (!image || !image->GetFieldData()) {
    return result;
  }
  auto* arr = image->GetFieldData()->GetArray("tilt_angles");
  if (!arr) {
    return result;
  }
  result.resize(arr->GetNumberOfTuples());
  for (vtkIdType i = 0; i < arr->GetNumberOfTuples(); ++i) {
    result[i] = arr->GetComponent(i, 0);
  }
  return result;
}

void VolumeData::setTiltAngles(vtkImageData* image,
                               const QVector<double>& angles)
{
  if (!image) {
    return;
  }
  vtkNew<vtkDoubleArray> array;
  array->SetName("tilt_angles");
  array->SetNumberOfTuples(angles.size());
  for (int i = 0; i < angles.size(); ++i) {
    array->SetValue(i, angles[i]);
  }
  image->GetFieldData()->AddArray(array);
}

bool VolumeData::hasScanIds() const
{
  return hasScanIds(m_imageData);
}

QVector<int> VolumeData::scanIds() const
{
  return getScanIds(m_imageData);
}

void VolumeData::setScanIds(const QVector<int>& ids)
{
  setScanIds(m_imageData, ids);
}

bool VolumeData::hasScanIds(vtkImageData* image)
{
  if (!image || !image->GetFieldData()) {
    return false;
  }
  auto* arr = image->GetFieldData()->GetArray("scan_ids");
  return arr != nullptr && arr->GetNumberOfTuples() > 0;
}

QVector<int> VolumeData::getScanIds(vtkImageData* image)
{
  QVector<int> result;
  if (!image || !image->GetFieldData()) {
    return result;
  }
  auto* arr = image->GetFieldData()->GetArray("scan_ids");
  if (!arr) {
    return result;
  }
  result.resize(arr->GetNumberOfTuples());
  for (vtkIdType i = 0; i < arr->GetNumberOfTuples(); ++i) {
    result[i] = static_cast<int>(arr->GetComponent(i, 0));
  }
  return result;
}

void VolumeData::setScanIds(vtkImageData* image, const QVector<int>& ids)
{
  if (!image) {
    return;
  }
  vtkNew<vtkTypeInt32Array> array;
  array->SetName("scan_ids");
  array->SetNumberOfTuples(ids.size());
  for (int i = 0; i < ids.size(); ++i) {
    array->SetValue(i, ids[i]);
  }
  image->GetFieldData()->AddArray(array);
}

void VolumeData::clearScanIds(vtkImageData* image)
{
  if (image && image->GetFieldData()) {
    image->GetFieldData()->RemoveArray("scan_ids");
  }
}

bool VolumeData::wasSubsampled(vtkImageData* image)
{
  if (!image || !image->GetFieldData()) {
    return false;
  }
  auto* arr = image->GetFieldData()->GetArray("was_subsampled");
  return arr != nullptr && arr->GetNumberOfTuples() > 0 &&
         arr->GetComponent(0, 0) != 0;
}

void VolumeData::setWasSubsampled(vtkImageData* image, bool b)
{
  if (!image) {
    return;
  }
  vtkNew<vtkTypeInt8Array> array;
  array->SetName("was_subsampled");
  array->SetNumberOfTuples(1);
  array->SetValue(0, b ? 1 : 0);
  image->GetFieldData()->AddArray(array);
}

void VolumeData::subsampleStrides(vtkImageData* image, int strides[3])
{
  for (int i = 0; i < 3; ++i) {
    strides[i] = 1;
  }
  if (!image || !image->GetFieldData()) {
    return;
  }
  auto* arr = image->GetFieldData()->GetArray("subsample_strides");
  if (arr && arr->GetNumberOfTuples() >= 3) {
    for (int i = 0; i < 3; ++i) {
      strides[i] = static_cast<int>(arr->GetComponent(i, 0));
    }
  }
}

void VolumeData::setSubsampleStrides(vtkImageData* image, int strides[3])
{
  if (!image) {
    return;
  }
  vtkNew<vtkTypeInt32Array> array;
  array->SetName("subsample_strides");
  array->SetNumberOfTuples(3);
  for (int i = 0; i < 3; ++i) {
    array->SetValue(i, strides[i]);
  }
  image->GetFieldData()->AddArray(array);
}

void VolumeData::subsampleVolumeBounds(vtkImageData* image, int bounds[6])
{
  for (int i = 0; i < 6; ++i) {
    bounds[i] = -1;
  }
  if (!image || !image->GetFieldData()) {
    return;
  }
  auto* arr = image->GetFieldData()->GetArray("subsample_volume_bounds");
  if (arr && arr->GetNumberOfTuples() >= 6) {
    for (int i = 0; i < 6; ++i) {
      bounds[i] = static_cast<int>(arr->GetComponent(i, 0));
    }
  }
}

void VolumeData::setSubsampleVolumeBounds(vtkImageData* image, int bounds[6])
{
  if (!image) {
    return;
  }
  vtkNew<vtkTypeInt32Array> array;
  array->SetName("subsample_volume_bounds");
  array->SetNumberOfTuples(6);
  for (int i = 0; i < 6; ++i) {
    array->SetValue(i, bounds[i]);
  }
  image->GetFieldData()->AddArray(array);
}

void VolumeData::setType(vtkImageData* image, DataType type)
{
  if (!image) {
    return;
  }
  vtkNew<vtkTypeInt8Array> array;
  array->SetName("tomviz_data_source_type");
  array->SetNumberOfTuples(1);
  array->SetValue(0, static_cast<int>(type));
  image->GetFieldData()->AddArray(array);

  if (type != DataType::TiltSeries) {
    // Mirror legacy DataSource: a non-tilt-series type has no tilt angles.
    image->GetFieldData()->RemoveArray("tilt_angles");
  }
}

void VolumeData::setTimeSteps(const QList<TimeStep>& steps)
{
  m_timeSteps = steps;
  if (!steps.isEmpty()) {
    m_currentTimeStep = 0;
    m_imageData = steps[0].image;
  }

  std::array<double, 2> range = { 0.0, 0.0 };
  bool first = true;
  for (const auto& step : steps) {
    auto* scalars =
      step.image ? step.image->GetPointData()->GetScalars() : nullptr;
    if (!scalars) {
      continue;
    }
    double r[2];
    scalars->GetFiniteRange(r, -1);
    range[0] = first ? r[0] : std::min(range[0], r[0]);
    range[1] = first ? r[1] : std::max(range[1], r[1]);
    first = false;
  }

  if (range != m_timeSeriesRange) {
    m_timeSeriesRange = range;
    // The color map was built for whichever image was loaded first; it
    // now has to cover every step so the frames stay comparable.
    rescaleColorMap();
  }
}

QList<VolumeData::TimeStep> VolumeData::timeSteps() const
{
  return m_timeSteps;
}

bool VolumeData::hasTimeSteps() const
{
  return !m_timeSteps.isEmpty();
}

int VolumeData::currentTimeStepIndex() const
{
  return m_currentTimeStep;
}

void VolumeData::switchTimeStep(int index)
{
  if (index < 0 || index >= m_timeSteps.size()) {
    return;
  }
  m_currentTimeStep = index;
  m_imageData = m_timeSteps[index].image;
}

namespace {

QJsonArray toJsonArray(const std::array<double, 3>& a)
{
  return QJsonArray{ a[0], a[1], a[2] };
}

} // namespace

QJsonObject VolumeData::serialize() const
{
  QJsonObject json;
  if (!m_label.isEmpty()) {
    json["label"] = m_label;
  }
  if (!m_units.isEmpty()) {
    json["units"] = m_units;
  }
  if (m_imageData) {
    json["spacing"] = toJsonArray(spacing());
  }
  // "origin" carries the display-side translation (what legacy called
  // "origin" in .tvsm). vtkImageData's intrinsic Origin isn't persisted
  // because tomviz never modifies it — it's a property of the file and
  // is restored on reload.
  json["origin"] = toJsonArray(m_displayPosition);
  json["orientation"] = toJsonArray(m_displayOrientation);

  // Active-scalar name and rename history, mirroring legacy DataSource.
  // scalarsRename is emitted as {originalName: currentName} pairs.
  if (m_imageData) {
    if (auto* scalars = m_imageData->GetPointData()
                          ? m_imageData->GetPointData()->GetScalars()
                          : nullptr) {
      if (scalars->GetName()) {
        json["activeScalars"] = QString::fromUtf8(scalars->GetName());
      }
    }
  }
  if (!m_currentToOriginal.isEmpty()) {
    QJsonObject rename;
    for (auto it = m_currentToOriginal.constBegin();
         it != m_currentToOriginal.constEnd(); ++it) {
      // key: currentName  value: originalName  →  inverse on disk
      rename[it.value()] = it.key();
    }
    json["scalarsRename"] = rename;
  }

  if (m_colorMap) {
    json["colorOpacityMap"] = tomviz::serialize(m_colorMap);
  }
  if (m_gradientOpacity && m_gradientOpacity->GetSize() > 0) {
    json["gradientOpacityMap"] = tomviz::serialize(m_gradientOpacity.Get());
  }
  return json;
}

bool VolumeData::deserialize(const QJsonObject& json)
{
  if (json.contains("label")) {
    m_label = json.value("label").toString();
  }
  if (json.contains("units")) {
    setUnits(json.value("units").toString());
  }
  if (json.contains("spacing")) {
    auto arr = json.value("spacing").toArray();
    if (arr.size() == 3) {
      setSpacing(arr.at(0).toDouble(), arr.at(1).toDouble(),
                 arr.at(2).toDouble());
    }
  }
  // "origin" is the display-side translation (what legacy DataSource
  // called "origin" in its .tvsm output). Nothing in tomviz mutates
  // vtkImageData's intrinsic origin after load, so we don't touch it.
  if (json.contains("origin")) {
    auto arr = json.value("origin").toArray();
    if (arr.size() == 3) {
      setDisplayPosition(arr.at(0).toDouble(), arr.at(1).toDouble(),
                         arr.at(2).toDouble());
    }
  }
  if (json.contains("orientation")) {
    auto arr = json.value("orientation").toArray();
    if (arr.size() == 3) {
      setDisplayOrientation(arr.at(0).toDouble(), arr.at(1).toDouble(),
                            arr.at(2).toDouble());
    }
  }

  // Rename scalar arrays back to the names they had when the state was
  // saved. Legacy serialized this as {originalName: currentName} pairs;
  // we replay the rename so the freshly-loaded array ends up with the
  // saved display name. Routed through renameScalarArray() so the
  // rename-history map stays in sync.
  if (m_imageData && json.contains("scalarsRename")) {
    auto renames = json.value("scalarsRename").toObject();
    for (auto it = renames.constBegin(); it != renames.constEnd(); ++it) {
      renameScalarArray(it.key(), it.value().toString());
    }
  }

  // DataSource-level "activeScalars" is a scalar array name (string).
  // Per-sink "activeScalars" is an index (int) and is handled in each
  // sink's deserialize — deliberately not applied here.
  if (m_imageData && json.value("activeScalars").isString()) {
    auto name = json.value("activeScalars").toString();
    if (auto* pd = m_imageData->GetPointData()) {
      if (pd->HasArray(name.toUtf8().constData())) {
        pd->SetActiveScalars(name.toUtf8().constData());
      }
    }
  }

  if (json.contains("colorOpacityMap")) {
    // Ensure the SM proxy and our cached client-side VTK pointers exist.
    colorMap();
    auto cmap = json.value("colorOpacityMap").toObject();

    // The discretizable-CTF overload applies colors + colorSpace, and
    // its own internal ScalarOpacityFunction, but that may be a
    // different PWF than the one the ScalarOpacityFunction sub-proxy
    // exposes (which is what the sinks read via opacityMap()), so we
    // also apply the opacity points to m_opacity directly.
    if (auto* disc =
          vtkDiscretizableColorTransferFunction::SafeDownCast(m_ctf)) {
      tomviz::deserialize(disc, cmap);
    }
    if (m_opacity) {
      tomviz::deserialize(m_opacity, cmap);
    }
    // Client-side edits don't automatically update the proxy's cached
    // property state — push them up so proxy consumers see them.
    syncColorMapToProxy();
  }
  if (json.contains("gradientOpacityMap")) {
    tomviz::deserialize(m_gradientOpacity.Get(),
                        json.value("gradientOpacityMap").toObject());
  }
  return true;
}

} // namespace pipeline
} // namespace tomviz
