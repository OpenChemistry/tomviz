/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleSlice.h"

#include "DataSource.h"
#include "Utilities.h"

#include <vtkAlgorithm.h>
#include <vtkCommand.h>
#include <vtkDataObject.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkNonOrthoImagePlaneWidget.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarsToColors.h>

#include <pqCoreUtilities.h>
#include <pqDoubleVectorPropertyWidget.h>
#include <pqLineEdit.h>
#include <pqProxiesWidget.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPVRepresentationProxy.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QDebug>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

ModuleSlice::ModuleSlice(QObject* parentObject) : Module(parentObject) {}

ModuleSlice::~ModuleSlice()
{
  finalize();
}

QIcon ModuleSlice::icon() const
{
  return QIcon(":/icons/pqSlice.png");
}

bool ModuleSlice::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  auto producer = data->proxy();
  auto pxm = producer->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  // Create the Properties panel proxy
  m_propsPanelProxy.TakeReference(
    pxm->NewProxy("tomviz_proxies", "NonOrthogonalSlice"));

  pqCoreUtilities::connect(m_propsPanelProxy, vtkCommand::PropertyModifiedEvent,
                           this, SLOT(onPropertyChanged()));

  m_passThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_passThrough);
  controller->PreInitializeProxy(m_passThrough);
  vtkSMPropertyHelper(m_passThrough, "Input").Set(producer);
  controller->PostInitializeProxy(m_passThrough);
  controller->RegisterPipelineProxy(m_passThrough);

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  const bool widgetSetup = setupWidget(vtkView, producer);

  if (widgetSetup) {
    m_widget->SetDisplayOffset(data->displayPosition());
    m_widget->On();
    m_widget->InteractionOn();
    pqCoreUtilities::connect(m_widget, vtkCommand::InteractionEvent, this,
                             SLOT(onPlaneChanged()));
    connect(data, SIGNAL(dataChanged()), this, SLOT(dataUpdated()));
  }

  Q_ASSERT(m_widget);
  return widgetSetup;
}

//  Should only be called from initialize after the PassThrough has been setup
bool ModuleSlice::setupWidget(vtkSMViewProxy* vtkView,
                              vtkSMSourceProxy* producer)
{
  vtkAlgorithm* passThroughAlg =
    vtkAlgorithm::SafeDownCast(m_passThrough->GetClientSideObject());

  vtkRenderWindowInteractor* rwi = vtkView->GetRenderWindow()->GetInteractor();

  // Determine the name of the property we are coloring by
  const char* propertyName = producer->GetDataInformation()
                               ->GetPointDataInformation()
                               ->GetArrayInformation(0)
                               ->GetName();

  if (!rwi || !passThroughAlg || !propertyName) {
    return false;
  }

  m_widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();

  // Set the interactor on the widget to be what the current
  // render window is using.
  m_widget->SetInteractor(rwi);

  // Setup the color of the border of the widget.
  {
    double color[3] = { 1, 0, 0 };
    m_widget->GetPlaneProperty()->SetColor(color);
  }

  // Turn texture interpolation to be linear.
  m_widget->TextureInterpolateOn();
  m_widget->SetResliceInterpolateToLinear();

  // Construct the transfer function proxy for the widget.
  vtkSMProxy* lut = colorMap();

  // Set the widgets lookup table to be the one that the transfer function
  // manager is using.
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  m_widget->SetLookupTable(stc);

  // Lastly we set up the input connection.
  m_widget->SetInputConnection(passThroughAlg->GetOutputPort());

  Q_ASSERT(rwi);
  Q_ASSERT(passThroughAlg);
  onPlaneChanged();
  return true;
}

void ModuleSlice::updateColorMap()
{
  Q_ASSERT(m_widget);

  // Construct the transfer function proxy for the widget
  vtkSMProxy* lut = colorMap();

  // set the widgets lookup table to be the one that the transfer function
  // manager is using
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  m_widget->SetLookupTable(stc);
}

bool ModuleSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_passThrough);

  m_passThrough = nullptr;

  if (m_widget != nullptr) {
    m_widget->InteractionOff();
    m_widget->Off();
  }

  return true;
}

bool ModuleSlice::setVisibility(bool val)
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
  return true;
}

bool ModuleSlice::visibility() const
{
  if (m_widget) {
    return m_widget->GetEnabled() != 0;
  } else {
    return false;
  }
}

