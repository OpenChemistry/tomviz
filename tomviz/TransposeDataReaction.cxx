/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TransposeDataReaction.h"

#include <QAction>
#include <QMainWindow>

#include "ActiveObjects.h"
#include "TransposeDataOperator.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"

namespace tomviz {

TransposeDataReaction::TransposeDataReaction(QAction* parentObject,
                                             QMainWindow* mw)
  : pqReaction(parentObject), m_mainWindow(mw)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void TransposeDataReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

void TransposeDataReaction::transposeData(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeParentDataSource();
  if (!source) {
    return;
  }

  Operator* Op = new TransposeDataOperator();

  EditOperatorDialog* dialog =
    new EditOperatorDialog(Op, source, true, m_mainWindow);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  connect(Op, SIGNAL(destroyed()), dialog, SLOT(reject()));
}
} // namespace tomviz
