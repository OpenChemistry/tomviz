/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "InternalPythonHelper.h"

#include <QDebug>

namespace tomviz {

class InternalPythonHelper::Internal
{
public:
  Python::Module operatorModule;
  Python::Module internalModule;
  Python::Function deleteModuleFunction;

  void loadInternalModules()
  {
    Python::initialize();

    {
      Python python;
      operatorModule = python.import("tomviz.utils");
      if (!operatorModule.isValid()) {
        qCritical() << "Failed to import tomviz.utils module.";
      }

      internalModule = python.import("tomviz._internal");
      if (!internalModule.isValid()) {
        qCritical() << "Failed to import tomviz._internal module.";
      }

      deleteModuleFunction = internalModule.findFunction("delete_module");
      if (!deleteModuleFunction.isValid()) {
        qCritical() << "Unable to locate delete_module.";
      }
    }
  }

  Python::Module loadModule(const QString& script, const QString& name)
  {
    Python::Module result;
    {
      Python python;
      QString label = name + ".py";

      result = python.import(script, label, name);
      if (!result.isValid()) {
        qCritical("Failed to load module.");
        return result;
      }

      // Delete the module from sys.module so we don't reuse it
      Python::Tuple delArgs(1);
      Python::Object pyName(name);
      delArgs.set(0, pyName);
      auto delResult = deleteModuleFunction.call(delArgs);
      if (!delResult.isValid()) {
        qCritical("An error occurred deleting module.");
      }
    }

    return result;
  }
};

InternalPythonHelper::InternalPythonHelper()
{
  m_internal.reset(new Internal);
  m_internal->loadInternalModules();
}

InternalPythonHelper::~InternalPythonHelper() = default;

Python::Module InternalPythonHelper::loadModule(const QString& script,
                                                const QString& name)
{
  return m_internal->loadModule(script, name);
}

} // namespace tomviz
