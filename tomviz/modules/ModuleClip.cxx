/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleClip.h"
#include "DataSource.h"
#include "IntSliderWidget.h"
#include "Utilities.h"

#include <vtkAlgorithm.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkNonOrthoImagePlaneWidget.h>
#include <vtkPlane.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTrivialProducer.h>

#include <pqCoreUtilities.h>
#include <pqLineEdit.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

ModuleClip::ModuleClip(QObject* parentObject) : Module(parentObject) {}

ModuleClip::~ModuleClip()
{
  finalize();
}

QIcon ModuleClip::icon() const
{
  return QIcon(":/icons/orthoslice.png");
}

bool ModuleClip::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  auto producer = data->proxy();
  auto pxm = producer->GetSessionProxyManager();

  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  // Create the Properties panel proxy
  m_propsPanelProxy.TakeReference(
    pxm->NewProxy("tomviz_proxies", "NonOrthogonalSlice"));

  pqCoreUtilities::connect(m_propsPanelProxy, vtkCommand::PropertyModifiedEvent,
                           this, SLOT(onPropertyChanged()));

  m_clipVolume = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_clipVolume);
  controller->PreInitializeProxy(m_clipVolume);
  vtkSMPropertyHelper(m_clipVolume, "Input").Set(producer);
  controller->PostInitializeProxy(m_clipVolume);
  controller->RegisterPipelineProxy(m_clipVolume);

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  const bool widgetSetup = setupWidget(vtkView);

  if (widgetSetup) {
    m_widget->SetDisplayOffset(data->displayPosition());
    m_widget->On();
    onDirectionChanged(m_direction);
    pqCoreUtilities::connect(m_widget, vtkCommand::InteractionEvent, this,
                             SLOT(onPlaneChanged()));
    connect(data, SIGNAL(dataChanged()), this, SLOT(dataUpdated()));
  }

  Q_ASSERT(m_widget);
  return widgetSetup;
}

// Should only be called from initialize after ClipVolume has been setup
bool ModuleClip::setupWidget(vtkSMViewProxy* vtkView)
{
  vtkAlgorithm* clipVolumeAlg =
    vtkAlgorithm::SafeDownCast(m_clipVolume->GetClientSideObject());

  vtkRenderWindowInteractor* rwi = vtkView->GetRenderWindow()->GetInteractor();

  if (!rwi || !clipVolumeAlg) {
    return false;
  }

  m_widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();
  m_widget->SetTextureVisibility(0);

  // Set the interactor on the widget to be what the current
  // render window is using.
  m_widget->SetInteractor(rwi);

  // Setup the color of the border of the widget.
  {
    double color[3] = { 0.5, 0.5, 0.5 };
    double opacity = 1.0;
    m_widget->GetPlaneProperty()->SetColor(color);
    m_widget->GetPlaneProperty()->SetOpacity(opacity);
    m_widget->GetPlaneProperty()->SetLineWidth(3.0);
  }

  m_clippingPlane = vtkSmartPointer<vtkPlane>::New();
  m_clippingPlane->SetOrigin(m_widget->GetCenter());
  m_clippingPlane->SetNormal(m_widget->GetNormal());

  m_widget->SetInputConnection(clipVolumeAlg->GetOutputPort());

  Q_ASSERT(rwi);
  Q_ASSERT(clipVolumeAlg);
  onPlaneChanged();
  return true;
}

void ModuleClip::updatePlaneWidget()
{
  if (!m_planeSlider || !imageData())
    return;

  int dims[3];
  imageData()->GetDimensions(dims);

  int axis = directionAxis(m_direction);
  int maxPlane = dims[axis] - 1;

  m_planeSlider->setMinimum(0);
  m_planeSlider->setMaximum(maxPlane);
}

bool ModuleClip::finalize()
{
  emit clipFilterUpdated(m_clippingPlane, true);
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_clipVolume);

  m_clipVolume = nullptr;

  if (m_widget != nullptr) {
    m_widget->Off();
  }

  return true;
}

