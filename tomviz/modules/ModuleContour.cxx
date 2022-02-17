/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleContour.h"
#include "ModuleContourWidget.h"

#include "DataSource.h"

#include "vtkActiveScalarsProducer.h"
#include "vtkActor.h"
#include "vtkColorTransferFunction.h"
#include "vtkDataSetMapper.h"
#include "vtkFlyingEdges3D.h"
#include "vtkPVRenderView.h"
#include "vtkPointData.h"
#include "vtkProbeFilter.h"
#include "vtkProperty.h"
#include "vtkSMViewProxy.h"

#include <QJsonObject>
#include <QLayout>
#include <QStringList>

#include <string>

namespace tomviz {

class ModuleContour::Private
{
public:
  bool ColorByArray = false;
  bool UseSolidColor = false;
  QString ColorArrayName;
  vtkNew<vtkActiveScalarsProducer> ColorArrayProducer;
  vtkNew<vtkActiveScalarsProducer> ContourArrayProducer;
};

ModuleContour::ModuleContour(QObject* parentObject) : Module(parentObject)
{
  d = new Private;
}

ModuleContour::~ModuleContour()
{
  finalize();

  delete d;
  d = nullptr;
}

QIcon ModuleContour::icon() const
{
  return QIcon(":pqWidgets/Icons/pqIsosurface.svg");
}

bool ModuleContour::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  d->ColorArrayName = data->activeScalars();

  updateContourArrayProducer();

  m_flyingEdges->SetInputConnection(d->ContourArrayProducer->GetOutputPort());
  resetIsoValue();

  auto* displayPosition = data->displayPosition();
  m_actor->SetPosition(displayPosition[0], displayPosition[1],
                       displayPosition[2]);
  auto* displayOrientation = data->displayOrientation();
  m_actor->SetOrientation(displayOrientation[0], displayOrientation[1],
                          displayOrientation[2]);

  m_mapper->SetInputConnection(m_flyingEdges->GetOutputPort());
  m_mapper->SetScalarModeToUsePointFieldData();
  onColorMapDataToggled(true);
  updateColorMap();
  m_actor->SetMapper(m_mapper);

  m_actor->SetProperty(m_property);
  m_property->SetSpecularPower(100);

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_view->AddPropToRenderer(m_actor);
  m_view->Update();

  connect(data, &DataSource::dataChanged, this, &ModuleContour::onDataChanged);
  connect(data, &DataSource::dataPropertiesChanged, this,
          &ModuleContour::onDataPropertiesChanged);
  connect(data, &DataSource::activeScalarsChanged, this,
          &ModuleContour::onActiveScalarsChanged);

  emit renderNeeded();
  return true;
}

bool ModuleContour::finalize()
{
  if (m_view) {
    m_view->RemovePropFromRenderer(m_actor);
  }
  return true;
}

void ModuleContour::onDataChanged()
{
  // FIXME: Implementing the vtkActiveScalarsProducer as a producer breaks the
  // vtk pipeline, requiring manual updates to keep in sync like here.
  // It really should be implemented as a filter.
  onDataPropertiesChanged();
}

void ModuleContour::onDataPropertiesChanged()
{
  updateContourArrayProducer();
  updateContourByArrayOptions();
  updateColorArrayProducer();
  updateColorByArrayOptions();
}

void ModuleContour::onActiveScalarsChanged()
{
  // We only need to update if we are using the default option
  if (activeScalars() != Module::defaultScalarsIdx())
    return;

  updateContourArrayProducer();
  updateIsoRange();
  resetIsoValue();
  updateColorMap();
  emit renderNeeded();
}

void ModuleContour::updateContourArrayProducer()
{
  d->ContourArrayProducer->SetOutput(dataSource()->dataObject());
  auto name = contourByArrayName().toStdString();
  d->ContourArrayProducer->SetActiveScalars(name.c_str());
}

void ModuleContour::updateColorArrayProducer()
{
  if (!d->ColorByArray) {
    clearColorArrayProducer();
    return;
  }

  d->ColorArrayProducer->SetOutput(dataSource()->dataObject());
  auto activeName = colorByArrayName().toStdString();
  d->ColorArrayProducer->SetActiveScalars(activeName.c_str());
}

