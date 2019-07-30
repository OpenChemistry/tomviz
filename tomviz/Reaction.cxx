/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Reaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "PipelineManager.h"

namespace tomviz {

Reaction::Reaction(QAction* parentObject) : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  connect(&PipelineManager::instance(), &PipelineManager::executionModeUpdated,
          this, &Reaction::updateEnableState);

  updateEnableState();
}

void Reaction::updateEnableState()
{
  auto compatibleExecutionMode = PipelineManager::instance().executionMode() ==
                                 Pipeline::ExecutionMode::Threaded;

  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                               nullptr &&
                             compatibleExecutionMode);
}

} // namespace tomviz
