/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ResetReaction.h"

#include "ModuleManager.h"
#include "Utilities.h"

#include <QMessageBox>

namespace tomviz {

ResetReaction::ResetReaction(QAction* parentObject) : Superclass(parentObject)
{}

void ResetReaction::updateEnableState()
{
  bool enabled = !ModuleManager::instance().hasRunningOperators();
  parentAction()->setEnabled(enabled);
}

void ResetReaction::reset()
{
  if (ModuleManager::instance().hasDataSources() ||
      ModuleManager::instance().hasMoleculeSources()) {
    if (QMessageBox::Yes !=
        QMessageBox::warning(
          tomviz::mainWidget(), "Reset",
          "Data may be lost when resetting. Are you sure you want to reset?",
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No)) {
      return;
    }
  }
  ModuleManager::instance().reset();
}
} // namespace tomviz
