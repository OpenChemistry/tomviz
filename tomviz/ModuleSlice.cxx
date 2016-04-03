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
#include "pqProxiesWidget.h"
#include "pqCoreUtilities.h"
#include "Utilities.h"

#include "vtkAlgorithm.h"
#include "vtkCommand.h"
#include "vtkNonOrthoImagePlaneWidget.h"
#include "vtkDataObject.h"
#include "vtkNew.h"
#include "vtkProperty.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkScalarsToColors.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTransferFunctionManager.h"
#include "vtkSMTransferFunctionProxy.h"
#include "vtkSMViewProxy.h"

#include <QDebug>

namespace tomviz
{

ModuleSlice::ModuleSlice(QObject* parentObject)
  : Superclass(parentObject)
  , IgnoreSignals(false)
{
}

ModuleSlice::~ModuleSlice()
{
  this->finalize();
}

QIcon ModuleSlice::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqSlice24.png");
}

bool ModuleSlice::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView))
  {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSourceProxy* producer = data->producer();
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));
  
  // Create the Properties panel proxy
  this->PropsPanelProxy.TakeReference(
      pxm->NewProxy("tomviz_proxies", "NonOrthogonalSlice"));

  pqCoreUtilities::connect(
    this->PropsPanelProxy, vtkCommand::PropertyModifiedEvent,
    this, SLOT(onPropertyChanged()));

  this->PassThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->PassThrough);
  controller->PreInitializeProxy(this->PassThrough);
  vtkSMPropertyHelper(this->PassThrough, "Input").Set(producer);
  controller->PostInitializeProxy(this->PassThrough);
  controller->RegisterPipelineProxy(this->PassThrough);

  const bool widgetSetup = this->setupWidget(vtkView, producer);

  if(widgetSetup)
  {
    this->Widget->On();
    this->Widget->InteractionOn();
    this->Widget->SetDisplayOffset(data->displayPosition());
    pqCoreUtilities::connect(
      this->Widget, vtkCommand::InteractionEvent,
      this, SLOT(onPlaneChanged()));
  }

  Q_ASSERT(this->Widget);
  return widgetSetup;
}

//  Should only be called from initialize after the PassThrough has been setup
bool ModuleSlice::setupWidget(vtkSMViewProxy* vtkView, vtkSMSourceProxy* producer)
{
  vtkAlgorithm* passThroughAlg = vtkAlgorithm::SafeDownCast(
                                 this->PassThrough->GetClientSideObject());

  vtkRenderWindowInteractor* rwi = vtkView->GetRenderWindow()->GetInteractor();

  // Determine the name of the property we are coloring by
  const char* propertyName = producer->GetDataInformation()->
                                       GetPointDataInformation()->
                                       GetArrayInformation(0)->
                                       GetName();

  if(!rwi||!passThroughAlg||!propertyName)
  {
    return false;
  }

  this->Widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();

  // Set the interactor on the widget to be what the current
  // render window is using.
  this->Widget->SetInteractor( rwi );

  // Setup the color of the border of the widget.
  {
  double color[3] = {1, 0, 0};
  this->Widget->GetPlaneProperty()->SetColor(color);
  }

  // Turn texture interpolation to be linear.
  this->Widget->TextureInterpolateOn();
  this->Widget->SetResliceInterpolateToLinear();

  // Construct the transfer function proxy for the widget.
  vtkSMProxy* lut = this->colorMap();

  // Set the widgets lookup table to be the one that the transfer function
  // manager is using.
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  this->Widget->SetLookupTable(stc);

  // Lastly we set up the input connection.
  this->Widget->SetInputConnection(passThroughAlg->GetOutputPort());

  Q_ASSERT(rwi);
  Q_ASSERT(passThroughAlg);
  onPlaneChanged();
  return true;
}

void ModuleSlice::updateColorMap()
{
  Q_ASSERT(this->Widget);

  //Construct the transfer function proxy for the widget
  vtkSMProxy* lut = this->colorMap();

  //set the widgets lookup table to be the one that the transfer function
  //manager is using
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  this->Widget->SetLookupTable(stc);
}

bool ModuleSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->PassThrough);

  this->PassThrough = nullptr;

  if(this->Widget != nullptr)
  {
    this->Widget->InteractionOff();
    this->Widget->Off();
  }

  return true;
}

bool ModuleSlice::setVisibility(bool val)
{
  Q_ASSERT(this->Widget);
  this->Widget->SetEnabled(val ? 1 : 0);
  // update the state of the arrow as well since it cannot update when the
  // widget is not enabled
  if (val)
  {
    vtkSMPropertyHelper showProperty(this->PropsPanelProxy, "ShowArrow");
    // Not this: it hides the plane as well as the arrow...
    //this->Widget->SetEnabled(showProperty.GetAsInt());
    this->Widget->SetArrowVisibility(showProperty.GetAsInt());
    this->Widget->SetInteraction(showProperty.GetAsInt());
  }
  return true;
}

bool ModuleSlice::visibility() const
{
  Q_ASSERT(this->Widget);
  return this->Widget->GetEnabled() != 0;
}