bool ModuleClip::setVisibility(bool val)
{
  Q_ASSERT(m_widget);
  m_widget->SetEnabled(val ? 1 : 0);
  // update the state of the arrow as well since it cannot update when the
  // widget is not enabled
  if (val) {
    vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
    // Not this: it hides the plane as well as the arrow...
    // Widget->SetEnabled(showProperty.GetAsInt());
    m_widget->SetArrowVisibility(showProperty.GetAsInt());
    m_widget->SetInteraction(showProperty.GetAsInt());
  }
  val ? emit clipFilterUpdated(m_clippingPlane, false) : emit clipFilterUpdated(m_clippingPlane, true);
  return true;
}

bool ModuleClip::visibility() const
{
  if (m_widget) {
    return m_widget->GetEnabled() != 0;
  } else {
    return false;
  }
}

void ModuleClip::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  QFormLayout* formLayout = new QFormLayout;

  QWidget* container = new QWidget;
  container->setLayout(formLayout);
  layout->addWidget(container);
  formLayout->setContentsMargins(0, 0, 0, 0);

  m_directionCombo = new QComboBox();
  m_directionCombo->addItem("XY Plane", QVariant(Direction::XY));
  m_directionCombo->addItem("YZ Plane", QVariant(Direction::YZ));
  m_directionCombo->addItem("XZ Plane", QVariant(Direction::XZ));
  m_directionCombo->addItem("Custom", QVariant(Direction::Custom));
  m_directionCombo->setCurrentIndex(static_cast<int>(m_direction));
  formLayout->addRow("Direction", m_directionCombo);

  m_planeSlider = new IntSliderWidget(true);
  m_planeSlider->setLineEditWidth(50);
  m_planeSlider->setPageStep(1);
  m_planeSlider->setMinimum(0);
  int axis = directionAxis(m_direction);
  bool isOrtho = axis >= 0;
  if (isOrtho) {
    int dims[3];
    imageData()->GetDimensions(dims);
    m_planeSlider->setMaximum(dims[axis] - 1);
  }

  // Sanity check: make sure the plane value is within the bounds
  if (m_plane < m_planeSlider->minimum())
    m_plane = m_planeSlider->minimum();
  else if (m_plane > m_planeSlider->maximum())
    m_plane = m_planeSlider->maximum();

  m_planeSlider->setValue(m_plane);

  formLayout->addRow("Plane", m_planeSlider);

  QCheckBox* showArrow = new QCheckBox("Show Arrow");
  formLayout->addRow(showArrow);

  m_Links.addPropertyLink(showArrow, "checked", SIGNAL(toggled(bool)),
                          m_propsPanelProxy,
                          m_propsPanelProxy->GetProperty("ShowArrow"), 0);
  connect(showArrow, &QCheckBox::toggled, this, &ModuleClip::dataUpdated);

  QLabel* label = new QLabel("Point on Plane");
  layout->addWidget(label);
  QHBoxLayout* row = new QHBoxLayout;
  const char* labels[] = { "X:", "Y:", "Z:" };
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setEnabled(!isOrtho);
    inputBox->setValidator(new QDoubleValidator(inputBox));
    m_Links.addPropertyLink(
      inputBox, "text2", SIGNAL(textChanged(const QString&)), m_propsPanelProxy,
      m_propsPanelProxy->GetProperty("PointOnPlane"), i);
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleClip::dataUpdated);
    row->addWidget(inputBox);
    m_pointInputs[i] = inputBox;
  }
  layout->addItem(row);

  label = new QLabel("Plane Normal");
  layout->addWidget(label);
  row = new QHBoxLayout;
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setEnabled(!isOrtho);
    inputBox->setValidator(new QDoubleValidator(inputBox));
    m_Links.addPropertyLink(
      inputBox, "text2", SIGNAL(textChanged(const QString&)), m_propsPanelProxy,
      m_propsPanelProxy->GetProperty("PlaneNormal"), i);
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleClip::dataUpdated);
    row->addWidget(inputBox);
    m_normalInputs[i] = inputBox;
  }
  layout->addItem(row);

  layout->addStretch();

  panel->setLayout(layout);

  connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            Direction dir = m_directionCombo->itemData(idx).value<Direction>();
            onDirectionChanged(dir);
          });

  connect(m_planeSlider, &IntSliderWidget::valueEdited, this,
          QOverload<int>::of(&ModuleClip::onPlaneChanged));
  connect(m_planeSlider, &IntSliderWidget::valueChanged, this,
          QOverload<int>::of(&ModuleClip::onPlaneChanged));
}

