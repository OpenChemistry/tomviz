/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleThreshold.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "Utilities.h"
#include "pqProxiesWidget.h"
#include "pqSignalAdaptors.h"
#include "pqStringVectorPropertyWidget.h"
#include "pqWidgetRangeDomain.h"

#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace tomviz {

ModuleThreshold::ModuleThreshold(QObject* parentObject) : Module(parentObject)
{}

ModuleThreshold::~ModuleThreshold()
{
  finalize();
}

QIcon ModuleThreshold::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqThreshold.svg");
}

bool ModuleThreshold::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  auto producer = data->proxy();

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  // Create the contour filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "Threshold"));

  m_thresholdFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_thresholdFilter);
  controller->PreInitializeProxy(m_thresholdFilter);
  vtkSMPropertyHelper(m_thresholdFilter, "Input").Set(producer);
  controller->PostInitializeProxy(m_thresholdFilter);
  controller->RegisterPipelineProxy(m_thresholdFilter);

  vtkSMPropertyHelper(m_thresholdFilter, "ThresholdMethod").Set("Between");

  // Update min/max to avoid thresholding the full dataset.
  vtkSMPropertyHelper lowerProp(m_thresholdFilter, "LowerThreshold");
  vtkSMPropertyHelper upperProp(m_thresholdFilter, "UpperThreshold");

  double range[2], newRange[2];
  lowerProp.Get(&range[0]);
  upperProp.Get(&range[1]);

  double delta = (range[1] - range[0]);
  double mid = ((range[0] + range[1]) / 2.0);
  newRange[0] = mid - 0.1 * delta;
  newRange[1] = mid + 0.1 * delta;

  lowerProp.Set(newRange[0]);
  upperProp.Set(newRange[1]);

  m_thresholdFilter->UpdateVTKObjects();

  // Create the representation for it.
  m_thresholdRepresentation = controller->Show(m_thresholdFilter, 0, vtkView);
  Q_ASSERT(m_thresholdRepresentation);
  vtkSMRepresentationProxy::SetRepresentationType(m_thresholdRepresentation,
                                                  "Surface");
  vtkSMPropertyHelper(m_thresholdRepresentation, "Position")
    .Set(data->displayPosition(), 3);
  vtkSMPropertyHelper(m_thresholdRepresentation, "Orientation")
    .Set(data->displayOrientation(), 3);
  updateColorMap();
  m_thresholdRepresentation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  connect(data, SIGNAL(activeScalarsChanged()), SLOT(onScalarArrayChanged()));
  onScalarArrayChanged();

  return true;
}

void ModuleThreshold::updateColorMap()
{
  Q_ASSERT(m_thresholdRepresentation);

  // by default, use the data source's color/opacity maps.
  vtkSMPropertyHelper(m_thresholdRepresentation, "LookupTable").Set(colorMap());
  vtkSMPropertyHelper(m_thresholdRepresentation, "ScalarOpacityFunction")
    .Set(opacityMap());

  m_thresholdRepresentation->UpdateVTKObjects();
}

bool ModuleThreshold::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_thresholdRepresentation);
  controller->UnRegisterProxy(m_thresholdFilter);
  m_thresholdFilter = nullptr;
  m_thresholdRepresentation = nullptr;
  return true;
}

bool ModuleThreshold::setVisibility(bool val)
{
  Q_ASSERT(m_thresholdRepresentation);
  vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility").Set(val ? 1 : 0);
  m_thresholdRepresentation->UpdateVTKObjects();

  Module::setVisibility(val);

  return true;
}

