/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThresholdSink.h"
#include "ThresholdSinkWidget.h"

#include "data/VolumeData.h"

#include <QCheckBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <vtkCompositeRepresentation.h>
#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkGeometryRepresentation.h>
#include <vtkPVLODActor.h>
#include <vtkImageData.h>
#include <vtkMapper.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMPVRepresentationProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>

namespace tomviz {
namespace pipeline {

ThresholdSink::ThresholdSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Threshold");
}

ThresholdSink::~ThresholdSink()
{
  finalize();
}

QIcon ThresholdSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqThreshold.svg"));
}

void ThresholdSink::setVisibility(bool visible)
{
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility")
      .Set(visible ? 1 : 0);
    m_thresholdRepresentation->UpdateVTKObjects();
  }

  LegacyModuleSink::setVisibility(visible);
}

bool ThresholdSink::isColorMapNeeded() const
{
  return true;
}

bool ThresholdSink::initialize(vtkSMViewProxy* vtkView)
{
  if (!LegacyModuleSink::initialize(vtkView)) {
    return false;
  }
  // SM pipeline is created lazily in setupOrUpdatePipeline() when
  // the first real data arrives via consume().
  return true;
}

bool ThresholdSink::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  if (m_thresholdRepresentation) {
    controller->UnRegisterProxy(m_thresholdRepresentation);
  }
  if (m_thresholdFilter) {
    controller->UnRegisterProxy(m_thresholdFilter);
  }
  if (m_producer) {
    controller->UnRegisterProxy(m_producer);
  }
  m_thresholdRepresentation = nullptr;
  m_thresholdFilter = nullptr;
  m_producer = nullptr;
  return LegacyModuleSink::finalize();
}

void ThresholdSink::clearVisualization()
{
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility").Set(0);
    m_thresholdRepresentation->UpdateVTKObjects();
  }
}

bool ThresholdSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  // Cache image and scalar range for the main-thread pipeline setup.
  m_pendingImage = volume->imageData();

  auto range = volume->scalarRange();
  m_scalarRange[0] = range[0];
  m_scalarRange[1] = range[1];

  // Cache scalar array names
  m_scalarArrayNames.clear();
  auto* pointData = volume->imageData()->GetPointData();
  for (int i = 0; i < pointData->GetNumberOfArrays(); ++i) {
    auto* arr = pointData->GetArray(i);
    if (arr && arr->GetName()) {
      m_scalarArrayNames.append(QString::fromUtf8(arr->GetName()));
    }
  }

  // Initialize default color-by array name if not set
  if (m_colorByArrayName.isEmpty() && !m_scalarArrayNames.isEmpty()) {
    auto* scalars = pointData->GetScalars();
    if (scalars && scalars->GetName()) {
      m_colorByArrayName = QString::fromUtf8(scalars->GetName());
    }
  }

  // Auto-set to the middle 10% of range if not explicitly set
  // (copied from old ModuleThreshold::initialize).
  if (!m_rangeSet) {
    double delta = (range[1] - range[0]);
    double mid = (range[0] + range[1]) / 2.0;
    m_lower = mid - 0.1 * delta;
    m_upper = mid + 0.1 * delta;
  }

  // SM proxy work must happen on the GUI thread; consume() runs there
  // now (consumeOnGuiThread()).
  setupOrUpdatePipeline();

  onMetadataChanged();
  return true;
}

