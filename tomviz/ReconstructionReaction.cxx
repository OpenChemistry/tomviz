/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ReconstructionReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "Pipeline.h"

#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

#include "ReconstructionOperator.h"

#include <QDebug>
#include <QSharedPointer>

namespace tomviz {

ReconstructionReaction::ReconstructionReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void ReconstructionReaction::updateEnableState()
{
  auto pipeline = ActiveObjects::instance().activePipeline();
  bool enable = pipeline != nullptr;

  if (enable) {
    auto dataSource = pipeline->transformedDataSource();
    enable = dataSource->type() == DataSource::TiltSeries;
  }

  parentAction()->setEnabled(enable);
}

void ReconstructionReaction::recon(DataSource* input)
{
  input = input ? input : ActiveObjects::instance().activeParentDataSource();
  if (!input) {
    qDebug() << "Exiting early - no data :-(";
    return;
  }

  Operator* op = new ReconstructionOperator(input);
  input->addOperator(op);
}
} // namespace tomviz
