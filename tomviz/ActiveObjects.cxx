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
#include "Pipeline.h"
#include "Utilities.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPipelineSource.h>
#include <pqServer.h>
#include <pqView.h>

#include <vtkNew.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include <functional>

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

ActiveObjects::~ActiveObjects() = default;

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

    // Setting to nullptr so the traverse logic is re-run.
    m_activeParentDataSource = nullptr;
  }
  emit dataSourceActivated(m_activeDataSource);

  if (!m_activeDataSource.isNull() &&
      m_activeDataSource->pipeline() != nullptr) {
    setActiveTransformedDataSource(
      m_activeDataSource->pipeline()->transformedDataSource());
  }
}

void ActiveObjects::setSelectedDataSource(DataSource* source)
{
  m_selectedDataSource = source;

  if (!m_selectedDataSource.isNull()) {
    setActiveDataSource(m_selectedDataSource);
  }
}

void ActiveObjects::setActiveTransformedDataSource(DataSource* source)
{
  if (m_activeTransformedDataSource != source) {
    m_activeTransformedDataSource = source;
  }
  emit transformedDataSourceActivated(m_activeTransformedDataSource);
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
  if (module) {
    setActiveView(module->view());
    setActiveDataSource(module->dataSource());
  }
  if (m_activeModule != module) {
    m_activeModule = module;
    emit moduleChanged(module);
  }
  emit moduleActivated(module);
}

void ActiveObjects::setActiveOperator(Operator* op)
{
  if (op) {
    setActiveDataSource(op->dataSource());
  }
  if (m_activeOperator != op) {
    m_activeOperator = op;
    emit operatorChanged(op);
  }
  emit operatorActivated(op);
}

void ActiveObjects::setActiveOperatorResult(OperatorResult* result)
{
  if (result) {
    auto op = qobject_cast<Operator*>(result->parent());
    if (op) {
      setActiveOperator(op);
      setActiveDataSource(op->dataSource());
    }
  }
  if (m_activeOperatorResult != result) {
    m_activeOperatorResult = result;
    emit resultChanged(result);
  }
  emit resultActivated(result);
}

void ActiveObjects::createRenderViewIfNeeded()
{
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(proxyManager());
  iter->SetModeToOneGroup();
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    vtkSMRenderViewProxy* renderView =
      vtkSMRenderViewProxy::SafeDownCast(iter->GetProxy());
    if (renderView) {
      return;
    }
  }

  // If we get here, there was no existing view, so create one.
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  pqServer* server = pqApplicationCore::instance()->getActiveServer();
  builder->createView("RenderView", server);
}

void ActiveObjects::setActiveViewToFirstRenderView()
{
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(proxyManager());
  iter->SetModeToOneGroup();
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    vtkSMRenderViewProxy* renderView =
      vtkSMRenderViewProxy::SafeDownCast(iter->GetProxy());
    if (renderView) {
      setActiveView(renderView);
      break;
    }
  }
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

DataSource* ActiveObjects::activeParentDataSource()
{

  if (m_activeParentDataSource == nullptr) {
    auto pipeline = activePipeline();
    auto dataSource = activeDataSource();

    if (dataSource == nullptr) {
      return nullptr;
    }

    if (dataSource->forkable()) {
      return dataSource;
    }

    std::function<QList<DataSource*>(DataSource*, DataSource*,
                                     QList<DataSource*>)>
      dfs = [&dfs](DataSource* currentDataSource, DataSource* targetDataSource,
                   QList<DataSource*> path) {
        path.append(currentDataSource);
        if (currentDataSource == targetDataSource) {
          return path;
        }

        foreach (Operator* op, currentDataSource->operators()) {
          if (op->childDataSource() != nullptr) {
            QList<DataSource*> p =
              dfs(op->childDataSource(), targetDataSource, path);
            if (!p.isEmpty()) {
              return p;
            }
          }
        }

        return QList<DataSource*>();
      };

    // Find path to the active datasource
    auto path = dfs(pipeline->dataSource(), dataSource, QList<DataSource*>());

    // Return the first non output data source
    for (auto itr = path.rbegin(); itr != path.rend(); ++itr) {
      auto ds = *itr;
      if (ds->forkable()) {
        m_activeParentDataSource = ds;
        break;
      }
    }
  }

  return m_activeParentDataSource;
}

Pipeline* ActiveObjects::activePipeline() const
{

  if (m_activeDataSource != nullptr) {
    return m_activeDataSource->pipeline();
  }

  return nullptr;
}

} // end of namespace tomviz
