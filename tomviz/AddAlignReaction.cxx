/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AddAlignReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "Pipeline.h"
#include "TranslateAlignOperator.h"
#include "Utilities.h"

#include <QDebug>

namespace tomviz {

AddAlignReaction::AddAlignReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void AddAlignReaction::updateEnableState()
{
  auto pipeline = ActiveObjects::instance().activePipeline();
  bool enable = pipeline != nullptr;

  // if (enable) {
  //   auto dataSource = pipeline->transformedDataSource();
  //   enable = (dataSource->type() == DataSource::TiltSeries ||
  //             dataSource->type() == DataSource::FIB);
  // }

  parentAction()->setEnabled(enable);
}

void AddAlignReaction::align(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeParentDataSource();
  if (!source) {
    qDebug() << "Exiting early - no data found.";
    return;
  }

  auto Op = new TranslateAlignOperator(source);
  auto dialog = new EditOperatorDialog(Op, source, true, tomviz::mainWidget());

  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle("Manual Image Alignment");
  dialog->show();
  connect(Op, SIGNAL(destroyed()), dialog, SLOT(reject()));
}
} // namespace tomviz
