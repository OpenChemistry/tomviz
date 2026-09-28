/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ContourSink.h"
#include "ContourSinkWidget.h"

#include "data/VolumeData.h"
#include "vtkActiveScalarsProducer.h"

#include <QCheckBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

#include <vtkActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkDataArray.h>
#include <vtkDataSetMapper.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkProbeFilter.h>
#include <vtkProperty.h>
#include <vtkPVRenderView.h>
#include <vtkSMProxy.h>

namespace tomviz {
namespace pipeline {

ContourSink::ContourSink(QObject* parent) : LegacyModuleSink(parent)
{
  addInput("volume", PortType::ImageData);
  setLabel("Contour");

  m_property->SetAmbient(0.0);
  m_property->SetDiffuse(1.0);
  m_property->SetSpecular(1.0);
  m_property->SetSpecularPower(100.0);
  m_property->SetRepresentationToSurface();

  m_flyingEdges->SetInputConnection(m_contourArrayProducer->GetOutputPort());
  m_flyingEdges->ComputeScalarsOn();
  m_mapper->SetInputConnection(m_flyingEdges->GetOutputPort());
  m_mapper->SetScalarModeToUsePointFieldData();
  m_mapper->SetColorModeToMapScalars();
  m_actor->SetMapper(m_mapper);
  m_actor->SetVisibility(0);
  m_actor->SetProperty(m_property);
}

ContourSink::~ContourSink()
{
  finalize();
}

QIcon ContourSink::icon() const
{
  return QIcon(QStringLiteral(":/pqWidgets/Icons/pqIsosurface.svg"));
}

void ContourSink::setVisibility(bool visible)
{
  m_actor->SetVisibility(visible ? 1 : 0);
  LegacyModuleSink::setVisibility(visible);
}

bool ContourSink::isColorMapNeeded() const
{
  return true;
}

bool ContourSink::initialize(vtkSMViewProxy* view)
{
  if (!LegacyModuleSink::initialize(view)) {
    return false;
  }

  renderView()->AddPropToRenderer(m_actor);
  return true;
}

bool ContourSink::finalize()
{
  if (renderView()) {
    renderView()->RemovePropFromRenderer(m_actor);
  }
  return LegacyModuleSink::finalize();
}

void ContourSink::clearVisualization()
{
  m_actor->SetVisibility(0);
}

bool ContourSink::consume(const QMap<QString, PortData>& inputs)
{
  if (!validateInput(inputs, "volume")) {
    return false;
  }

  auto volume = inputs["volume"].value<VolumeDataPtr>();
  if (!volume || !volume->isValid()) {
    return false;
  }

  m_contourArrayProducer->SetOutput(volume->imageData());
  applyActiveScalars();

  // Cache scalar range
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

  // Auto-set iso value to 2/3 of range (matching old ModuleContour default)
  if (!m_isoValueSet) {
    m_isoValue = range[0] + (range[1] - range[0]) * (2.0 / 3.0);
  }

  m_flyingEdges->SetValue(0, m_isoValue);
  m_actor->SetVisibility(visibility() ? 1 : 0);

  // Snapshot spacing so onMetadataChanged can compute actor scale ratios.
  // Also disconnect the mapper from the pipeline so that spacing changes
  // on the shared vtkImageData don't trigger an expensive FlyingEdges
  // re-execution on the next render.  We feed the mapper a static copy
  // of the contour output and reconnect on the next consume().
  m_baseSpacing = volume->spacing();
  m_baseOrigin = volume->origin();
  updateMapperInput();

  // consume() runs on the GUI thread (consumeOnGuiThread()), so refresh
  // the panel directly.
  updatePanel();

  onMetadataChanged();
  emit renderNeeded();
  return true;
}

// --- Iso Value ---

double ContourSink::isoValue() const
{
  return m_isoValue;
}

void ContourSink::setIsoValue(double value)
{
  m_isoValue = value;
  m_isoValueSet = true;
  m_flyingEdges->SetValue(0, value);
  updateMapperInput();
  if (m_controllers) {
    QSignalBlocker blocker(m_controllers);
    m_controllers->setIso(value);
  }
  emit renderNeeded();
}

// --- Opacity ---

double ContourSink::opacity() const
{
  return m_property->GetOpacity();
}

void ContourSink::setOpacity(double value)
{
  m_property->SetOpacity(value);
  emit renderNeeded();
}

// --- Lighting ---

double ContourSink::ambient() const
{
  return m_property->GetAmbient();
}

void ContourSink::setAmbient(double value)
{
  m_property->SetAmbient(value);
  emit renderNeeded();
}

double ContourSink::diffuse() const
{
  return m_property->GetDiffuse();
}

void ContourSink::setDiffuse(double value)
{
  m_property->SetDiffuse(value);
  emit renderNeeded();
}

double ContourSink::specular() const
{
  return m_property->GetSpecular();
}

void ContourSink::setSpecular(double value)
{
  m_property->SetSpecular(value);
  emit renderNeeded();
}

double ContourSink::specularPower() const
{
  return m_property->GetSpecularPower();
}

void ContourSink::setSpecularPower(double value)
{
  m_property->SetSpecularPower(value);
  emit renderNeeded();
}

// --- Representation ---

int ContourSink::representation() const
{
  return m_property->GetRepresentation();
}

void ContourSink::setRepresentation(int rep)
{
  m_property->SetRepresentation(rep);
  emit renderNeeded();
}

QString ContourSink::representationString() const
{
  return QString::fromUtf8(m_property->GetRepresentationAsString());
}

void ContourSink::setRepresentationString(const QString& rep)
{
  if (rep == "Surface")
    m_property->SetRepresentationToSurface();
  else if (rep == "Points")
    m_property->SetRepresentationToPoints();
  else if (rep == "Wireframe")
    m_property->SetRepresentationToWireframe();

  emit renderNeeded();
}

// --- Color ---

void ContourSink::color(double rgb[3]) const
{
  m_property->GetDiffuseColor(rgb);
}

void ContourSink::setColor(double r, double g, double b)
{
  m_property->SetDiffuseColor(r, g, b);
  emit renderNeeded();
}

QColor ContourSink::qcolor() const
{
  double rgb[3];
  m_property->GetDiffuseColor(rgb);
  return QColor(static_cast<int>(rgb[0] * 255.0 + 0.5),
                static_cast<int>(rgb[1] * 255.0 + 0.5),
                static_cast<int>(rgb[2] * 255.0 + 0.5));
}

void ContourSink::setQColor(const QColor& c)
{
  double rgb[3] = { c.red() / 255.0, c.green() / 255.0, c.blue() / 255.0 };
  m_property->SetDiffuseColor(rgb);
  emit renderNeeded();
}

// --- Solid Color ---

bool ContourSink::useSolidColor() const
{
  return m_useSolidColor;
}

void ContourSink::setUseSolidColor(bool state)
{
  m_useSolidColor = state;
  updateColorMap();
  emit renderNeeded();
}

// --- Color By Array ---

bool ContourSink::colorByArray() const
{
  return m_colorByArray;
}

void ContourSink::setColorByArray(bool state)
{
  m_colorByArray = state;
  if (state) {
    updateColorArrayProducer();
    m_probeFilter->SetInputConnection(m_flyingEdges->GetOutputPort());
    m_probeFilter->SetSourceConnection(m_colorArrayProducer->GetOutputPort());
  } else {
    m_probeFilter->RemoveAllInputs();
  }
  updateMapperInput();
  updateColorMap();
  emit renderNeeded();
}

QString ContourSink::colorByArrayName() const
{
  return m_colorByArrayName;
}

void ContourSink::setColorByArrayName(const QString& name)
{
  m_colorByArrayName = name;
  if (m_colorByArray) {
    updateColorArrayProducer();
  }
  updateColorMap();
  emit renderNeeded();
}

void ContourSink::updateColorArrayProducer()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    m_colorArrayProducer->SetOutput(nullptr);
    return;
  }
  m_colorArrayProducer->SetOutput(vol->imageData());
  auto name = m_colorByArrayName.toStdString();
  m_colorArrayProducer->SetActiveScalars(name.c_str());
}

