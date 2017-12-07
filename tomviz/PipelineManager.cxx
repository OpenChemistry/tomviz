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

class PipelineManager::PMInternals
{
public:
  QList<QPointer<Pipeline>> Pipelines;
};

PipelineManager::PipelineManager(QObject* parentObject)
  : Superclass(parentObject), Internals(new PipelineManager::PMInternals())
{
}

PipelineManager::~PipelineManager()
{
  // Internals is a QScopedPointer.
}

PipelineManager& PipelineManager::instance()
{
  static PipelineManager theInstance;
  return theInstance;
}

void PipelineManager::addPipeline(Pipeline* pipeline)
{
  if (pipeline && !this->Internals->Pipelines.contains(pipeline)) {
    pipeline->setParent(this);
    this->Internals->Pipelines.push_back(pipeline);
  }
}

void PipelineManager::removePipeline(Pipeline* pipeline)
{
  if (this->Internals->Pipelines.removeOne(pipeline)) {
    pipeline->deleteLater();
  }
}

void PipelineManager::removeAllPipelines()
{
  foreach (Pipeline* pipeline, this->Internals->Pipelines) {
    pipeline->deleteLater();
  }
  this->Internals->Pipelines.clear();
}

} // end of namespace tomviz