void ThresholdSink::setupOrUpdatePipeline()
{
  if (!m_pendingImage || !view()) {
    return;
  }

  vtkSMViewProxy* vtkView = view();

  if (!m_producer) {
    // --- First time: build the full SM pipeline (like ModuleThreshold) ---

    vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
    vtkSMSessionProxyManager* pxm = vtkView->GetSessionProxyManager();

    // Create TrivialProducer with real data.
    vtkSmartPointer<vtkSMProxy> producerProxy;
    producerProxy.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
    m_producer = vtkSMSourceProxy::SafeDownCast(producerProxy);
    Q_ASSERT(m_producer);
    controller->PreInitializeProxy(m_producer);
    controller->PostInitializeProxy(m_producer);
    controller->RegisterPipelineProxy(m_producer);

    auto* tp = vtkTrivialProducer::SafeDownCast(
      m_producer->GetClientSideObject());
    tp->SetOutput(m_pendingImage);
    m_producer->UpdatePipeline();

    // Create the threshold filter (copied from ModuleThreshold::initialize).
    vtkSmartPointer<vtkSMProxy> proxy;
    proxy.TakeReference(pxm->NewProxy("filters", "Threshold"));

    m_thresholdFilter = vtkSMSourceProxy::SafeDownCast(proxy);
    Q_ASSERT(m_thresholdFilter);
    controller->PreInitializeProxy(m_thresholdFilter);
    vtkSMPropertyHelper(m_thresholdFilter, "Input").Set(m_producer);
    controller->PostInitializeProxy(m_thresholdFilter);
    controller->RegisterPipelineProxy(m_thresholdFilter);

    vtkSMPropertyHelper(m_thresholdFilter, "ThresholdMethod").Set("Between");

    // Update min/max to avoid thresholding the full dataset.
    vtkSMPropertyHelper(m_thresholdFilter, "LowerThreshold").Set(m_lower);
    vtkSMPropertyHelper(m_thresholdFilter, "UpperThreshold").Set(m_upper);
    m_thresholdFilter->UpdateVTKObjects();

    // Create the representation for it.
    m_thresholdRepresentation =
      controller->Show(m_thresholdFilter, 0, vtkView);
    Q_ASSERT(m_thresholdRepresentation);
    vtkSMRepresentationProxy::SetRepresentationType(
      m_thresholdRepresentation, "Surface");
    vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility")
      .Set(visibility() ? 1 : 0);

    updateColorMap();
    applyActiveScalars();
    updateColorArray();

  } else {
    // --- Subsequent calls: update the existing pipeline ---

    auto* tp = vtkTrivialProducer::SafeDownCast(
      m_producer->GetClientSideObject());
    tp->SetOutput(m_pendingImage);

    vtkSMPropertyHelper(m_thresholdFilter, "LowerThreshold").Set(m_lower);
    vtkSMPropertyHelper(m_thresholdFilter, "UpperThreshold").Set(m_upper);
    m_thresholdFilter->UpdateVTKObjects();

    vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility")
      .Set(visibility() ? 1 : 0);

    applyActiveScalars();
    updateColorMap();
  }

  // Snapshot the spacing baked into the pipeline output. onMetadataChanged()
  // uses this as the reference to compute actor scale ratios.
  auto vol = volumeData();
  if (vol && vol->isValid()) {
    m_lastSpacing = vol->spacing();
    m_lastOrigin = vol->origin();
  }

  m_pendingImage = nullptr;

  // Force a render so the SM representation fully initialises its internal
  // geometry representation and mapper — without this the actor/mapper
  // lookups in onMetadataChanged / applyClippingPlanes silently bail out.
  vtkView->StillRender();

  // Apply current position/orientation/scale to the actor.
  onMetadataChanged();
  applyClippingPlanes();
  updatePanel();
  emit renderNeeded();
}

// --- Threshold Range ---

double ThresholdSink::lowerThreshold() const
{
  return m_lower;
}

double ThresholdSink::upperThreshold() const
{
  return m_upper;
}

void ThresholdSink::setThresholdRange(double lower, double upper)
{
  bool changed = lower != m_lower || upper != m_upper;
  m_lower = lower;
  m_upper = upper;
  m_rangeSet = true;
  if (m_thresholdFilter) {
    vtkSMPropertyHelper(m_thresholdFilter, "LowerThreshold").Set(m_lower);
    vtkSMPropertyHelper(m_thresholdFilter, "UpperThreshold").Set(m_upper);
    m_thresholdFilter->UpdateVTKObjects();
  }
  if (m_controllers) {
    QSignalBlocker blocker(m_controllers);
    m_controllers->setMinimum(lower);
    m_controllers->setMaximum(upper);
  }
  if (changed) {
    emit thresholdRangeChanged(m_lower, m_upper);
  }
  emit renderNeeded();
}

// --- Opacity ---

double ThresholdSink::opacity() const
{
  if (m_thresholdRepresentation) {
    return vtkSMPropertyHelper(m_thresholdRepresentation, "Opacity")
      .GetAsDouble();
  }
  return 1.0;
}

void ThresholdSink::setOpacity(double value)
{
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "Opacity").Set(value);
    m_thresholdRepresentation->UpdateVTKObjects();
  }
  emit renderNeeded();
}

// --- Specular ---

double ThresholdSink::specular() const
{
  if (m_thresholdRepresentation) {
    return vtkSMPropertyHelper(m_thresholdRepresentation, "Specular")
      .GetAsDouble();
  }
  return 1.0;
}

