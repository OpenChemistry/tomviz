/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineProxyBase_h
#define tomvizPipelineProxyBase_h

#include "tomvizcore_export.h"

#include <string>
#include <vector>

namespace tomviz {

/** Pure virtual base class providing a proxy to the operator class. */
class TOMVIZCORE_EXPORT PipelineProxyBase
{
public:
  virtual ~PipelineProxyBase() = default;
  virtual std::string serialize() = 0;
  virtual void load(const std::string& state,
                    const std::string& stateRelDir) = 0;
  virtual std::string modulesJson() = 0;
  virtual std::string operatorsJson() = 0;
  virtual std::string serializeOperator(const std::string& path,
                                        const std::string& id) = 0;
  virtual void updateOperator(const std::string& path,
                              const std::string& state) = 0;
  virtual std::string serializeModule(const std::string& path,
                                      const std::string& id) = 0;
  virtual void updateModule(const std::string& path,
                            const std::string& state) = 0;
  virtual std::string serializeDataSource(const std::string& path,
                                          const std::string& id) = 0;
  virtual void updateDataSource(const std::string& path,
                                const std::string& state) = 0;
  virtual std::string addModule(const std::string& dataSourcePath,
                                const std::string& dataSourceId,
                                const std::string& moduleType) = 0;
  virtual std::string addOperator(const std::string& dataSourcePath,
                                  const std::string& dataSourceId,
                                  const std::string& opState) = 0;
  virtual std::string addDataSource(const std::string& dataSourceState) = 0;
  virtual void removeOperator(const std::string& opPath,
                              const std::string& dataSourceId,
                              const std::string& opId = "") = 0;
  virtual void removeModule(const std::string& modulePath,
                            const std::string& dataSourceId,
                            const std::string& moduleId = "") = 0;
  virtual void removeDataSource(const std::string& dataSourcePath,
                                const std::string& dataSourceId = "") = 0;
  virtual void modified(std::vector<std::string> opPaths,
                        std::vector<std::string> modulePaths) = 0;
  virtual void syncToPython() = 0;
  virtual void syncViewsToPython() = 0;
  virtual void enableSyncToPython() = 0;
  virtual void disableSyncToPython() = 0;
  virtual void pausePipeline(const std::string& dataSourcePath) = 0;
  virtual void resumePipeline(const std::string& dataSourcePath) = 0;
  virtual void executePipeline(const std::string& dataSourcePath) = 0;
  virtual bool pipelinePaused(const std::string& dataSourcePath) = 0;
};

class PipelineProxyBaseFactory
{
public:
  virtual ~PipelineProxyBaseFactory() = default;
  virtual PipelineProxyBase* create() = 0;
};

} // namespace tomviz

#endif