void ModuleContour::clearColorArrayProducer()
{
  d->ColorArrayProducer->SetOutput(nullptr);
}

void ModuleContour::updateColorMap()
{
  auto* lut =
    vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject());
  m_mapper->SetLookupTable(lut);

  updateColorArray();
}

void ModuleContour::updateColorArray()
{
  std::string name;
  if (colorByArray()) {
    name = colorByArrayName().toStdString();
  } else if (useSolidColor()) {
    // Have to set the color array to "" to use a solid color
    // Diffuse color should already be set on the property
    name = "";
  } else {
    name = contourByArrayName().toStdString();
  }

  m_mapper->SelectColorArray(name.c_str());
}

bool ModuleContour::setVisibility(bool val)
{
  m_actor->SetVisibility(val ? 1 : 0);
  Module::setVisibility(val);
  return true;
}

bool ModuleContour::visibility() const
{
  return m_actor->GetVisibility() == 1;
}

void ModuleContour::addToPanel(QWidget* panel)
{
  if (panel->layout())
    delete panel->layout();

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  m_controllers = new ModuleContourWidget;
  layout->addWidget(m_controllers);

  updatePanel();

  connect(m_controllers, &ModuleContourWidget::colorMapDataToggled, this,
          &ModuleContour::onColorMapDataToggled);
  connect(m_controllers, &ModuleContourWidget::ambientChanged, this,
          &ModuleContour::onAmbientChanged);
  connect(m_controllers, &ModuleContourWidget::diffuseChanged, this,
          &ModuleContour::onDiffuseChanged);
  connect(m_controllers, &ModuleContourWidget::specularChanged, this,
          &ModuleContour::onSpecularChanged);
  connect(m_controllers, &ModuleContourWidget::specularPowerChanged, this,
          &ModuleContour::onSpecularPowerChanged);
  connect(m_controllers, &ModuleContourWidget::isoChanged, this,
          &ModuleContour::onIsoChanged);
  connect(m_controllers, &ModuleContourWidget::representationChanged, this,
          &ModuleContour::onRepresentationChanged);
  connect(m_controllers, &ModuleContourWidget::opacityChanged, this,
          &ModuleContour::onOpacityChanged);
  connect(m_controllers, &ModuleContourWidget::colorChanged, this,
          &ModuleContour::onColorChanged);
  connect(m_controllers, &ModuleContourWidget::useSolidColorToggled, this,
          &ModuleContour::onUseSolidColorToggled);
  connect(m_controllers, &ModuleContourWidget::contourByArrayValueChanged, this,
          &ModuleContour::onContourByArrayValueChanged);
  connect(m_controllers, &ModuleContourWidget::colorByArrayToggled, this,
          &ModuleContour::onColorByArrayToggled);
  connect(m_controllers, &ModuleContourWidget::colorByArrayNameChanged, this,
          &ModuleContour::onColorByArrayNameChanged);
}

void ModuleContour::updatePanel()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);

  updateIsoRange();
  updateContourByArrayOptions();
  updateColorByArrayOptions();

  m_controllers->setColorMapData(colorMapData());
  m_controllers->setAmbient(ambient());
  m_controllers->setDiffuse(diffuse());
  m_controllers->setSpecular(specular());
  m_controllers->setSpecularPower(specularPower());
  m_controllers->setIso(iso());
  m_controllers->setRepresentation(representation());
  m_controllers->setOpacity(opacity());
  m_controllers->setColor(color());
  m_controllers->setUseSolidColor(useSolidColor());
  m_controllers->setContourByArrayValue(activeScalars());
  m_controllers->setColorByArray(colorByArray());
  m_controllers->setColorByArrayName(colorByArrayName());
}

namespace {

void getRange(vtkAlgorithm* alg, double range[2])
{
  for (int i = 0; i < 2; ++i)
    range[i] = 0.0;

  if (!alg)
    return;

  auto* data = vtkDataSet::SafeDownCast(alg->GetOutputDataObject(0));
  if (!data)
    return;

  auto* arrayPtr = data->GetPointData()->GetScalars();
  if (!arrayPtr)
    return;

  arrayPtr->GetFiniteRange(range, -1);
}

} // namespace