void ThresholdSink::setSpecular(double value)
{
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "Specular").Set(value);
    m_thresholdRepresentation->UpdateVTKObjects();
  }
  emit renderNeeded();
}

// --- Representation ---

int ThresholdSink::representation() const
{
  if (m_thresholdRepresentation) {
    return vtkSMPropertyHelper(m_thresholdRepresentation, "Representation")
      .GetAsInt();
  }
  return 2; // VTK_SURFACE
}

void ThresholdSink::setRepresentation(int rep)
{
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "Representation").Set(rep);
    m_thresholdRepresentation->UpdateVTKObjects();
  }
  emit renderNeeded();
}

QString ThresholdSink::representationString() const
{
  if (m_thresholdRepresentation) {
    return QString::fromUtf8(
      vtkSMPropertyHelper(m_thresholdRepresentation, "Representation")
        .GetAsString());
  }
  return QStringLiteral("Surface");
}

void ThresholdSink::setRepresentationString(const QString& rep)
{
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "Representation")
      .Set(rep.toLatin1().data());
    m_thresholdRepresentation->UpdateVTKObjects();
  }
  emit renderNeeded();
}

// --- Scalar Range ---

void ThresholdSink::scalarRange(double range[2]) const
{
  range[0] = m_scalarRange[0];
  range[1] = m_scalarRange[1];
}

// --- Map Scalars ---

bool ThresholdSink::mapScalars() const
{
  return m_mapScalars;
}

void ThresholdSink::setMapScalars(bool map)
{
  m_mapScalars = map;
  if (m_thresholdRepresentation) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "MapScalars")
      .Set(map ? 1 : 0);
    m_thresholdRepresentation->UpdateVTKObjects();
  }
  emit renderNeeded();
}

// --- Color Map (copied from ModuleThreshold::updateColorMap) ---

void ThresholdSink::updateColorMap()
{
  if (!m_thresholdRepresentation) {
    return;
  }

  // By default, use the data source's color/opacity maps.
  vtkSMPropertyHelper(m_thresholdRepresentation, "LookupTable")
    .Set(colorMap());
  vtkSMPropertyHelper(m_thresholdRepresentation, "ScalarOpacityFunction")
    .Set(opacityMap());

  m_thresholdRepresentation->UpdateVTKObjects();
}

// --- Properties Widget ---

QWidget* ThresholdSink::createSinkPropertiesWidget(QWidget* parent)
{
  auto* widget = new QWidget(parent);
  auto* layout = new QVBoxLayout;
  widget->setLayout(layout);

  // --- Separate Color Map checkbox ---
  auto* separateCmapCheck = new QCheckBox("Separate Color Map", widget);
  {
    QSignalBlocker blocker(separateCmapCheck);
    separateCmapCheck->setChecked(useDetachedColorMap());
  }
  layout->addWidget(separateCmapCheck);
  connect(separateCmapCheck, &QCheckBox::toggled,
          [this](bool on) { setUseDetachedColorMap(on); });

  // Create, update and connect
  m_controllers = new ThresholdSinkWidget;
  layout->addWidget(m_controllers);

  updatePanel();

  connect(m_controllers, &ThresholdSinkWidget::colorMapDataToggled, this,
          [this](bool state) { setMapScalars(state); });
  connect(m_controllers, &ThresholdSinkWidget::minimumChanged, this,
          [this](double v) { setThresholdRange(v, m_upper); });
  connect(m_controllers, &ThresholdSinkWidget::maximumChanged, this,
          [this](double v) { setThresholdRange(m_lower, v); });
  connect(m_controllers, &ThresholdSinkWidget::representationChanged, this,
          [this](const QString& rep) { setRepresentationString(rep); });
  connect(m_controllers, &ThresholdSinkWidget::opacityChanged, this,
          [this](double v) { setOpacity(v); });
  connect(m_controllers, &ThresholdSinkWidget::specularChanged, this,
          [this](double v) { setSpecular(v); });
  connect(m_controllers, &ThresholdSinkWidget::thresholdByArrayValueChanged,
          this, [this](int i) { setActiveScalars(i); });
  connect(m_controllers, &ThresholdSinkWidget::colorByArrayToggled, this,
          [this](bool state) { setColorByArray(state); });
  connect(m_controllers, &ThresholdSinkWidget::colorByArrayNameChanged, this,
          [this](const QString& name) { setColorByArrayName(name); });

  return widget;
}

