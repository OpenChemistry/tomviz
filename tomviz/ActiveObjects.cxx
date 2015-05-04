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
#include "ActiveObjects.h"

#include "ModuleManager.h"
#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqPipelineSource.h"
#include "pqServer.h"
#include "pqView.h"
#include "Utilities.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

namespace TEM
{

//-----------------------------------------------------------------------------
ActiveObjects::ActiveObjects()
  : Superclass(),
    ActiveDataSource(NULL),
    VoidActiveDataSource(NULL),
    ActiveModule(NULL),
    VoidActiveModule(NULL)
{
  this->connect(&pqActiveObjects::instance(), SIGNAL(viewChanged(pqView*)),
                SLOT(viewChanged(pqView*)));
  this->connect(&ModuleManager::instance(),
                SIGNAL(dataSourceRemoved(DataSource*)),
                SLOT(dataSourceRemoved(DataSource*)));
  this->connect(&ModuleManager::instance(), SIGNAL(moduleRemoved(Module*)),
                SLOT(moduleRemoved(Module*)));
}

//-----------------------------------------------------------------------------
ActiveObjects::~ActiveObjects()
{
}

//-----------------------------------------------------------------------------
ActiveObjects& ActiveObjects::instance()
{
  static ActiveObjects theInstance;
  return theInstance;
}

//-----------------------------------------------------------------------------
void ActiveObjects::setActiveView(vtkSMViewProxy* view)
{
  pqActiveObjects::instance().setActiveView(TEM::convert<pqView*>(view));
}

//-----------------------------------------------------------------------------
vtkSMViewProxy* ActiveObjects::activeView() const
{
  pqView* view = pqActiveObjects::instance().activeView();
  return view ? view->getViewProxy() : NULL;
}

//-----------------------------------------------------------------------------
void ActiveObjects::viewChanged(pqView* view)
{
  emit this->viewChanged(view ? view->getViewProxy() : NULL);
}

//-----------------------------------------------------------------------------
void ActiveObjects::dataSourceRemoved(DataSource* ds)
{
  if (this->VoidActiveDataSource == ds)
    {
    this->setActiveDataSource(NULL);
    }
}

//-----------------------------------------------------------------------------
void ActiveObjects::moduleRemoved(Module* mdl)
{
  if (this->VoidActiveModule == mdl)
    {
    this->setActiveModule(NULL);
    }
}

//-----------------------------------------------------------------------------
void ActiveObjects::setActiveDataSource(DataSource* source)
{
  if (this->VoidActiveDataSource != source)
    {
    this->ActiveDataSource = source;
    this->VoidActiveDataSource = source;
    emit this->dataSourceChanged(this->ActiveDataSource);
    }
}

//-----------------------------------------------------------------------------
vtkSMSessionProxyManager* ActiveObjects::proxyManager() const
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  return server ? server->proxyManager() : NULL;
}

//-----------------------------------------------------------------------------
void ActiveObjects::setActiveModule(Module* module)
{
  if (this->VoidActiveModule != module)
    {
    this->VoidActiveModule = module;
    this->ActiveModule = module;
    if (module)
      {
      this->setActiveView(module->view());
      this->setActiveDataSource(module->dataSource());
      }
    emit this->moduleChanged(module);
    }
}

//-----------------------------------------------------------------------------
void ActiveObjects::renderAllViews()
{
  pqApplicationCore::instance()->render();
}


//-----------------------------------------------------------------------------
} // end of namespace TEM
