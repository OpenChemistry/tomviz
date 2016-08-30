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
#include "ModuleVolume.h"

#include "DataSource.h"
#include "Utilities.h"
#include "pqProxiesWidget.h"
#include "vtkNew.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"

namespace tomviz {

ModuleVolume::ModuleVolume(QObject* parentObject) : Superclass(parentObject)
{
}

ModuleVolume::~ModuleVolume()
{
  this->finalize();
}

QIcon ModuleVolume::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqVolumeData16.png");
}

bool ModuleVolume::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
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
                                                  "Volume");
  vtkSMPropertyHelper(this->Representation, "Position")
    .Set(data->displayPosition(), 3);

  this->updateColorMap();
  this->Representation->UpdateVTKObjects();
  return true;
}

void ModuleVolume::updateColorMap()
{
  Q_ASSERT(this->Representation);
  vtkSMPropertyHelper(this->Representation, "LookupTable")
    .Set(this->colorMap());
  vtkSMPropertyHelper(this->Representation, "ScalarOpacityFunction")
    .Set(this->opacityMap());
  this->Representation->UpdateVTKObjects();

  // BUG: volume mappers don't update property when LUT is changed and has an
  // older Mtime. Fix for now by forcing the LUT to update.
  vtkObject::SafeDownCast(this->colorMap()->GetClientSideObject())->Modified();
}

bool ModuleVolume::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->Representation);
  controller->UnRegisterProxy(this->PassThrough);

  this->PassThrough = nullptr;
  this->Representation = nullptr;
  return true;
}

bool ModuleVolume::setVisibility(bool val)
{
  Q_ASSERT(this->Representation);
  vtkSMPropertyHelper(this->Representation, "Visibility").Set(val ? 1 : 0);
  this->Representation->UpdateVTKObjects();
  return true;
}

bool ModuleVolume::visibility() const
{
  Q_ASSERT(this->Representation);
  return vtkSMPropertyHelper(this->Representation, "Visibility").GetAsInt() !=
         0;
}

bool ModuleVolume::serialize(pugi::xml_node& ns) const
{
  QStringList list;
  list << "Visibility"
       << "ScalarOpacityUnitDistance";
  pugi::xml_node nodeR = ns.append_child("Representation");
  return (tomviz::serialize(this->Representation, nodeR, list) &&
          this->Superclass::serialize(ns));
}

bool ModuleVolume::deserialize(const pugi::xml_node& ns)
{
  if (!tomviz::deserialize(this->Representation, ns.child("Representation"))) {
    return false;
  }

  return this->Superclass::deserialize(ns);
}

void ModuleVolume::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(this->Representation, "Position").Set(pos, 3);
  this->Representation->UpdateVTKObjects();
}

//-----------------------------------------------------------------------------
bool ModuleVolume::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == this->PassThrough.Get()) ||
         (proxy == this->Representation.Get());
}

std::string ModuleVolume::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == this->PassThrough.Get()) {
    return "PassThrough";
  } else if (proxy == this->Representation.Get()) {
    return "Representation";
  } else {
    qWarning("Unknown proxy passed to module volume in save animation");
    return "";
  }
}

vtkSMProxy* ModuleVolume::getProxyForString(const std::string& str)
{
  if (str == "PassThrough") {
    return this->PassThrough.Get();
  } else if (str == "Representation") {
    return this->Representation.Get();
  } else {
    return nullptr;
  }
}

} // end of namespace tomviz
