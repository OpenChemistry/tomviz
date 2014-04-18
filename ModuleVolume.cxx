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
#include "ModuleVolume.h"

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
ModuleVolume::ModuleVolume(QObject* parentObject) :Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleVolume::~ModuleVolume()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleVolume::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqVolumeData16.png");
}

//-----------------------------------------------------------------------------
bool ModuleVolume::initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(dataSource, view))
    {
    return false;
    }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  // Create the representation for it.
  this->Representation = controller->Show(dataSource, 0, view);
  Q_ASSERT(this->Representation);
  vtkSMPropertyHelper(this->Representation,
                      "Representation").Set("Volume");
  this->Representation->UpdateVTKObjects();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleVolume::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->Representation);
  this->Representation = NULL;
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleVolume::setVisibility(bool val)
{
  vtkSMPropertyHelper(this->Representation, "Visibility").Set(val? 1 : 0);
  this->Representation->UpdateVTKObjects();
  return true;
}

} // end of namespace TEM