void ThresholdSink::updatePanel()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);

  m_controllers->setThresholdRange(m_scalarRange);
  updateScalarArrayOptions();

  m_controllers->setColorMapData(mapScalars());
  m_controllers->setMinimum(lowerThreshold());
  m_controllers->setMaximum(upperThreshold());
  m_controllers->setRepresentation(representationString());
  m_controllers->setOpacity(opacity());
  m_controllers->setSpecular(specular());
  m_controllers->setThresholdByArrayValue(activeScalars());
  m_controllers->setColorByArray(colorByArray());
  m_controllers->setColorByArrayName(colorByArrayName());
}

// --- Clipping ---

void ThresholdSink::addClippingPlane(vtkPlane* plane)
{
  if (!plane) {
    return;
  }
  m_clippingPlanes.insert(plane);
  applyClippingPlanes();
  emit renderNeeded();
}

void ThresholdSink::removeClippingPlane(vtkPlane* plane)
{
  if (!plane) {
    return;
  }
  m_clippingPlanes.remove(plane);
  applyClippingPlanes();
  emit renderNeeded();
}

void ThresholdSink::applyClippingPlanes()
{
  if (!m_thresholdRepresentation) {
    return;
  }
  // The SM representation wraps a vtkPVCompositeRepresentation whose
  // active sub-representation is a vtkGeometryRepresentation.
  auto* clientObj = m_thresholdRepresentation->GetClientSideObject();
  vtkGeometryRepresentation* geoRep = nullptr;
  auto* compositeRep = vtkCompositeRepresentation::SafeDownCast(clientObj);
  if (compositeRep) {
    geoRep = vtkGeometryRepresentation::SafeDownCast(
      compositeRep->GetActiveRepresentation());
  } else {
    geoRep = vtkGeometryRepresentation::SafeDownCast(clientObj);
  }
  if (!geoRep || !geoRep->GetActor()) {
    return;
  }
  auto* actor = geoRep->GetActor();

  // Apply to both full-res and LOD mappers.
  vtkMapper* mappers[2] = { actor->GetMapper(), actor->GetLODMapper() };
  for (auto* mapper : mappers) {
    if (!mapper) {
      continue;
    }
    mapper->RemoveAllClippingPlanes();
    for (auto* plane : m_clippingPlanes) {
      mapper->AddClippingPlane(plane);
    }
  }
}

// --- Active Scalars ---

int ThresholdSink::activeScalars() const
{
  return m_activeScalars;
}

void ThresholdSink::setActiveScalars(int idx)
{
  m_activeScalars = idx;
  applyActiveScalars();
  if (!m_colorByArray) {
    updateColorArray();
  }
  emit renderNeeded();
}

void ThresholdSink::applyActiveScalars()
{
  if (!m_thresholdFilter) {
    return;
  }

  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  resolvePendingActiveScalar(m_activeScalars);

  auto* pointData = vol->imageData()->GetPointData();
  vtkDataArray* selected = nullptr;
  int idx = m_activeScalars;
  if (idx >= 0 && idx < pointData->GetNumberOfArrays()) {
    selected = pointData->GetArray(idx);
  }
  if (!selected) {
    // Default or saved index doesn't fit the current data — fall back
    // to active. m_activeScalars left alone.
    selected = pointData->GetScalars();
  }
  if (selected && selected->GetName()) {
    vtkSMPropertyHelper(m_thresholdFilter, "SelectInputScalars")
      .SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS,
                              selected->GetName());
    m_thresholdFilter->UpdateVTKObjects();
  }
}

// --- Color By Array ---

bool ThresholdSink::colorByArray() const
{
  return m_colorByArray;
}

void ThresholdSink::setColorByArray(bool state)
{
  m_colorByArray = state;
  updateColorArray();
  emit renderNeeded();
}

QString ThresholdSink::colorByArrayName() const
{
  return m_colorByArrayName;
}

void ThresholdSink::setColorByArrayName(const QString& name)
{
  m_colorByArrayName = name;
  updateColorArray();
  emit renderNeeded();
}