void ModuleClip::dataUpdated()
{
  m_Links.accept();
  // In case there are new planes, update min and max
  updatePlaneWidget();
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

QJsonObject ModuleClip::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
  props["showArrow"] = showProperty.GetAsInt() != 0;

  // Serialize the plane
  double point[3];
  m_widget->GetOrigin(point);
  QJsonArray origin = { point[0], point[1], point[2] };
  m_widget->GetPoint1(point);
  QJsonArray point1 = { point[0], point[1], point[2] };
  m_widget->GetPoint2(point);
  QJsonArray point2 = { point[0], point[1], point[2] };

  props["origin"] = origin;
  props["point1"] = point1;
  props["point2"] = point2;

  props["plane"] = m_plane;
  QVariant qData;
  qData.setValue(m_direction);
  props["direction"] = qData.toString();

  json["properties"] = props;
  return json;
}

bool ModuleClip::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
    showProperty.Set(props["showArrow"].toBool() ? 1 : 0);
    if (props.contains("origin") && props.contains("point1") &&
        props.contains("point2")) {
      auto o = props["origin"].toArray();
      auto p1 = props["point1"].toArray();
      auto p2 = props["point2"].toArray();
      double origin[3] = { o[0].toDouble(), o[1].toDouble(), o[2].toDouble() };
      double point1[3] = { p1[0].toDouble(), p1[1].toDouble(),
                           p1[2].toDouble() };
      double point2[3] = { p2[0].toDouble(), p2[1].toDouble(),
                           p2[2].toDouble() };
      m_widget->SetOrigin(origin);
      m_widget->SetPoint1(point1);
      m_widget->SetPoint2(point2);
    }

    m_widget->UpdatePlacement();
    // If deserializing a former OrthogonalPlane, the direction is encoded in
    // the property "planeMode" as an int
    if (props.contains("planeMode")) {
      Direction direction = modeToDirection(props["planeMode"].toInt());
      onDirectionChanged(direction);
    }
    if (props.contains("direction")) {
      Direction direction = stringToDirection(props["direction"].toString());
      onDirectionChanged(direction);
    }
    if (props.contains("plane")) {
      m_plane = props["plane"].toInt();
      onPlaneChanged(m_plane);
    }
    onPlaneChanged();
    return true;
  }
  return false;
}

void ModuleClip::onPropertyChanged()
{
  // Avoid recursive clobbering of the plane position
  if (m_ignoreSignals) {
    return;
  }
  m_ignoreSignals = true;
  vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
  if (m_widget->GetEnabled()) {
    // Not this: it hides the plane as well as the arrow...
    // Widget->SetEnabled(showProperty.GetAsInt());
    m_widget->SetArrowVisibility(showProperty.GetAsInt());
    m_widget->SetInteraction(showProperty.GetAsInt());
  }
  vtkSMPropertyHelper pointProperty(m_propsPanelProxy, "PointOnPlane");
  std::vector<double> centerPoint = pointProperty.GetDoubleArray();
  m_widget->SetCenter(&centerPoint[0]);
  m_clippingPlane->SetOrigin(&centerPoint[0]);
  vtkSMPropertyHelper normalProperty(m_propsPanelProxy, "PlaneNormal");
  std::vector<double> normalVector = normalProperty.GetDoubleArray();
  m_widget->SetNormal(&normalVector[0]);
  m_clippingPlane->SetNormal(&normalVector[0]);
  m_widget->UpdatePlacement();
  m_ignoreSignals = false;
}

void ModuleClip::onPlaneChanged()
{
  // Avoid recursive clobbering of the plane position
  if (m_ignoreSignals) {
    return;
  }
  m_ignoreSignals = true;
  vtkSMPropertyHelper pointProperty(m_propsPanelProxy, "PointOnPlane");
  double* centerPoint = m_widget->GetCenter();
  pointProperty.Set(centerPoint, 3);
  m_clippingPlane->SetOrigin(centerPoint);
  vtkSMPropertyHelper normalProperty(m_propsPanelProxy, "PlaneNormal");
  double* normalVector = m_widget->GetNormal();
  normalProperty.Set(normalVector, 3);
  m_clippingPlane->SetNormal(normalVector);
  vtkSMPropertyHelper mapScalarsProperty(m_propsPanelProxy, "MapScalars");

  // Adjust the plane slider if the plane has changed from dragging the arrow
  onPlaneChanged(centerPoint);

  m_ignoreSignals = false;
}

