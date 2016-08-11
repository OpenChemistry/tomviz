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
#include "ActiveObjects.h"
#include "ModuleManager.h"
#include "Utilities.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqPipelineSource.h>
#include <pqServer.h>
#include <pqView.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

namespace tomviz {

ActiveObjects::ActiveObjects()
  : Superclass(), ActiveDataSource(nullptr), VoidActiveDataSource(nullptr),
    ActiveDataSourceType(DataSource::Volume), ActiveModule(nullptr),
    VoidActiveModule(nullptr), MoveObjectsEnabled(false)
{
  this->connect(&pqActiveObjects::instance(), SIGNAL(viewChanged(pqView*)),
                SLOT(viewChanged(pqView*)));
  this->connect(&ModuleManager::instance(),
                SIGNAL(dataSourceRemoved(DataSource*)),
                SLOT(dataSourceRemoved(DataSource*)));
  this->connect(&ModuleManager::instance(), SIGNAL(moduleRemoved(Module*)),
                SLOT(moduleRemoved(Module*)));
}

ActiveObjects::~ActiveObjects()
{
}

ActiveObjects& ActiveObjects::instance()
{
  static ActiveObjects theInstance;
  return theInstance;
}

void ActiveObjects::setActiveView(vtkSMViewProxy* view)
{
  pqActiveObjects::instance().setActiveView(tomviz::convert<pqView*>(view));
}

vtkSMViewProxy* ActiveObjects::activeView() const
{
  pqView* view = pqActiveObjects::instance().activeView();
  return view ? view->getViewProxy() : nullptr;
}

void ActiveObjects::viewChanged(pqView* view)
{
  emit this->viewChanged(view ? view->getViewProxy() : nullptr);
}

void ActiveObjects::dataSourceRemoved(DataSource* ds)
{
  if (this->VoidActiveDataSource == ds) {
    this->setActiveDataSource(nullptr);
  }
}

void ActiveObjects::moduleRemoved(Module* mdl)
{
  if (this->VoidActiveModule == mdl) {
    this->setActiveModule(nullptr);
  }
}

void ActiveObjects::setActiveDataSource(DataSource* source)
{
  if (this->VoidActiveDataSource != source) {
    if (this->ActiveDataSource) {
      QObject::disconnect(this->ActiveDataSource, SIGNAL(dataChanged()), this,
                          SLOT(dataSourceChanged()));
    }
    if (source) {
      QObject::connect(source, SIGNAL(dataChanged()), this,
                       SLOT(dataSourceChanged()));
      this->ActiveDataSourceType = source->type();
    }
    this->ActiveDataSource = source;
    this->VoidActiveDataSource = source;
    emit this->dataSourceChanged(this->ActiveDataSource);
  }
  emit this->dataSourceActivated(this->ActiveDataSource);
}

void ActiveObjects::dataSourceChanged()
{
  if (this->ActiveDataSource->type() != this->ActiveDataSourceType) {
    this->ActiveDataSourceType = this->ActiveDataSource->type();
    emit this->dataSourceChanged(this->ActiveDataSource);
  }
}

vtkSMSessionProxyManager* ActiveObjects::proxyManager() const
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  return server ? server->proxyManager() : nullptr;
}

void ActiveObjects::setActiveModule(Module* module)
{
  if (this->VoidActiveModule != module) {
    this->VoidActiveModule = module;
    this->ActiveModule = module;
    if (module) {
      this->setActiveView(module->view());
      this->setActiveDataSource(module->dataSource());
    }
    emit this->moduleChanged(module);
  }
  emit this->moduleActivated(module);
}

void ActiveObjects::setMoveObjectsMode(bool moveObjectsOn)
{
  if (this->MoveObjectsEnabled != moveObjectsOn) {
    this->MoveObjectsEnabled = moveObjectsOn;
    emit this->moveObjectsModeChanged(moveObjectsOn);
  }
}

void ActiveObjects::renderAllViews()
{
  pqApplicationCore::instance()->render();
}

} // end of namespace tomviz
