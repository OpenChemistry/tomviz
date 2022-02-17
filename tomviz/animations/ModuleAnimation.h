/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleAnimation_h
#define tomvizModuleAnimation_h

#include "ActiveObjects.h"
#include "Module.h"
#include "ModuleManager.h"

#include <pqTimeKeeper.h>

#include <QObject>

namespace tomviz {

class ModuleAnimation : public QObject
{
  Q_OBJECT

public:
  Module* baseModule = nullptr;

  ModuleAnimation(Module* module) : baseModule(module) { setupConnections(); }

  virtual void setupConnections()
  {
    if (timeKeeper()) {
      connect(timeKeeper(), &pqTimeKeeper::timeChanged, this,
              &ModuleAnimation::onTimeChanged);
    }

    connect(&moduleManager(), &ModuleManager::moduleRemoved, this,
            &ModuleAnimation::onModuleRemoved);
  }

  virtual void onModuleRemoved(Module* module)
  {
    if (module != baseModule) {
      return;
    }

    this->disconnect();
  }

  virtual ActiveObjects& activeObjects() { return ActiveObjects::instance(); }
  virtual pqTimeKeeper* timeKeeper()
  {
    return activeObjects().activeTimeKeeper();
  }

  virtual double time()
  {
    if (!timeKeeper()) {
      return 0;
    }

    return timeKeeper()->getTime();
  }

  virtual QList<double> timeSteps()
  {
    if (!timeKeeper()) {
      return { 0 };
    }

    auto timeSteps = timeKeeper()->getTimeSteps();
    if (timeSteps.empty()) {
      // It's just a 0 to 1 default.
      timeSteps.append(0);
      timeSteps.append(1);
    }

    return timeSteps;
  }

  virtual double timeStart() { return timeSteps().front(); }
  virtual double timeStop() { return timeSteps().back(); }
  virtual double progress() { return time() / (timeStop() - timeStart()); }

  virtual ModuleManager& moduleManager() { return ModuleManager::instance(); }

  virtual void onTimeChanged() {}
};

} // namespace tomviz

#endif
