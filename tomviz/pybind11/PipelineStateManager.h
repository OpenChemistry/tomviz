/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleManagerWrapper_h
#define tomvizModuleManagerWrapper_h

#include <string>
#include <vector>

class PipelineStateManager
{
public:
  PipelineStateManager();
  std::string serialize();
  void load(const std::string& state, const std::string& stateRelDir);
  std::string modulesJson();
  std::string operatorsJson();
  std::string serializeOperator(const std::string& path, const std::string& id);
  void updateOperator(const std::string& path, const std::string& state);
  std::string serializeModule(const std::string& path, const std::string& id);
  void updateModule(const std::string& path, const std::string& state);
  std::string serializeDataSource(const std::string& path, const std::string& id);
  void updateDataSource(const std::string& path, const std::string& state);
  std::string addModule(const std::string& dataSourcePath,
                        const std::string& dataSourceId,
                        const std::string& moduleType);
  std::string addOperator(const std::string& dataSourcePath,
                   const std::string& dataSourceId, const std::string& opState);
  std::string addDataSource(const std::string& dataSourceState);
  void removeOperator(const std::string& opPath,
                      const std::string& dataSourceId,
                      const std::string& opId = "");
  void removeModule(const std::string& modulePath,
                    const std::string& dataSourceId,
                    const std::string& moduleId = "");
  void removeDataSource(const std::string& dataSourcePath,
                        const std::string& dataSourceId = "");
  void modified(std::vector<std::string> opPaths,
                std::vector<std::string> modulePaths);
  void syncToPython();
  void enableSyncToPython();
  void disableSyncToPython();

private:
  bool m_syncToPython = true;
};

#endif