bool ModuleThreshold::visibility() const
{
  if (m_thresholdRepresentation) {
    return vtkSMPropertyHelper(m_thresholdRepresentation, "Visibility")
             .GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleThreshold::addToPanel(QWidget* panel)
{
  Q_ASSERT(m_thresholdFilter);
  Q_ASSERT(m_thresholdRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;

  pqStringVectorPropertyWidget* arraySelection =
    new pqStringVectorPropertyWidget(
      m_thresholdFilter->GetProperty("SelectInputScalars"),
      m_thresholdFilter);
  layout->addWidget(arraySelection);

  auto lowerProp = m_thresholdFilter->GetProperty("LowerThreshold");
  auto upperProp = m_thresholdFilter->GetProperty("UpperThreshold");

  auto* lowerSlider = new DoubleSliderWidget(true);
  auto* upperSlider = new DoubleSliderWidget(true);

  auto clampLower = [lowerSlider, upperSlider]() {
    if (lowerSlider->value() > upperSlider->value()) {
      lowerSlider->setValue(upperSlider->value());
    }
  };

  auto clampUpper = [lowerSlider, upperSlider]() {
    if (lowerSlider->value() > upperSlider->value()) {
      upperSlider->setValue(lowerSlider->value());
    }
  };

  connect(lowerSlider, &DoubleSliderWidget::valueEdited, upperSlider,
          clampUpper);
  connect(upperSlider, &DoubleSliderWidget::valueEdited, lowerSlider,
          clampLower);

  // Only update when the user releases the slider
  lowerSlider->setSliderTracking(false);
  upperSlider->setSliderTracking(false);

  lowerSlider->setKeyboardTracking(false);
  upperSlider->setKeyboardTracking(false);

  lowerSlider->setLineEditWidth(50);
  upperSlider->setLineEditWidth(50);

  QFormLayout* thresholdFormLayout = new QFormLayout;
  thresholdFormLayout->setHorizontalSpacing(5);
  layout->addItem(thresholdFormLayout);

  thresholdFormLayout->addRow("Minimum", lowerSlider);
  thresholdFormLayout->addRow("Maximum", upperSlider);

  m_links.addPropertyLink(lowerSlider, "value", SIGNAL(valueEdited(double)),
                          m_thresholdRepresentation, lowerProp);
  m_links.addPropertyLink(upperSlider, "value", SIGNAL(valueEdited(double)),
                          m_thresholdRepresentation, upperProp);

  // Keep the slider ranges up-to-date with the data
  new pqWidgetRangeDomain(lowerSlider, "minimum", "maximum", lowerProp);
  new pqWidgetRangeDomain(upperSlider, "minimum", "maximum", upperProp);

  QFormLayout* formLayout = new QFormLayout;
  formLayout->setHorizontalSpacing(5);
  layout->addItem(formLayout);

  QComboBox* representations = new QComboBox;
  representations->addItem("Surface");
  representations->addItem("Wireframe");
  representations->addItem("Points");
  formLayout->addRow("Representation", representations);

  DoubleSliderWidget* opacitySlider = new DoubleSliderWidget(true);
  opacitySlider->setLineEditWidth(50);
  formLayout->addRow("Opacity", opacitySlider);

  DoubleSliderWidget* specularSlider = new DoubleSliderWidget(true);
  specularSlider->setLineEditWidth(50);
  formLayout->addRow("Specular", specularSlider);

  QCheckBox* mapScalarsCheckBox = new QCheckBox();
  formLayout->addRow("Color Map Data", mapScalarsCheckBox);

  layout->addStretch();
  panel->setLayout(layout);

  pqSignalAdaptorComboBox* adaptor =
    new pqSignalAdaptorComboBox(representations);

  m_links.addPropertyLink(
    adaptor, "currentText", SIGNAL(currentTextChanged(QString)),
    m_thresholdRepresentation,
    m_thresholdRepresentation->GetProperty("Representation"));

  m_links.addPropertyLink(opacitySlider, "value", SIGNAL(valueEdited(double)),
                          m_thresholdRepresentation,
                          m_thresholdRepresentation->GetProperty("Opacity"), 0);
  m_links.addPropertyLink(specularSlider, "value", SIGNAL(valueEdited(double)),
                          m_thresholdRepresentation,
                          m_thresholdRepresentation->GetProperty("Specular"),
                          0);

  m_links.addPropertyLink(mapScalarsCheckBox, "checked", SIGNAL(toggled(bool)),
                          m_thresholdRepresentation,
                          m_thresholdRepresentation->GetProperty("MapScalars"),
                          0);

  connect(arraySelection, &pqPropertyWidget::changeFinished, arraySelection,
          &pqPropertyWidget::apply);
  connect(arraySelection, &pqPropertyWidget::changeFinished, this,
          &Module::renderNeeded);
  connect(lowerSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleThreshold::dataUpdated);
  connect(upperSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleThreshold::dataUpdated);
  connect(representations, &QComboBox::currentTextChanged, this,
          &ModuleThreshold::dataUpdated);
  connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleThreshold::dataUpdated);
  connect(specularSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleThreshold::dataUpdated);
  connect(mapScalarsCheckBox, &QCheckBox::toggled, this,
          &ModuleThreshold::dataUpdated);
}

void ModuleThreshold::dataUpdated()
{
  m_links.accept();
  // FIXME: why aren't threshold filter changes being pushed automatically?
  m_thresholdFilter->UpdateVTKObjects();
  emit renderNeeded();
}

void ModuleThreshold::onScalarArrayChanged()
{
  QString arrayName = dataSource()->activeScalars();
  vtkSMPropertyHelper(m_thresholdRepresentation, "ColorArrayName")
    .SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS,
                            arrayName.toLatin1().data());
  m_thresholdRepresentation->UpdateVTKObjects();

  emit renderNeeded();
}

QJsonObject ModuleThreshold::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  auto rep = m_thresholdRepresentation;
  vtkSMPropertyHelper scalars(m_thresholdFilter, "SelectInputScalars");
  props["scalarArray"] = scalars.GetAsInt();
  double range[2];
  vtkSMPropertyHelper(m_thresholdFilter, "LowerThreshold").Get(&range[0]);
  vtkSMPropertyHelper(m_thresholdFilter, "UpperThreshold").Get(&range[1]);
  props["minimum"] = range[0];
  props["maximum"] = range[1];
  vtkSMPropertyHelper representationHelper(rep->GetProperty("Representation"));
  props["representation"] = representationHelper.GetAsString();
  vtkSMPropertyHelper specular(rep->GetProperty("Specular"));
  props["specular"] = specular.GetAsDouble();
  vtkSMPropertyHelper opacity(rep->GetProperty("Opacity"));
  props["opacity"] = opacity.GetAsDouble();
  vtkSMPropertyHelper mapScalars(rep->GetProperty("MapScalars"));
  props["mapScalars"] = mapScalars.GetAsInt() == 1;

  json["properties"] = props;

  return json;
}

