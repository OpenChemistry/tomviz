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
#include "DeleteDataReaction.h"

#include "ActiveObjects.h"
#include "ModuleManager.h"
#include "Pipeline.h"

namespace tomviz {

DeleteDataReaction::DeleteDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(activeDataSourceChanged()));
  m_activeDataSource = ActiveObjects::instance().activeDataSource();
  updateEnableState();
}

void DeleteDataReaction::updateEnableState()
{
  bool enabled = (m_activeDataSource != nullptr);
  if (enabled) {
    enabled = m_activeDataSource->pipeline() &&
              !m_activeDataSource->pipeline()->isRunning();
  }
  parentAction()->setEnabled(enabled);
}

void DeleteDataReaction::onTriggered()
{
  auto source = ActiveObjects::instance().activeDataSource();
  Q_ASSERT(source);
  DeleteDataReaction::deleteDataSource(source);
  ActiveObjects::instance().renderAllViews();
}

void DeleteDataReaction::deleteDataSource(DataSource* source)
{
  Q_ASSERT(source);

  auto& mmgr = ModuleManager::instance();
  mmgr.removeAllModules(source);
  mmgr.removeDataSource(source);
}

void DeleteDataReaction::activeDataSourceChanged()
{
  auto source = ActiveObjects::instance().activeDataSource();
  if (m_activeDataSource != source) {
    if (m_activeDataSource && m_activeDataSource.data()->pipeline()) {
      disconnect(m_activeDataSource.data()->pipeline(), &Pipeline::started,
                 this, nullptr);
      disconnect(m_activeDataSource.data()->pipeline(), &Pipeline::finished,
                 this, nullptr);
    }
    m_activeDataSource = source;
    if (m_activeDataSource && m_activeDataSource.data()->pipeline()) {
      connect(m_activeDataSource.data()->pipeline(), &Pipeline::started, this,
              &DeleteDataReaction::updateEnableState);
      connect(m_activeDataSource.data()->pipeline(), &Pipeline::finished, this,
              &DeleteDataReaction::updateEnableState);
    }
  }
  updateEnableState();
}

} // end of namespace tomviz