void ModuleClip::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  m_widget->SetDisplayOffset(pos);
}

bool ModuleClip::areScalarsMapped() const
{
  vtkSMPropertyHelper mapScalars(m_propsPanelProxy->GetProperty("MapScalars"));
  return mapScalars.GetAsInt() != 0;
}

vtkImageData* ModuleClip::imageData() const
{
  vtkImageData* data = vtkImageData::SafeDownCast(
    dataSource()->producer()->GetOutputDataObject(0));
  Q_ASSERT(data);
  return data;
}

void ModuleClip::onDirectionChanged(Direction direction)
{
  m_direction = direction;
  int axis = directionAxis(direction);

  bool isOrtho = axis >= 0;

  for (int i = 0; i < 3; ++i) {
    if (m_pointInputs[i]) {
      m_pointInputs[i]->setEnabled(!isOrtho);
    }
    if (m_normalInputs[i]) {
      m_normalInputs[i]->setEnabled(!isOrtho);
    }
  }
  if (m_planeSlider) {
    m_planeSlider->setVisible(isOrtho);
  }

  m_widget->SetPlaneOrientation(axis);

  if (m_directionCombo) {
    if (direction != m_directionCombo->currentData().value<Direction>()) {
      for (int i = 0; i < m_directionCombo->count(); ++i) {
        Direction data = m_directionCombo->itemData(i).value<Direction>();
        if (data == direction) {
          m_directionCombo->setCurrentIndex(i);
        }
      }
    }
  }

  if (!isOrtho) {
    return;
  }

  int dims[3];
  imageData()->GetDimensions(dims);

  double normal[3] = { 0, 0, 0 };
  int planePosition = 0;
  int maxPlane = 0;

  normal[axis] = 1;
  maxPlane = dims[axis] - 1;

  m_widget->SetNormal(normal);
  if (m_planeSlider) {
    m_planeSlider->setMinimum(0);
    m_planeSlider->setMaximum(maxPlane);
  }

  onPlaneChanged(plane);
  onPlaneChanged();
  dataUpdated();

  emit clipFilterUpdated(m_clippingPlane, false);
}

void ModuleClip::onPlaneChanged(int plane)
{
  m_plane = plane;
  int axis = directionAxis(m_direction);

  if (axis < 0) {
    return;
  }

  m_widget->SetSliceIndex(plane);
  if (m_planeSlider) {
    m_planeSlider->setValue(plane);
  }

  onPlaneChanged();
  dataUpdated();

  emit clipFilterUpdated(m_clippingPlane, false);
}

void ModuleClip::onPlaneChanged(double* point)
{
  int axis = directionAxis(m_direction);
  if (axis < 0) {
    return;
  }

  int dims[3];
  imageData()->GetDimensions(dims);
  double bounds[6];
  imageData()->GetBounds(bounds);
  int plane;

  plane = (dims[axis] - 1) * (point[axis] - bounds[2 * axis]) /
          (bounds[2 * axis + 1] - bounds[2 * axis]);

  onPlaneChanged(plane);

  emit clipFilterUpdated(m_clippingPlane, false);
}

int ModuleClip::directionAxis(Direction direction)
{
  switch (direction) {
    case Direction::XY: {
      return 2;
    }
    case Direction::YZ: {
      return 0;
    }
    case Direction::XZ: {
      return 1;
    }
    default: {
      return -1;
    }
  }
}

ModuleClip::Direction ModuleClip::stringToDirection(const QString& name)
{
  if (name == "XY") {
    return Direction::XY;
  } else if (name == "YZ") {
    return Direction::YZ;
  } else if (name == "XZ") {
    return Direction::XZ;
  } else {
    return Direction::Custom;
  }
}

ModuleClip::Direction ModuleClip::modeToDirection(int planeMode)
{
  switch (planeMode) {
    case 5: {
      return Direction::XY;
    }
    case 6: {
      return Direction::YZ;
    }
    case 7: {
      return Direction::XZ;
    }
    default: {
      return Direction::Custom;
    }
  }
}

} // namespace tomviz