// Feed the mapper a static copy of the surface (see consume()), probed
// for the color-by array when one is chosen
void ContourSink::updateMapperInput()
{
  if (!m_contourArrayProducer->GetOutputDataObject(0)) {
    return; // no data yet (a state restore sets properties first)
  }
  m_flyingEdges->Update();
  if (m_colorByArray) {
    m_probeFilter->Update();
    m_mapper->SetInputDataObject(m_probeFilter->GetOutput());
  } else {
    m_mapper->SetInputDataObject(m_flyingEdges->GetOutput());
  }
}

// --- Active Scalars ---

int ContourSink::activeScalars() const
{
  return m_activeScalars;
}

void ContourSink::setActiveScalars(int idx)
{
  m_activeScalars = idx;
  applyActiveScalars();
  updateMapperInput();
  updateColorMap();
  emit renderNeeded();
}

// --- Scalar Range ---

void ContourSink::scalarRange(double range[2]) const
{
  range[0] = m_scalarRange[0];
  range[1] = m_scalarRange[1];
}

// --- Map Scalars ---

bool ContourSink::mapScalars() const
{
  return m_mapScalars;
}

void ContourSink::setMapScalars(bool map)
{
  m_mapScalars = map;
  if (map) {
    m_mapper->SetColorModeToMapScalars();
  } else {
    m_mapper->SetColorModeToDirectScalars();
  }
  emit renderNeeded();
}