bool ModuleThreshold::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    auto rep = m_thresholdRepresentation;
    vtkSMPropertyHelper scalars(m_thresholdFilter, "SelectInputScalars");
    scalars.Set(props["scalarArray"].toInt());
    vtkSMPropertyHelper(m_thresholdFilter, "LowerThreshold")
      .Set(props["minimum"].toDouble());
    vtkSMPropertyHelper(m_thresholdFilter, "UpperThreshold")
      .Set(props["maximum"].toDouble());
    vtkSMPropertyHelper repHelper(rep, "Representation");
    repHelper.Set(props["representation"].toString().toStdString().c_str());
    vtkSMPropertyHelper specular(rep, "Specular");
    specular.Set(props["specular"].toDouble());
    vtkSMPropertyHelper opacity(rep, "Opacity");
    opacity.Set(props["opacity"].toDouble());
    vtkSMPropertyHelper mapScalars(rep, "MapScalars");
    mapScalars.Set(props["mapScalars"].toBool() ? 1 : 0);
    m_thresholdFilter->UpdateVTKObjects();
    rep->UpdateVTKObjects();
    return true;
  }
  return false;
}

void ModuleThreshold::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_thresholdRepresentation, "Position").Set(pos, 3);
  m_thresholdRepresentation->UpdateVTKObjects();
}

void ModuleThreshold::dataSourceRotated(double newX, double newY, double newZ)
{
  double orientation[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_thresholdRepresentation, "Orientation")
    .Set(orientation, 3);
  m_thresholdRepresentation->UpdateVTKObjects();
}

} // namespace tomviz
