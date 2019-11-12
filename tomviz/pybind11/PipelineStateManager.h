/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleManagerWrapper_h
#define tomvizModuleManagerWrapper_h

#include <string>
#include <vector>

struct PipelineStateManager
{
  std::string serialize();
  void sync(const std::string& jsonpatch);
  void load(const std::string& state, const std::string& stateRelDir);
  std::string modulesJson();
  std::string operatorsJson();
  std::string serializeOperator(const std::string& path, const std::string& id);
  void deserializeOperator(const std::string& path, const std::string& state);
  std::string serializeModule(const std::string& path, const std::string& id);
  void deserializeModule(const std::string& path, const std::string& state);
  std::string addModule(const std::string& dataSourcePath,
                        const std::string& dataSourceId,
                        const std::string& moduleType);
  void addOperator(const std::string& dataSourcePath,
                   const std::string& dataSourceId, const std::string& opState);
  void modified(std::vector<std::string> opPaths,
                std::vector<std::string> modulePaths);
};

#endif
