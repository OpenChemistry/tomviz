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
#include "ResetReaction.h"

#include "ActiveObjects.h"
#include "ModuleManager.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMProxy.h"
#include "vtkSMSessionProxyManager.h"

namespace TEM
{
//-----------------------------------------------------------------------------
ResetReaction::ResetReaction(QAction* parentObject)
  : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ResetReaction::~ResetReaction()
{
}

//-----------------------------------------------------------------------------
void ResetReaction::reset()
{
  ModuleManager::instance().reset();

  // create default render view.
  vtkNew<vtkSMParaViewPipelineController> controller;
  vtkSmartPointer<vtkSMProxy> view;
  view.TakeReference(ActiveObjects::instance().proxyManager()->NewProxy(
      "views", "RenderView"));
  controller->InitializeProxy(view);
  controller->RegisterViewProxy(view);
}

}
