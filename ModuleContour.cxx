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
#include "ModuleContour.h"

#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include "vtkAccelContour.h"
#include "vtkAlgorithm.h"

#include <QDebug>

namespace TEM
{

//-----------------------------------------------------------------------------
ModuleContour::ModuleContour(QObject* parentObject) :Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleContour::~ModuleContour()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleContour::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqIsosurface24.png");
}

//-----------------------------------------------------------------------------
bool ModuleContour::initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  //get the input algorithm and connect it to the accelerated contour
  vtkAlgorithm* input = vtkAlgorithm::SafeDownCast(dataSource->GetClientSideObject());

  vtkNew<vtkAccelContour> accContour;
  accContour->SetInputConnection( input->GetOutputPort() );
  accContour->Update();


  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  vtkSMSessionProxyManager* pxm = dataSource->GetSessionProxyManager();


  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "Contour"));

  this->ContourFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->ContourFilter);
  controller->PreInitializeProxy(this->ContourFilter);
  vtkSMPropertyHelper(this->ContourFilter, "Input").Set(dataSource);
  controller->PostInitializeProxy(this->ContourFilter);
  controller->RegisterPipelineProxy(this->ContourFilter);

  // Create the representation for it.
  this->ContourRepresentation = controller->Show(this->ContourFilter, 0, view);
  Q_ASSERT(this->ContourRepresentation);
  vtkSMPropertyHelper(this->ContourRepresentation, "Representation").Set("Surface");
  this->ContourRepresentation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleContour::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->ContourRepresentation);
  controller->UnRegisterProxy(this->ContourFilter);
  this->ContourFilter = NULL;
  this->ContourRepresentation = NULL;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleContour::setVisibility(bool val)
{
  vtkSMPropertyHelper(this->ContourRepresentation, "Visibility").Set(val? 1 : 0);
  this->ContourRepresentation->UpdateVTKObjects();
  return true;
}

} // end of namespace TEM
