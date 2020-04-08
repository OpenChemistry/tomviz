/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineStateManager.h"

#include "core/PipelineProxyBase.h"
#include "core/PythonFactory.h"

using namespace tomviz;

PipelineStateManager::PipelineStateManager()
{
  m_proxy = PythonFactory::instance().createPipelineProxy();
}

void PipelineStateManager::syncToPython()
{
  m_proxy->syncToPython();
}

void PipelineStateManager::syncViewsToPython()
{
  m_proxy->syncViewsToPython();
}

std::string PipelineStateManager::serialize()
{
  return m_proxy->serialize();
}

void PipelineStateManager::load(const std::string& state,
                                const std::string& stateRelDir)
{
  m_proxy->load(state, stateRelDir);
}

std::string PipelineStateManager::modulesJson()
{
  return m_proxy->modulesJson();
}

std::string PipelineStateManager::operatorsJson()
{
  return m_proxy->operatorsJson();
}

std::string PipelineStateManager::serializeOperator(const std::string& path,
                                                    const std::string& id)
{
  return m_proxy->serializeOperator(path, id);
}

void PipelineStateManager::updateOperator(const std::string& path,
                                          const std::string& state)
{
  m_proxy->updateOperator(path, state);
}

std::string PipelineStateManager::serializeModule(const std::string& path,
                                                  const std::string& id)
{
  return m_proxy->serializeModule(path, id);
}

void PipelineStateManager::updateModule(const std::string& path,
                                        const std::string& state)
{
  m_proxy->updateModule(path, state);
}

std::string PipelineStateManager::serializeDataSource(const std::string& path,
                                                      const std::string& id)
{
  return m_proxy->serializeDataSource(path, id);
}

void PipelineStateManager::updateDataSource(const std::string& path,
                                            const std::string& state)
{
  m_proxy->updateDataSource(path, state);
}

void PipelineStateManager::modified(std::vector<std::string> opPaths,
                                    std::vector<std::string> modulePaths)
{
  m_proxy->modified(opPaths, modulePaths);
}

std::string PipelineStateManager::addModule(const std::string& dataSourcePath,
                                            const std::string& dataSourceId,
                                            const std::string& moduleType)
{
  return m_proxy->addModule(dataSourcePath, dataSourceId, moduleType);
}

std::string PipelineStateManager::addOperator(const std::string& dataSourcePath,
                                              const std::string& dataSourceId,
                                              const std::string& opState)
{
  return m_proxy->addOperator(dataSourcePath, dataSourceId, opState);
}

std::string PipelineStateManager::addDataSource(
  const std::string& dataSourceState)
{
  return m_proxy->addDataSource(dataSourceState);
}

void PipelineStateManager::removeOperator(const std::string& opPath,
                                          const std::string& dataSourceId,
                                          const std::string& opId)
{
  m_proxy->removeOperator(opPath, dataSourceId, opId);
}

void PipelineStateManager::removeModule(const std::string& modulePath,
                                        const std::string& dataSourceId,
                                        const std::string& moduleId)
{
  m_proxy->removeModule(modulePath, dataSourceId, moduleId);
}

void PipelineStateManager::removeDataSource(const std::string& dataSourcePath,
                                            const std::string& dataSourceId)
{
  m_proxy->removeDataSource(dataSourcePath, dataSourceId);
}

void PipelineStateManager::enableSyncToPython()
{
  m_proxy->enableSyncToPython();
}

void PipelineStateManager::disableSyncToPython()
{
  m_proxy->disableSyncToPython();
}

void PipelineStateManager::pausePipeline(const std::string& dataSourcePath)
{
  m_proxy->pausePipeline(dataSourcePath);
}

void PipelineStateManager::resumePipeline(const std::string& dataSourcePath)
{
  m_proxy->resumePipeline(dataSourcePath);
}

void PipelineStateManager::executePipeline(const std::string& dataSourcePath)
{
  m_proxy->executePipeline(dataSourcePath);
}

bool PipelineStateManager::pipelinePaused(const std::string& dataSourcePath)
{
  return m_proxy->pipelinePaused(dataSourcePath);
}
