/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveDataReaction.h"

#include "ActiveObjects.h"
#include "SaveDataDialog.h"
#include "Utilities.h"

#include "pipeline/Pipeline.h"

#include <QAction>

namespace tomviz {

SaveDataReaction::SaveDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  auto& ao = ActiveObjects::instance();
  connect(&ao, &ActiveObjects::activePipelineChanged, this,
          &SaveDataReaction::connectToPipeline);
  // MainWindow creates the pipeline before it creates its reactions, so
  // activePipelineChanged has usually already fired by the time we get
  // here — pick up whatever is already there.
  connectToPipeline(ao.pipeline());
}

void SaveDataReaction::connectToPipeline(pipeline::Pipeline* p)
{
  // Ports gain data when a run finishes and lose it when their node goes
  // away; between them those cover the transitions worth greying the menu
  // item for. A port that gains data by some other route — a state file
  // attaching payloads before the first run — leaves the item stale until
  // the next run, which the dialog's empty state handles gracefully.
  if (p) {
    connect(p, &pipeline::Pipeline::executionFinished, this,
            &SaveDataReaction::updateEnableState, Qt::UniqueConnection);
    connect(p, &pipeline::Pipeline::nodeRemoved, this,
            &SaveDataReaction::updateEnableState, Qt::UniqueConnection);
  }
  updateEnableState();
}

void SaveDataReaction::updateEnableState()
{
  auto* pipeline = ActiveObjects::instance().pipeline();
  // AllPorts covers every port LeafNodes does. Released ports count too,
  // so the dialog can say why they cannot be saved.
  const auto scope = SaveDataDialog::Scope::AllPorts;
  bool anything =
    !SaveDataDialog::candidatePorts(pipeline, scope).isEmpty() ||
    !SaveDataDialog::releasedPorts(pipeline, scope).isEmpty();
  parentAction()->setEnabled(anything);
}

void SaveDataReaction::onTriggered()
{
  auto* pipeline = ActiveObjects::instance().pipeline();
  if (!pipeline) {
    return;
  }

  auto* parent = tomviz::mainWidget();
  SaveDataDialog dialog(pipeline, parent);
  if (dialog.exec() == QDialog::Accepted) {
    SaveDataDialog::writeEntries(dialog.selectedEntries(), parent);
  }
}

} // end of namespace tomviz