void ModuleContour::updateIsoRange()
{
  if (!m_controllers)
    return;

  double range[2];
  getRange(d->ContourArrayProducer, range);
  m_controllers->setIsoRange(range);
}

void ModuleContour::updateContourByArrayOptions()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);
  m_controllers->setContourByArrayOptions(dataSource(), this);
}

void ModuleContour::updateColorByArrayOptions()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);
  QStringList options = dataSource()->listScalars();
  m_controllers->setColorByArrayOptions(options);

  if (!options.contains(colorByArrayName()))
    onColorByArrayNameChanged(dataSource()->activeScalars());

  m_controllers->setColorByArrayName(colorByArrayName());
}

void ModuleContour::resetIsoValue()
{
  double range[2];
  getRange(d->ContourArrayProducer, range);

  // Use 2/3 of the range by default
  double val = (range[1] - range[0]) * 2 / 3 + range[0];
  setIsoValue(val);
}

void ModuleContour::isoRange(double range[2])
{
  getRange(d->ContourArrayProducer, range);
}

QJsonObject ModuleContour::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["useSolidColor"] = useSolidColor();
  props["color"] = color().name();
  props["contourValue"] = iso();

  QJsonObject lighting;
  lighting["ambient"] = ambient();
  lighting["diffuse"] = diffuse();
  lighting["specular"] = specular();
  lighting["specularPower"] = specularPower();
  props["lighting"] = lighting;

  props["representation"] = representation();
  props["opacity"] = opacity();
  props["mapScalars"] = colorMapData();
  props["colorByArray"] = colorByArray();
  props["colorByArrayName"] = colorByArrayName();

  json["properties"] = props;

  return json;
}

bool ModuleContour::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }

  if (!json["properties"].isObject())
    return false;

  auto props = json["properties"].toObject();
  onUseSolidColorToggled(props["useSolidColor"].toBool());
  onColorChanged(QColor(props["color"].toString()));

  QJsonObject lighting = props["lighting"].toObject();
  onAmbientChanged(lighting["ambient"].toDouble());
  onDiffuseChanged(lighting["diffuse"].toDouble());
  onSpecularChanged(lighting["specular"].toDouble());
  onSpecularPowerChanged(lighting["specularPower"].toDouble());

  onRepresentationChanged(props["representation"].toString());
  onOpacityChanged(props["opacity"].toDouble());
  onColorMapDataToggled(props["mapScalars"].toBool());
  onColorByArrayToggled(props["colorByArray"].toBool());
  onColorByArrayNameChanged(props["colorByArrayName"].toString());

  // Some of the above operations modify the contour value.
  // Set this at the end.
  onIsoChanged(props["contourValue"].toDouble());

  updatePanel();

  return true;
}

void ModuleContour::dataSourceMoved(double newX, double newY, double newZ)
{
  m_actor->SetPosition(newX, newY, newZ);
}

void ModuleContour::dataSourceRotated(double newX, double newY, double newZ)
{
  m_actor->SetOrientation(newX, newY, newZ);
}

vtkDataObject* ModuleContour::dataToExport()
{
  return m_flyingEdges->GetOutputDataObject(0);
}

void ModuleContour::setIsoValue(double value)
{
  onIsoChanged(value);
  if (m_controllers) {
    QSignalBlocker blocker(m_controllers);
    m_controllers->setIso(value);
  }
}

bool ModuleContour::colorMapData() const
{
  return m_mapper->GetColorMode() == VTK_COLOR_MODE_MAP_SCALARS;
}

double ModuleContour::ambient() const
{
  return m_property->GetAmbient();
}

double ModuleContour::diffuse() const
{
  return m_property->GetDiffuse();
}

double ModuleContour::specular() const
{
  return m_property->GetSpecular();
}

double ModuleContour::iso() const
{
  return m_flyingEdges->GetValue(0);
}

double ModuleContour::specularPower() const
{
  return m_property->GetSpecularPower();
}

QString ModuleContour::representation() const
{
  return m_property->GetRepresentationAsString();
}

double ModuleContour::opacity() const
{
  return m_property->GetOpacity();
}

