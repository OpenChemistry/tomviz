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
#include "ModuleOrthogonalSlice.h"

#include "pqProxiesWidget.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

namespace TEM
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
  return QIcon(":/pqWidgets/Icons/pqSlice24.png");
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = dataSource->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  this->PassThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->PassThrough);
  controller->PreInitializeProxy(this->PassThrough);
  vtkSMPropertyHelper(this->PassThrough, "Input").Set(dataSource);
  controller->PostInitializeProxy(this->PassThrough);
  controller->RegisterPipelineProxy(this->PassThrough);

  // Create the representation for it.
  this->Representation = controller->Show(this->PassThrough, 0, view);
  Q_ASSERT(this->Representation);
  vtkSMRepresentationProxy::SetRepresentationType(this->Representation,
    "Slice");
  this->Representation->UpdateVTKObjects();
  return true;
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

  vtkSMProxy* lut = vtkSMPropertyHelper(this->Representation, "LookupTable").GetAsProxy();
  Q_ASSERT(lut);

  QStringList list;
  list
    << "Mapping Data"
    << "EnableOpacityMapping"
    << "RGBPoints"
    << "ScalarOpacityFunction"
    << "UseLogScale";
  panel->addProxy(lut, "Color Map", list, true);
  this->Superclass::addToPanel(panel);
}

}
