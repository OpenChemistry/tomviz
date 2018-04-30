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
#include "PipelineManager.h"

#include "ActiveObjects.h"
#include "Pipeline.h"

namespace tomviz {

PipelineManager::PipelineManager(QObject* p) : QObject(p) {}

PipelineManager::~PipelineManager() = default;

PipelineManager& PipelineManager::instance()
{
  static PipelineManager theInstance;
  return theInstance;
}

void PipelineManager::addPipeline(Pipeline* pipeline)
{
  if (pipeline && !m_pipelines.contains(pipeline)) {
    pipeline->setParent(this);
    m_pipelines.push_back(pipeline);
  }
}

void PipelineManager::removePipeline(Pipeline* pipeline)
{
  if (m_pipelines.removeOne(pipeline)) {
    pipeline->deleteLater();
  }
}

void PipelineManager::removeAllPipelines()
{
  foreach (Pipeline* pipeline, m_pipelines) {
    pipeline->deleteLater();
  }
  m_pipelines.clear();
}

} // end of namespace tomviz