bool ContourSink::applyActiveScalars()
{
  auto vol = volumeData();
  if (!vol || !vol->isValid()) {
    return false;
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
  QString arrayName;
  if (selected && selected->GetName()) {
    arrayName = QString::fromUtf8(selected->GetName());
  }

  bool changed = (!arrayName.isEmpty() && arrayName != m_lastContourArrayName);
  if (!arrayName.isEmpty()) {
    m_contourArrayProducer->SetActiveScalars(arrayName.toUtf8().constData());
    m_lastContourArrayName = arrayName;
  }
  return changed;
}

// --- Color Map ---

void ContourSink::updateColorMap()
{
  auto* cmap = colorMap();
  if (cmap) {
    auto* ctf = vtkColorTransferFunction::SafeDownCast(
      cmap->GetClientSideObject());
    if (ctf) {
      m_mapper->SetLookupTable(ctf);
    }
  }

  updateColorArray();
  emit renderNeeded();
}

void ContourSink::updateColorArray()
{
  std::string name;
  if (m_colorByArray) {
    name = m_colorByArrayName.toStdString();
  } else if (m_useSolidColor) {
    // Empty color array → use actor's diffuse color
    name = "";
  } else {
    // Use the contour-by array (resolving the active scalars override)
    auto vol = volumeData();
    if (vol && vol->isValid()) {
      auto* pointData = vol->imageData()->GetPointData();
      int idx = m_activeScalars;
      if (idx < 0) {
        auto* scalars = pointData->GetScalars();
        if (scalars && scalars->GetName()) {
          name = scalars->GetName();
        }
      } else if (idx < pointData->GetNumberOfArrays()) {
        auto* array = pointData->GetArray(idx);
        if (array && array->GetName()) {
          name = array->GetName();
        }
      }
    }
  }

  m_mapper->SelectColorArray(name.c_str());
}

// --- Properties Widget ---

QWidget* ContourSink::createSinkPropertiesWidget(QWidget* parent)
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
  m_controllers = new ContourSinkWidget;
  layout->addWidget(m_controllers);

  updatePanel();

  connect(m_controllers, &ContourSinkWidget::colorMapDataToggled, this,
          [this](bool state) { setMapScalars(state); });
  connect(m_controllers, &ContourSinkWidget::ambientChanged, this,
          [this](double v) { setAmbient(v); });
  connect(m_controllers, &ContourSinkWidget::diffuseChanged, this,
          [this](double v) { setDiffuse(v); });
  connect(m_controllers, &ContourSinkWidget::specularChanged, this,
          [this](double v) { setSpecular(v); });
  connect(m_controllers, &ContourSinkWidget::specularPowerChanged, this,
          [this](double v) { setSpecularPower(v); });
  connect(m_controllers, &ContourSinkWidget::isoChanged, this,
          [this](double v) { setIsoValue(v); });
  connect(m_controllers, &ContourSinkWidget::representationChanged, this,
          [this](const QString& rep) { setRepresentationString(rep); });
  connect(m_controllers, &ContourSinkWidget::opacityChanged, this,
          [this](double v) { setOpacity(v); });
  connect(m_controllers, &ContourSinkWidget::colorChanged, this,
          [this](const QColor& c) { setQColor(c); });
  connect(m_controllers, &ContourSinkWidget::useSolidColorToggled, this,
          [this](bool state) { setUseSolidColor(state); });
  connect(m_controllers, &ContourSinkWidget::contourByArrayValueChanged, this,
          [this](int i) {
            setActiveScalars(i);
            // TODO: update contour array producer when multi-array
            // support is added
          });
  connect(m_controllers, &ContourSinkWidget::colorByArrayToggled, this,
          [this](bool state) { setColorByArray(state); });
  connect(m_controllers, &ContourSinkWidget::colorByArrayNameChanged, this,
          [this](const QString& name) { setColorByArrayName(name); });

  return widget;
}

void ContourSink::updatePanel()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);

  m_controllers->setIsoRange(m_scalarRange);
  updateScalarArrayOptions();

  m_controllers->setColorMapData(mapScalars());
  m_controllers->setAmbient(ambient());
  m_controllers->setDiffuse(diffuse());
  m_controllers->setSpecular(specular());
  m_controllers->setSpecularPower(specularPower());
  m_controllers->setIso(isoValue());
  m_controllers->setRepresentation(representationString());
  m_controllers->setOpacity(opacity());
  m_controllers->setColor(qcolor());
  m_controllers->setUseSolidColor(useSolidColor());
  m_controllers->setContourByArrayValue(activeScalars());
  m_controllers->setColorByArray(colorByArray());
  m_controllers->setColorByArrayName(colorByArrayName());
}