QColor ModuleContour::color() const
{
  double rgb[3];
  m_property->GetDiffuseColor(rgb);
  return QColor(static_cast<int>(rgb[0] * 255.0 + 0.5),
                static_cast<int>(rgb[1] * 255.0 + 0.5),
                static_cast<int>(rgb[2] * 255.0 + 0.5));
}

bool ModuleContour::useSolidColor() const
{
  return d->UseSolidColor;
}

QString ModuleContour::contourByArrayName() const
{
  if (activeScalars() == Module::defaultScalarsIdx())
    return dataSource()->activeScalars();

  return dataSource()->scalarsName(activeScalars());
}

bool ModuleContour::colorByArray() const
{
  return d->ColorByArray;
}

QString ModuleContour::colorByArrayName() const
{
  return d->ColorArrayName;
}

void ModuleContour::onColorMapDataToggled(const bool state)
{
  int mode = state ? VTK_COLOR_MODE_MAP_SCALARS : VTK_COLOR_MODE_DIRECT_SCALARS;
  m_mapper->SetColorMode(mode);
  emit renderNeeded();
}

void ModuleContour::onAmbientChanged(const double value)
{
  m_property->SetAmbient(value);
  emit renderNeeded();
}

void ModuleContour::onDiffuseChanged(const double value)
{
  m_property->SetDiffuse(value);
  emit renderNeeded();
}

void ModuleContour::onSpecularChanged(const double value)
{
  m_property->SetSpecular(value);
  emit renderNeeded();
}

void ModuleContour::onSpecularPowerChanged(const double value)
{
  m_property->SetSpecularPower(value);
  emit renderNeeded();
}

void ModuleContour::onIsoChanged(const double value)
{
  m_flyingEdges->SetValue(0, value);
  emit renderNeeded();
}

void ModuleContour::onRepresentationChanged(const QString& representation)
{
  if (representation == "Surface")
    m_property->SetRepresentationToSurface();
  else if (representation == "Points")
    m_property->SetRepresentationToPoints();
  else if (representation == "Wireframe")
    m_property->SetRepresentationToWireframe();

  emit renderNeeded();
}

void ModuleContour::onOpacityChanged(const double value)
{
  m_property->SetOpacity(value);
  emit renderNeeded();
}

void ModuleContour::onColorChanged(const QColor& c)
{
  double rgb[3] = { c.red() / 255.0, c.green() / 255.0, c.blue() / 255.0 };
  m_property->SetDiffuseColor(rgb);
  emit renderNeeded();
}

void ModuleContour::onUseSolidColorToggled(const bool state)
{
  d->UseSolidColor = state;
  updateColorMap();
  emit renderNeeded();
}

void ModuleContour::onContourByArrayValueChanged(int i)
{
  setActiveScalars(i);
  updateContourArrayProducer();
  updateIsoRange();
  resetIsoValue();
  updateColorMap();
  emit renderNeeded();
}

void ModuleContour::onColorByArrayToggled(const bool state)
{
  d->ColorByArray = state;
  updateColorArrayProducer();
  if (state) {
    m_probeFilter->SetInputConnection(m_flyingEdges->GetOutputPort());
    m_probeFilter->SetSourceConnection(d->ColorArrayProducer->GetOutputPort());
    m_mapper->SetInputConnection(m_probeFilter->GetOutputPort());
  } else {
    m_probeFilter->RemoveAllInputs();
    m_mapper->SetInputConnection(m_flyingEdges->GetOutputPort());
  }
  updateColorMap();
  emit renderNeeded();
}

void ModuleContour::onColorByArrayNameChanged(const QString& name)
{
  d->ColorArrayName = name;
  updateColorArrayProducer();
  updateColorMap();
  emit renderNeeded();
}

bool ModuleContour::updateClippingPlane(vtkPlane* plane, bool newFilter)
{
  if (m_mapper->GetNumberOfClippingPlanes()) {
    m_mapper->RemoveClippingPlane(plane);
  }
  if (!newFilter) {
    m_mapper->AddClippingPlane(plane);
  }

  emit renderNeeded();

  return true;
}

} // end of namespace tomviz
