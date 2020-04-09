/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineProxy_h
#define tomvizPipelineProxy_h

#include "core/PipelineProxyBase.h"

namespace tomviz {

/** Pure virtual base class providing a proxy to the pipeline. */
class PipelineProxy : public PipelineProxyBase
{
public:
  PipelineProxy();
  std::string serialize() override;
  void load(const std::string& state, const std::string& stateRelDir) override;
  std::string modulesJson() override;
  std::string operatorsJson() override;
  std::string serializeOperator(const std::string& path,
                                const std::string& id) override;
  void updateOperator(const std::string& path,
                      const std::string& state) override;
  std::string serializeModule(const std::string& path,
                              const std::string& id) override;
  void updateModule(const std::string& path, const std::string& state) override;
  std::string serializeDataSource(const std::string& path,
                                  const std::string& id) override;
  void updateDataSource(const std::string& path,
                        const std::string& state) override;
  std::string addModule(const std::string& dataSourcePath,
                        const std::string& dataSourceId,
                        const std::string& moduleType) override;
  std::string addOperator(const std::string& dataSourcePath,
                          const std::string& dataSourceId,
                          const std::string& opState) override;
  std::string addDataSource(const std::string& dataSourceState) override;
  void removeOperator(const std::string& opPath,
                      const std::string& dataSourceId,
                      const std::string& opId = "") override;
  void removeModule(const std::string& modulePath,
                    const std::string& dataSourceId,
                    const std::string& moduleId = "") override;
  void removeDataSource(const std::string& dataSourcePath,
                        const std::string& dataSourceId = "") override;
  void modified(std::vector<std::string> opPaths,
                std::vector<std::string> modulePaths) override;
  void syncToPython() override;
  void enableSyncToPython() override;
  void disableSyncToPython() override;
  void pausePipeline(const std::string& dataSourcePath) override;
  void resumePipeline(const std::string& dataSourcePath) override;
  void executePipeline(const std::string& dataSourcePath) override;
  bool pipelinePaused(const std::string& dataSourcePath) override;

  void syncViewsToPython() override;

private:
  bool m_syncToPython = true;
};

class PipelineProxyFactory : public PipelineProxyBaseFactory
{
public:
  PipelineProxyBase* create() override;
  static void registerWithFactory();
};

} // namespace tomviz

#endif