void ThresholdSink::updateColorArray()
{
  if (!m_thresholdRepresentation) {
    return;
  }

  QString name;
  if (m_colorByArray) {
    name = m_colorByArrayName;
  } else {
    // Use the threshold-by array (resolving the active scalars override)
    auto vol = volumeData();
    if (vol && vol->isValid()) {
      auto* pointData = vol->imageData()->GetPointData();
      int idx = m_activeScalars;
      if (idx < 0) {
        auto* scalars = pointData->GetScalars();
        if (scalars && scalars->GetName()) {
          name = QString::fromUtf8(scalars->GetName());
        }
      } else if (idx < pointData->GetNumberOfArrays()) {
        auto* array = pointData->GetArray(idx);
        if (array && array->GetName()) {
          name = QString::fromUtf8(array->GetName());
        }
      }
    }
  }

  if (!name.isEmpty()) {
    vtkSMPropertyHelper(m_thresholdRepresentation, "ColorArrayName")
      .SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS,
                              name.toUtf8().constData());
    m_thresholdRepresentation->UpdateVTKObjects();
  }
}

void ThresholdSink::updateScalarArrayOptions()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);
  m_controllers->setThresholdByArrayOptions(m_scalarArrayNames, m_activeScalars);
  m_controllers->setColorByArrayOptions(m_scalarArrayNames);

  if (!m_scalarArrayNames.contains(m_colorByArrayName) &&
      !m_scalarArrayNames.isEmpty()) {
    m_colorByArrayName = m_scalarArrayNames.first();
  }
  m_controllers->setColorByArrayName(m_colorByArrayName);
}

// --- Serialization ---

QJsonObject ThresholdSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["scalarArray"] = m_activeScalars;
  json["colorByArray"] = m_colorByArray;
  json["colorByArrayName"] = m_colorByArrayName;
  json["minimum"] = m_lower;
  json["maximum"] = m_upper;
  json["opacity"] = opacity();
  json["specular"] = specular();
  json["representation"] = representationString();
  json["mapScalars"] = m_mapScalars;
  return json;
}

bool ThresholdSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }
  if (json.contains("scalarArray")) {
    m_activeScalars = json["scalarArray"].toInt(-1);
  }
  if (json.contains("colorByArray")) {
    m_colorByArray = json["colorByArray"].toBool();
  }
  if (json.contains("colorByArrayName")) {
    m_colorByArrayName = json["colorByArrayName"].toString();
  }
  if (json.contains("minimum") && json.contains("maximum")) {
    setThresholdRange(json["minimum"].toDouble(),
                      json["maximum"].toDouble());
  }
  if (json.contains("opacity")) {
    setOpacity(json["opacity"].toDouble());
  }
  if (json.contains("specular")) {
    setSpecular(json["specular"].toDouble());
  }
  if (json.contains("representation")) {
    setRepresentationString(json["representation"].toString());
  }
  if (json.contains("mapScalars")) {
    setMapScalars(json["mapScalars"].toBool());
  }

  updatePanel();

  return true;
}

void ThresholdSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol || !m_thresholdRepresentation) {
    return;
  }

  // Go straight to the client-side VTK actor, bypassing the SM layer entirely.
  // This avoids the expensive SM pipeline update for interactive transforms.
  auto* clientObj = m_thresholdRepresentation->GetClientSideObject();
  auto* compositeRep = vtkCompositeRepresentation::SafeDownCast(clientObj);
  vtkGeometryRepresentation* geoRep = nullptr;
  if (compositeRep) {
    geoRep = vtkGeometryRepresentation::SafeDownCast(
      compositeRep->GetActiveRepresentation());
  } else {
    geoRep = vtkGeometryRepresentation::SafeDownCast(clientObj);
  }
  if (!geoRep || !geoRep->GetActor()) {
    return;
  }
  auto* actor = geoRep->GetActor();

  auto orient = vol->displayOrientation();
  actor->SetOrientation(orient.data());

  // The threshold filter output has geometry baked at the origin/spacing that
  // was current when the pipeline last executed. Apply the origin delta as
  // actor position and the spacing ratio as actor scale so the visual matches
  // without re-executing the expensive SM pipeline.
  if (vol->isValid()) {
    auto orig = vol->origin();
    auto sp = vol->spacing();
    auto displayPos = vol->displayPosition();
    double pos[3] = { 0, 0, 0 };
    double scale[3] = { 1.0, 1.0, 1.0 };
    for (int i = 0; i < 3; ++i) {
      pos[i] = displayPos[i] + orig[i] - m_lastOrigin[i];
      if (m_lastSpacing[i] != 0.0) {
        scale[i] = sp[i] / m_lastSpacing[i];
      }
    }
    actor->SetPosition(pos);
    actor->SetScale(scale);
  }

  applyActiveScalars();
  updateColorArray();
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
