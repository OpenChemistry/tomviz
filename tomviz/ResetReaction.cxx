/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ResetReaction.h"

#include "ActiveObjects.h"
#include "HistogramManager.h"
#include "animations/CameraViewpoints.h"
#include "animations/ModuleAnimations.h"
#include "pipeline/Pipeline.h"
#include "Utilities.h"

#include <QMessageBox>

namespace tomviz {

ResetReaction::ResetReaction(QAction* parentObject) : Superclass(parentObject)
{}

void ResetReaction::updateEnableState()
{
  auto* pipeline = ActiveObjects::instance().pipeline();
  bool enabled = !pipeline || !pipeline->isExecuting();
  parentAction()->setEnabled(enabled);
}

void ResetReaction::reset()
{
  auto* pipeline = ActiveObjects::instance().pipeline();
  if (pipeline && !pipeline->nodes().isEmpty()) {
    if (QMessageBox::Yes !=
        QMessageBox::warning(
          tomviz::mainWidget(), "Reset",
          "Data may be lost when resetting. Are you sure you want to reset?",
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No)) {
      return;
    }
  }
  if (pipeline) {
    pipeline->clear();
  }
  // The animations describe the data that just went: the viewpoints
  // framed it and their recorded state names its nodes. Clearing them
  // also stops the camera path, and lets the next dataset start with
  // its own opening orbit.
  CameraViewpoints::instance().clear();
  ModuleAnimations::instance().clear();
  HistogramManager::instance().clearCaches();
}
} // namespace tomviz
