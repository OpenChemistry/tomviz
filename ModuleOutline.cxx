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
#include "ModuleOutline.h"

#include "pqProxiesWidget.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

namespace TEM
{

//-----------------------------------------------------------------------------
ModuleOutline::ModuleOutline(QObject* parentObject) : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleOutline::~ModuleOutline()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleOutline::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqProbeLocation24.png");
}

//-----------------------------------------------------------------------------
bool ModuleOutline::initialize(vtkSMSourceProxy* dataSource,
                               vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = dataSource->GetSessionProxyManager();

  // Create the outline filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "OutlineFilter"));

  this->OutlineFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->OutlineFilter);
  controller->PreInitializeProxy(this->OutlineFilter);
  vtkSMPropertyHelper(this->OutlineFilter, "Input").Set(dataSource);
  controller->PostInitializeProxy(this->OutlineFilter);
  controller->RegisterPipelineProxy(this->OutlineFilter);

  // Create the representation for it.
  this->OutlineRepresentation = controller->Show(this->OutlineFilter, 0, view);
  Q_ASSERT(this->OutlineRepresentation);
  vtkSMPropertyHelper(this->OutlineRepresentation,
                      "Representation").Set("Outline");
  this->OutlineRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleOutline::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->OutlineRepresentation);
  controller->UnRegisterProxy(this->OutlineFilter);

  this->OutlineFilter = NULL;
  this->OutlineRepresentation = NULL;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleOutline::setVisibility(bool val)
{
  Q_ASSERT(this->OutlineRepresentation);
  vtkSMPropertyHelper(this->OutlineRepresentation, "Visibility").Set(val? 1 : 0);
  this->OutlineRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleOutline::visibility() const
{
  Q_ASSERT(this->OutlineRepresentation);
  return vtkSMPropertyHelper(this->OutlineRepresentation, "Visibility").GetAsInt() != 0;
}

//-----------------------------------------------------------------------------
void ModuleOutline::addToPanel(pqProxiesWidget* panel)
{
  Q_ASSERT(panel && this->OutlineRepresentation);
  QStringList properties;
  properties << "CubeAxesVisibility";
  panel->addProxy(
    this->OutlineRepresentation, "Annotations", properties, true);
  this->Superclass::addToPanel(panel);
}

} // end of namespace TEM
