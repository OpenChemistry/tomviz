/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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

void PipelineManager::updateExecutionMode(Pipeline::ExecutionMode mode)
{
  emit executionModeUpdated(mode);
}

void PipelineManager::addPipeline(Pipeline* pipeline)
{
  if (pipeline && !m_pipelines.contains(pipeline)) {
    pipeline->setParent(this);
    m_pipelines.push_back(pipeline);

    connect(this, &PipelineManager::executionModeUpdated, pipeline,
            &Pipeline::setExecutionMode);
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