void ContourSink::updateScalarArrayOptions()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);
  m_controllers->setContourByArrayOptions(m_scalarArrayNames, m_activeScalars);
  m_controllers->setColorByArrayOptions(m_scalarArrayNames);

  if (!m_scalarArrayNames.contains(m_colorByArrayName) &&
      !m_scalarArrayNames.isEmpty()) {
    m_colorByArrayName = m_scalarArrayNames.first();
  }
  m_controllers->setColorByArrayName(m_colorByArrayName);
}

// --- Clipping ---

void ContourSink::addClippingPlane(vtkPlane* plane)
{
  if (plane) {
    m_mapper->AddClippingPlane(plane);
    emit renderNeeded();
  }
}

void ContourSink::removeClippingPlane(vtkPlane* plane)
{
  if (plane) {
    m_mapper->RemoveClippingPlane(plane);
    emit renderNeeded();
  }
}

// --- Serialization ---

QJsonObject ContourSink::serialize() const
{
  auto json = LegacyModuleSink::serialize();
  json["contourValue"] = m_isoValue;
  json["opacity"] = m_property->GetOpacity();
  json["ambient"] = m_property->GetAmbient();
  json["diffuse"] = m_property->GetDiffuse();
  json["specular"] = m_property->GetSpecular();
  json["specularPower"] = m_property->GetSpecularPower();
  json["representation"] = representationString();
  json["mapScalars"] = m_mapScalars;
  json["useSolidColor"] = m_useSolidColor;
  json["colorByArray"] = m_colorByArray;
  json["colorByArrayName"] = m_colorByArrayName;
  json["activeScalars"] = activeScalarsToName(m_activeScalars);

  auto c = qcolor();
  json["color"] = c.name();

  return json;
}

bool ContourSink::deserialize(const QJsonObject& json)
{
  if (!LegacyModuleSink::deserialize(json)) {
    return false;
  }

  if (json.contains("useSolidColor")) {
    setUseSolidColor(json["useSolidColor"].toBool());
  }
  if (json.contains("color")) {
    setQColor(QColor(json["color"].toString()));
  }
  if (json.contains("ambient")) {
    setAmbient(json["ambient"].toDouble());
  }
  if (json.contains("diffuse")) {
    setDiffuse(json["diffuse"].toDouble());
  }
  if (json.contains("specular")) {
    setSpecular(json["specular"].toDouble());
  }
  if (json.contains("specularPower")) {
    setSpecularPower(json["specularPower"].toDouble());
  }
  if (json.contains("representation")) {
    setRepresentationString(json["representation"].toString());
  }
  if (json.contains("opacity")) {
    setOpacity(json["opacity"].toDouble());
  }
  if (json.contains("mapScalars")) {
    setMapScalars(json["mapScalars"].toBool());
  }
  if (json.contains("colorByArray")) {
    setColorByArray(json["colorByArray"].toBool());
  }
  if (json.contains("colorByArrayName")) {
    setColorByArrayName(json["colorByArrayName"].toString());
  }
  if (json.contains("activeScalars")) {
    readActiveScalars(json, m_activeScalars);
  }

  // Some of the above operations modify the contour value.
  // Set this at the end.
  if (json.contains("contourValue")) {
    setIsoValue(json["contourValue"].toDouble());
  }

  updatePanel();

  return true;
}

void ContourSink::onMetadataChanged()
{
  auto vol = volumeData();
  if (!vol) {
    return;
  }
  auto orient = vol->displayOrientation();
  m_actor->SetOrientation(orient.data());

  // The FlyingEdges output has geometry baked at the origin/spacing that was
  // current when consume() last ran. Apply the origin delta as actor position
  // and the spacing ratio as actor scale so the contour visually matches
  // without re-executing the expensive filter.
  if (vol->isValid()) {
    auto orig = vol->origin();
    auto sp = vol->spacing();
    auto displayPos = vol->displayPosition();
    double pos[3] = { 0, 0, 0 };
    double scale[3] = { 1.0, 1.0, 1.0 };
    for (int i = 0; i < 3; ++i) {
      pos[i] = displayPos[i] + orig[i] - m_baseOrigin[i];
      if (m_baseSpacing[i] != 0.0) {
        scale[i] = sp[i] / m_baseSpacing[i];
      }
    }
    m_actor->SetPosition(pos);
    m_actor->SetScale(scale);
  }

  if (applyActiveScalars()) {
    // The contour array changed — re-execute flying edges and reconnect
    // the mapper so the contour geometry reflects the new array.
    updateMapperInput();
  }
  updateColorArray();
  emit renderNeeded();
}

} // namespace pipeline
} // namespace tomviz
