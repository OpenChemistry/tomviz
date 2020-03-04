/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleContour.h"
#include "ModuleContourWidget.h"

#include "DataSource.h"

#include "vtkActor.h"
#include "vtkColorTransferFunction.h"
#include "vtkPolyDataMapper.h"
#include "vtkFlyingEdges3D.h"
#include "vtkPVRenderView.h"
#include "vtkProperty.h"
#include "vtkSMViewProxy.h"
#include "vtkTrivialProducer.h"

#include <string>

#include <QJsonObject>
#include <QLayout>

namespace tomviz {

class ModuleContour::Private
{
public:
  std::string ColorArrayName;
  bool UseSolidColor = false;
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
  return QIcon(":/icons/pqIsosurface.png");
}

bool ModuleContour::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  d->ColorArrayName = data->activeScalars().toStdString();

  m_flyingEdges->SetInputConnection(data->producer()->GetOutputPort());
  resetIsoValue();

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

void ModuleContour::onActiveScalarsChanged()
{
  d->ColorArrayName = dataSource()->activeScalars().toStdString();
  updateIsoRange();
  resetIsoValue();
  updateColorMap();
}

void ModuleContour::updateColorMap()
{
  auto* lut =
    vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject());
  m_mapper->SetLookupTable(lut);

  if (d->UseSolidColor) {
    // Have to set the color array to "" to use a solid color
    // Diffuse color should already be set on the property
    m_mapper->SelectColorArray("");
  } else {
    m_mapper->SelectColorArray(d->ColorArrayName.c_str());
  }
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
}

void ModuleContour::updatePanel()
{
  if (!m_controllers)
    return;

  QSignalBlocker blocker(m_controllers);

  updateIsoRange();

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
}

void ModuleContour::updateIsoRange()
{
  if (!m_controllers)
    return;

  double range[2];
  dataSource()->getRange(range);
  m_controllers->setIsoRange(range);
}

void ModuleContour::resetIsoValue()
{
  double range[2];
  dataSource()->getRange(range);

  // Use 2/3 of the range by default
  double val = (range[1] - range[0]) * 2 / 3 + range[0];
  setIsoValue(val);
}

QJsonObject ModuleContour::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["useSolidColor"] = d->UseSolidColor;
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
  d->UseSolidColor = props["useSolidColor"].toBool();
  onColorChanged(QColor(props["color"].toString()));
  onIsoChanged(props["contourValue"].toDouble());

  QJsonObject lighting = props["lighting"].toObject();
  onAmbientChanged(lighting["ambient"].toDouble());
  onDiffuseChanged(lighting["diffuse"].toDouble());
  onSpecularChanged(lighting["specular"].toDouble());
  onSpecularPowerChanged(lighting["specularPower"].toDouble());

  onRepresentationChanged(props["representation"].toString());
  onOpacityChanged(props["opacity"].toDouble());
  onColorMapDataToggled(props["mapScalars"].toBool());

  updatePanel();

  return true;
}

void ModuleContour::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  m_actor->SetPosition(pos);
}

vtkDataObject* ModuleContour::dataToExport()
{
  return m_flyingEdges->GetOutputDataObject(0);
}

void ModuleContour::setIsoValue(double value)
{
  onIsoChanged(value);
  if (m_controllers)
    m_controllers->setIso(value);
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

} // end of namespace tomviz