void ModuleSlice::addToPanel(pqProxiesWidget* panel)
{
  QStringList properties;
  properties << "ShowArrow" << "PointOnPlane" << "PlaneNormal";
  panel->addProxy(this->PropsPanelProxy, "Appearance", properties, true);

  this->Superclass::addToPanel(panel);
}

bool ModuleSlice::serialize(pugi::xml_node& ns) const
{
  // Save the state of the arrow's visibility
  vtkSMPropertyHelper showProperty(this->PropsPanelProxy, "ShowArrow");
  ns.append_attribute("show_arrow").set_value(showProperty.GetAsInt());

  // Serialize the plane
  double point[3];
  pugi::xml_node plane = ns.append_child("Plane");
  pugi::xml_node pointNode;
  this->Widget->GetOrigin(point);
  // Origin of plane
  pointNode = plane.append_child("Point");
  pointNode.append_attribute("index").set_value(0);
  pointNode.append_attribute("x").set_value(point[0]);
  pointNode.append_attribute("y").set_value(point[1]);
  pointNode.append_attribute("z").set_value(point[2]);
  // Point 1 of plane
  this->Widget->GetPoint1(point);
  pointNode = plane.append_child("Point");
  pointNode.append_attribute("index").set_value(1);
  pointNode.append_attribute("x").set_value(point[0]);
  pointNode.append_attribute("y").set_value(point[1]);
  pointNode.append_attribute("z").set_value(point[2]);
  // Point 2 of plane
  this->Widget->GetPoint2(point);
  pointNode = plane.append_child("Point");
  pointNode.append_attribute("index").set_value(2);
  pointNode.append_attribute("x").set_value(point[0]);
  pointNode.append_attribute("y").set_value(point[1]);
  pointNode.append_attribute("z").set_value(point[2]);

  // Let the superclass do its thing
  return this->Superclass::serialize(ns);
}

bool ModuleSlice::deserialize(const pugi::xml_node& ns)
{

  pugi::xml_node plane = ns.child("Plane");
  if (!plane)
  {
    // We are reading an older state file from before the change that added
    // the ability to save these...
    return this->Superclass::deserialize(ns);
  }
  // Deserialize the show arrow state
  vtkSMPropertyHelper showProperty(this->PropsPanelProxy, "ShowArrow");
  showProperty.Set(ns.attribute("show_arrow").as_int());


  // Deserialize the plane
  double point[3];
  for (pugi::xml_node pointNode = plane.child("Point"); pointNode;
         pointNode = pointNode.next_sibling("Point"))
  {
    point[0] = pointNode.attribute("x").as_double();
    point[1] = pointNode.attribute("y").as_double();
    point[2] = pointNode.attribute("z").as_double();
    int index = pointNode.attribute("index").as_int();
    if (index == 0)
    {
      this->Widget->SetOrigin(point);
    }
    else if (index == 1)
    {
      this->Widget->SetPoint1(point);
    }
    else if (index == 2)
    {
      this->Widget->SetPoint2(point);
    }
    else
    {
      qCritical("Unknown point index for slice plane point");
      return false;
    }
  }
  this->Widget->UpdatePlacement();

  // Let the superclass do its thing
  return this->Superclass::deserialize(ns);
}

void ModuleSlice::onPropertyChanged()
{
  // Avoid recursive clobbering of the plane position
  if (this->IgnoreSignals)
  {
    return;
  }
  this->IgnoreSignals = true;
  vtkSMPropertyHelper showProperty(this->PropsPanelProxy, "ShowArrow");
  if (this->Widget->GetEnabled())
  {
    // Not this: it hides the plane as well as the arrow...
    //this->Widget->SetEnabled(showProperty.GetAsInt());
    this->Widget->SetArrowVisibility(showProperty.GetAsInt());
    this->Widget->SetInteraction(showProperty.GetAsInt());
  }
  vtkSMPropertyHelper pointProperty(this->PropsPanelProxy, "PointOnPlane");
  std::vector<double> centerPoint = pointProperty.GetDoubleArray();
  this->Widget->SetCenter(&centerPoint[0]);
  vtkSMPropertyHelper normalProperty(this->PropsPanelProxy, "PlaneNormal");
  std::vector<double> normalVector = normalProperty.GetDoubleArray();
  this->Widget->SetNormal(&normalVector[0]);
  this->Widget->UpdatePlacement();
  this->IgnoreSignals = false;
}

void ModuleSlice::onPlaneChanged()
{
  // Avoid recursive clobbering of the plane position
  if (this->IgnoreSignals)
  {
    return;
  }
  this->IgnoreSignals = true;
  vtkSMPropertyHelper pointProperty(this->PropsPanelProxy, "PointOnPlane");
  double *centerPoint = this->Widget->GetCenter();
  pointProperty.Set(centerPoint, 3);
  vtkSMPropertyHelper normalProperty(this->PropsPanelProxy, "PlaneNormal");
  double *normalVector = this->Widget->GetNormal();
  normalProperty.Set(normalVector, 3);
  this->IgnoreSignals = false;
}

void ModuleSlice::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  this->Widget->SetDisplayOffset(pos);
}

}
