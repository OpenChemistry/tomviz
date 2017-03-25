/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "ModuleSlice.h"

#include "DataSource.h"
#include "Utilities.h"

#include <vtkAlgorithm.h>
#include <vtkCommand.h>
#include <vtkDataObject.h>
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
#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

ModuleSlice::ModuleSlice(QObject* parentObject) : Module(parentObject)
{
}

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
  vtkSMSourceProxy* producer = data->producer();
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

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
    m_widget->On();
    m_widget->InteractionOn();
    m_widget->SetDisplayOffset(data->displayPosition());
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

  layout->addStretch();

  panel->setLayout(layout);
}

void ModuleSlice::dataUpdated()
{
  m_Links.accept();
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

bool ModuleSlice::serialize(pugi::xml_node& ns) const
{
  // Save the state of the arrow's visibility
  vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
  ns.append_attribute("show_arrow").set_value(showProperty.GetAsInt());

  // Serialize the plane
  double point[3];
  pugi::xml_node plane = ns.append_child("Plane");
  pugi::xml_node pointNode;
  m_widget->GetOrigin(point);
  // Origin of plane
  pointNode = plane.append_child("Point");
  pointNode.append_attribute("index").set_value(0);
  pointNode.append_attribute("x").set_value(point[0]);
  pointNode.append_attribute("y").set_value(point[1]);
  pointNode.append_attribute("z").set_value(point[2]);
  // Point 1 of plane
  m_widget->GetPoint1(point);
  pointNode = plane.append_child("Point");
  pointNode.append_attribute("index").set_value(1);
  pointNode.append_attribute("x").set_value(point[0]);
  pointNode.append_attribute("y").set_value(point[1]);
  pointNode.append_attribute("z").set_value(point[2]);
  // Point 2 of plane
  m_widget->GetPoint2(point);
  pointNode = plane.append_child("Point");
  pointNode.append_attribute("index").set_value(2);
  pointNode.append_attribute("x").set_value(point[0]);
  pointNode.append_attribute("y").set_value(point[1]);
  pointNode.append_attribute("z").set_value(point[2]);

  // Let the superclass do its thing
  return Module::serialize(ns);
}

bool ModuleSlice::deserialize(const pugi::xml_node& ns)
{

  pugi::xml_node plane = ns.child("Plane");
  if (!plane) {
    // We are reading an older state file from before the change that added
    // the ability to save these...
    return Module::deserialize(ns);
  }
  // Deserialize the show arrow state
  vtkSMPropertyHelper showProperty(m_propsPanelProxy, "ShowArrow");
  showProperty.Set(ns.attribute("show_arrow").as_int());

  // Deserialize the plane
  double point[3];
  for (pugi::xml_node pointNode = plane.child("Point"); pointNode;
       pointNode = pointNode.next_sibling("Point")) {
    point[0] = pointNode.attribute("x").as_double();
    point[1] = pointNode.attribute("y").as_double();
    point[2] = pointNode.attribute("z").as_double();
    int index = pointNode.attribute("index").as_int();
    if (index == 0) {
      m_widget->SetOrigin(point);
    } else if (index == 1) {
      m_widget->SetPoint1(point);
    } else if (index == 2) {
      m_widget->SetPoint2(point);
    } else {
      qCritical("Unknown point index for slice plane point");
      return false;
    }
  }
  m_widget->UpdatePlacement();
  onPlaneChanged();

  // Let the superclass do its thing
  return Module::deserialize(ns);
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
  m_ignoreSignals = false;
}

void ModuleSlice::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  m_widget->SetDisplayOffset(pos);
}

//-----------------------------------------------------------------------------
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
}
