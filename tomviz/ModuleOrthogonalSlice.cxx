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
#include "ModuleOrthogonalSlice.h"

#include "DataSource.h"
#include "pqProxiesWidget.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

namespace tomviz
{
//-----------------------------------------------------------------------------
ModuleOrthogonalSlice::ModuleOrthogonalSlice(QObject* parentObject) : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleOrthogonalSlice::~ModuleOrthogonalSlice()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleOrthogonalSlice::icon() const
{
  return QIcon(":/icons/orthoslice.png");
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView))
  {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = data->producer()->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  this->PassThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->PassThrough);
  controller->PreInitializeProxy(this->PassThrough);
  vtkSMPropertyHelper(this->PassThrough, "Input").Set(data->producer());
  controller->PostInitializeProxy(this->PassThrough);
  controller->RegisterPipelineProxy(this->PassThrough);

  // Create the representation for it.
  this->Representation = controller->Show(this->PassThrough, 0, vtkView);
  Q_ASSERT(this->Representation);

  vtkSMRepresentationProxy::SetRepresentationType(this->Representation,
                                                  "Slice");

  // pick proper color/opacity maps.
  this->updateColorMap();
  this->Representation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
void ModuleOrthogonalSlice::updateColorMap()
{
  Q_ASSERT(this->Representation);

  vtkSMPropertyHelper(this->Representation,
                      "LookupTable").Set(this->colorMap());
  vtkSMPropertyHelper(this->Representation,
                      "ScalarOpacityFunction").Set(this->opacityMap());
  this->Representation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->Representation);
  controller->UnRegisterProxy(this->PassThrough);

  this->PassThrough = NULL;
  this->Representation = NULL;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::setVisibility(bool val)
{
  Q_ASSERT(this->Representation);
  vtkSMPropertyHelper(this->Representation, "Visibility").Set(val? 1 : 0);
  this->Representation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::visibility() const
{
  Q_ASSERT(this->Representation);
  return vtkSMPropertyHelper(this->Representation, "Visibility").GetAsInt() != 0;
}

//-----------------------------------------------------------------------------
void ModuleOrthogonalSlice::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(this->Representation);

  QStringList reprProperties;
  reprProperties
    << "SliceMode"
    << "Slice";
  panel->addProxy(this->Representation, "Slice", reprProperties, true);
  this->Superclass::addToPanel(panel);
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::serialize(pugi::xml_node& ns) const
{
  QStringList reprProperties;
  reprProperties
    << "SliceMode"
    << "Slice"
    << "Visibility";
  pugi::xml_node nodeR = ns.append_child("Representation");
  return (tomviz::serialize(this->Representation, nodeR, reprProperties) &&
    this->Superclass::serialize(ns));
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::deserialize(const pugi::xml_node& ns)
{
  if (!tomviz::deserialize(this->Representation, ns.child("Representation")))
  {
    return false;
  }
  return this->Superclass::deserialize(ns);
}

}
