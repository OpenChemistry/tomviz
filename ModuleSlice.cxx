/******************************************************************************

  This source file is part of the TEM tomography project.

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
#include "Utilities.h"

#include "vtkAlgorithm.h"
#include "vtkColorImagePlaneWidget.h"
#include "vtkNew.h"
#include "vtkProperty.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTransferFunctionManager.h"
#include "vtkSMViewProxy.h"
#include "vtkPVArrayInformation.h"
#include "vtkScalarsToColors.h"

namespace TEM
{

//-----------------------------------------------------------------------------
ModuleSlice::ModuleSlice(QObject* parentObject) : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleSlice::~ModuleSlice()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleSlice::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqSlice24.png");
}

//-----------------------------------------------------------------------------
bool ModuleSlice::initialize(DataSource* dataSource, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSourceProxy* producer = dataSource->producer();
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  this->PassThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->PassThrough);
  controller->PreInitializeProxy(this->PassThrough);
  vtkSMPropertyHelper(this->PassThrough, "Input").Set(producer);
  controller->PostInitializeProxy(this->PassThrough);
  controller->RegisterPipelineProxy(this->PassThrough);

  //Create the widget
  const bool widgetSetup = this->setupWidget(view,producer);

  if(widgetSetup)
    {
    this->Widget->On();
    this->Widget->InteractionOn();
    }

  Q_ASSERT(this->Widget);
  Q_ASSERT(rwi);
  Q_ASSERT(passThrough);
  return widgetSetup;
}


//-----------------------------------------------------------------------------
//should only be called from initialize after the PassThrough has been setup
bool ModuleSlice::setupWidget(vtkSMViewProxy* view, vtkSMSourceProxy* producer)
{
  vtkSMSessionProxyManager* pxm = producer->GetSessionProxyManager();
  vtkAlgorithm* passThroughAlg = vtkAlgorithm::SafeDownCast(
                                 this->PassThrough->GetClientSideObject());

  vtkRenderWindowInteractor* rwi = view->GetRenderWindow()->GetInteractor();

  //determine the name of the property we are coloring by
  const char* propertyName = producer->GetDataInformation()->
                                       GetPointDataInformation()->
                                       GetArrayInformation(0)->
                                       GetName();

  if(!rwi||!passThroughAlg||!propertyName)
    {
    return false;
    }

  this->Widget = vtkSmartPointer<vtkColorImagePlaneWidget>::New();

  //set the interactor on the widget to be what the current
  //render window is using
  this->Widget->SetInteractor( rwi );

  //setup the widget to keep the plane inside the volume
  this->Widget->RestrictPlaneToVolumeOn();

  //setup the color of the border of the widget
  {
  double color[3] = {1, 0, 0};
  this->Widget->GetPlaneProperty()->SetColor(color);
  }

  //turn texture interpolation to be linear
  this->Widget->TextureInterpolateOn();
  this->Widget->SetResliceInterpolateToLinear();

  //setup the transfer function manager, so that we can color the output
  vtkNew<vtkSMTransferFunctionManager> tfm;
  this->TransferFunction = tfm->GetColorTransferFunction(propertyName,pxm);
  vtkScalarsToColors* stc = vtkScalarsToColors ::SafeDownCast(
                                this->TransferFunction->GetClientSideObject());
  this->Widget->SetLookupTable(stc);


  //lastly we set up the input connection
  this->Widget->SetInputConnection(passThroughAlg->GetOutputPort());

  return true;
}

//-----------------------------------------------------------------------------
bool ModuleSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->PassThrough);

  this->PassThrough = NULL;
  this->TransferFunction = NULL;

  if(this->Widget != NULL)
    {
    this->Widget->InteractionOff();
    this->Widget->Off();
    }

  return true;
}

//-----------------------------------------------------------------------------
bool ModuleSlice::setVisibility(bool val)
{
  Q_ASSERT(this->Widget);
  this->Widget->SetEnabled(val ? 1 : 0);
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleSlice::visibility() const
{
  Q_ASSERT(this->Widget);
  return this->Widget->GetEnabled() != 0;
}

//-----------------------------------------------------------------------------
void ModuleSlice::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(this->Widget);
  Q_ASSERT(this->TransferFunction);

  QStringList list;
  list
    << "Mapping Data"
    << "EnableOpacityMapping"
    << "RGBPoints"
    << "ScalarOpacityFunction"
    << "UseLogScale";
  panel->addProxy(this->TransferFunction, "Color Map", list, true);
  this->Superclass::addToPanel(panel);
}

//-----------------------------------------------------------------------------
bool ModuleSlice::serialize(pugi::xml_node& ns) const
{
  // vtkSMProxy* lut = vtkSMPropertyHelper(this->Representation, "LookupTable").GetAsProxy();
  // vtkSMProxy* sof = vtkSMPropertyHelper(this->Representation, "ScalarOpacityFunction").GetAsProxy();
  // Q_ASSERT(lut && sof);

  // QStringList reprProperties;
  // reprProperties
  //   << "SliceMode"
  //   << "Slice"
  //   << "Visibility";
  // pugi::xml_node nodeR = ns.append_child("Representation");
  // pugi::xml_node nodeL = ns.append_child("LookupTable");
  // pugi::xml_node nodeS = ns.append_child("ScalarOpacityFunction");
  // return (TEM::serialize(this->Representation, nodeR, reprProperties) &&
  //   TEM::serialize(lut, nodeL) &&
  //   TEM::serialize(sof, nodeS));
  return false;
}

//-----------------------------------------------------------------------------
bool ModuleSlice::deserialize(const pugi::xml_node& ns)
{
  return false;
  // vtkSMProxy* lut = vtkSMPropertyHelper(this->Representation, "LookupTable").GetAsProxy();
  // vtkSMProxy* sof = vtkSMPropertyHelper(this->Representation, "ScalarOpacityFunction").GetAsProxy();
  // if (TEM::deserialize(this->Representation, ns.child("Representation")))
  //   {
  //   vtkSMPropertyHelper(this->Representation, "ScalarOpacityFunction").Set(sof);
  //   this->Representation->UpdateVTKObjects();
  //   }
  // else
  //   {
  //   return false;
  //   }
  // if (TEM::deserialize(lut, ns.child("LookupTable")))
  //   {
  //   vtkSMPropertyHelper(lut, "ScalarOpacityFunction").Set(sof);
  //   lut->UpdateVTKObjects();
  //   }
  // else
  //   {
  //   return false;
  //   }
  // if (TEM::deserialize(sof, ns.child("ScalarOpacityFunction")))
  //   {
  //   sof->UpdateVTKObjects();
  //   }
  // else
  //   {
  //   return false;
  //   }
  // return true;
}

}