void ModuleSlice::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;

  m_opacityCheckBox = new QCheckBox("Map Opacity");
  layout->addWidget(m_opacityCheckBox);

  QCheckBox* mapScalarsCheckBox = new QCheckBox("Color Map Data");
  layout->addWidget(mapScalarsCheckBox);

  auto line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addWidget(line);

  QCheckBox* showArrow = new QCheckBox("Show Arrow");
  layout->addWidget(showArrow);

  m_Links.addPropertyLink(showArrow, "checked", SIGNAL(toggled(bool)),
                          m_propsPanelProxy,
                          m_propsPanelProxy->GetProperty("ShowArrow"), 0);
  connect(showArrow, &QCheckBox::toggled, this, &ModuleSlice::dataUpdated);

  QLabel* label = new QLabel("Point on Plane");
  layout->addWidget(label);
  QHBoxLayout* row = new QHBoxLayout;
  const char* labels[] = { "X:", "Y:", "Z:" };
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setValidator(new QDoubleValidator(inputBox));
    m_Links.addPropertyLink(
      inputBox, "text2", SIGNAL(textChanged(const QString&)), m_propsPanelProxy,
      m_propsPanelProxy->GetProperty("PointOnPlane"), i);
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleSlice::dataUpdated);
    row->addWidget(inputBox);
  }
  layout->addItem(row);

  label = new QLabel("Plane Normal");
  layout->addWidget(label);
  row = new QHBoxLayout;
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setValidator(new QDoubleValidator(inputBox));
    m_Links.addPropertyLink(
      inputBox, "text2", SIGNAL(textChanged(const QString&)), m_propsPanelProxy,
      m_propsPanelProxy->GetProperty("PlaneNormal"), i);
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleSlice::dataUpdated);
    row->addWidget(inputBox);
  }
  layout->addItem(row);

  m_Links.addPropertyLink(mapScalarsCheckBox, "checked", SIGNAL(toggled(bool)),
                          m_propsPanelProxy,
                          m_propsPanelProxy->GetProperty("MapScalars"), 0);
  connect(mapScalarsCheckBox, SIGNAL(toggled(bool)), this, SLOT(dataUpdated()));

  layout->addStretch();

  panel->setLayout(layout);

  connect(m_opacityCheckBox, &QCheckBox::toggled, this, [this](bool val) {
    m_mapOpacity = val;
    // Ensure the colormap is detached before applying opacity
    if (val) {
      setUseDetachedColorMap(val);
    }
    auto func = vtkPVDiscretizableColorTransferFunction::SafeDownCast(
      colorMap()->GetClientSideObject());
    func->SetEnableOpacityMapping(val);
    emit opacityEnforced(val);
    updateColorMap();
    emit renderNeeded();
  });

  m_opacityCheckBox->setChecked(m_mapOpacity);
}

void ModuleSlice::dataUpdated()
{
  m_Links.accept();
  m_widget->SetMapScalars(
    vtkSMPropertyHelper(m_propsPanelProxy->GetProperty("MapScalars"), 1)
      .GetAsInt());
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

QJsonObject ModuleSlice::serialize() const
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
  props["mapScalars"] = m_widget->GetMapScalars() != 0;
  props["mapOpacity"] = m_mapOpacity;

  json["properties"] = props;
  return json;
}

bool ModuleSlice::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
    showProperty.Set(props["showArrow"].toBool() ? 1 : 0);
    auto o = props["origin"].toArray();
    auto p1 = props["point1"].toArray();
    auto p2 = props["point2"].toArray();
    double origin[3] = { o[0].toDouble(), o[1].toDouble(), o[2].toDouble() };
    double point1[3] = { p1[0].toDouble(), p1[1].toDouble(), p1[2].toDouble() };
    double point2[3] = { p2[0].toDouble(), p2[1].toDouble(), p2[2].toDouble() };
    m_widget->SetOrigin(origin);
    m_widget->SetPoint1(point1);
    m_widget->SetPoint2(point2);
    m_widget->SetMapScalars(props["mapScalars"].toBool() ? 1 : 0);
    if (props.contains("mapOpacity")) {
      m_mapOpacity = props["mapOpacity"].toBool();
      if (m_opacityCheckBox) {
        m_opacityCheckBox->setChecked(m_mapOpacity);
      }
    }
    m_widget->UpdatePlacement();
    onPlaneChanged();
    return true;
  }
  return false;
}

void ModuleSlice::onPropertyChanged()
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
  vtkSMPropertyHelper normalProperty(m_propsPanelProxy, "PlaneNormal");
  std::vector<double> normalVector = normalProperty.GetDoubleArray();
  m_widget->SetNormal(&normalVector[0]);
  m_widget->UpdatePlacement();
  m_ignoreSignals = false;
}

void ModuleSlice::onPlaneChanged()
{
  // Avoid recursive clobbering of the plane position
  if (m_ignoreSignals) {
    return;
  }
  m_ignoreSignals = true;
  vtkSMPropertyHelper pointProperty(m_propsPanelProxy, "PointOnPlane");
  double* centerPoint = m_widget->GetCenter();
  pointProperty.Set(centerPoint, 3);
  vtkSMPropertyHelper normalProperty(m_propsPanelProxy, "PlaneNormal");
  double* normalVector = m_widget->GetNormal();
  normalProperty.Set(normalVector, 3);
  vtkSMPropertyHelper mapScalarsProperty(m_propsPanelProxy, "MapScalars");
  int mapScalars = m_widget->GetMapScalars();
  mapScalarsProperty.Set(mapScalars);

  m_ignoreSignals = false;
}

void ModuleSlice::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  m_widget->SetDisplayOffset(pos);
}

vtkSmartPointer<vtkDataObject> ModuleSlice::getDataToExport()
{
  return m_widget->GetResliceOutput();
}

bool ModuleSlice::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == m_passThrough.Get()) || (proxy == m_propsPanelProxy.Get());
}

std::string ModuleSlice::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_passThrough.Get()) {
    return "PassThrough";
  } else if (proxy == m_propsPanelProxy.Get()) {
    return "NonOrthoSlice";
  } else {
    qWarning(
      "Unknown proxy passed to module non-ortho-slice in save animation");
    return "";
  }
}

vtkSMProxy* ModuleSlice::getProxyForString(const std::string& str)
{
  if (str == "PassThrough") {
    return m_passThrough.Get();
  } else if (str == "NonOrthoSlice") {
    return m_propsPanelProxy.Get();
  } else {
    return nullptr;
  }
}

bool ModuleSlice::areScalarsMapped() const
{
  vtkSMPropertyHelper mapScalars(m_propsPanelProxy->GetProperty("MapScalars"));
  return mapScalars.GetAsInt() != 0;
}
} // namespace tomviz
