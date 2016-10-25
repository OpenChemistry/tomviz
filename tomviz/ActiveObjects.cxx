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

ActiveObjects::ActiveObjects() : QObject()
{
  connect(&pqActiveObjects::instance(), SIGNAL(viewChanged(pqView*)),
          SLOT(viewChanged(pqView*)));
  connect(&ModuleManager::instance(), SIGNAL(dataSourceRemoved(DataSource*)),
          SLOT(dataSourceRemoved(DataSource*)));
  connect(&ModuleManager::instance(), SIGNAL(moduleRemoved(Module*)),
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
  emit viewChanged(view ? view->getViewProxy() : nullptr);
}

void ActiveObjects::dataSourceRemoved(DataSource* ds)
{
  if (m_activeDataSource == ds) {
    setActiveDataSource(nullptr);
  }
}

void ActiveObjects::moduleRemoved(Module* mdl)
{
  if (m_activeModule == mdl) {
    setActiveModule(nullptr);
  }
}

void ActiveObjects::setActiveDataSource(DataSource* source)
{
  if (m_activeDataSource != source) {
    if (m_activeDataSource) {
      disconnect(m_activeDataSource, SIGNAL(dataChanged()), this,
                 SLOT(dataSourceChanged()));
    }
    if (source) {
      connect(source, SIGNAL(dataChanged()), this, SLOT(dataSourceChanged()));
      m_activeDataSourceType = source->type();
    }
    m_activeDataSource = source;
    emit dataSourceChanged(m_activeDataSource);
  }
  emit dataSourceActivated(m_activeDataSource);
}

void ActiveObjects::dataSourceChanged()
{
  if (m_activeDataSource->type() != m_activeDataSourceType) {
    m_activeDataSourceType = m_activeDataSource->type();
    emit dataSourceChanged(m_activeDataSource);
  }
}

vtkSMSessionProxyManager* ActiveObjects::proxyManager() const
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  return server ? server->proxyManager() : nullptr;
}

void ActiveObjects::setActiveModule(Module* module)
{
  if (m_activeModule != module) {
    m_activeModule = module;
    if (module) {
      setActiveView(module->view());
      setActiveDataSource(module->dataSource());
    }
    emit moduleChanged(module);
  }
  emit moduleActivated(module);
}

void ActiveObjects::setMoveObjectsMode(bool moveObjectsOn)
{
  if (m_moveObjectsEnabled != moveObjectsOn) {
    m_moveObjectsEnabled = moveObjectsOn;
    emit moveObjectsModeChanged(moveObjectsOn);
  }
}

void ActiveObjects::renderAllViews()
{
  pqApplicationCore::instance()->render();
}

} // end of namespace tomviz
