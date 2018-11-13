/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SetTiltAnglesReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "SetTiltAnglesOperator.h"

#include <QMainWindow>

namespace tomviz {

SetTiltAnglesReaction::SetTiltAnglesReaction(QAction* p, QMainWindow* mw)
  : pqReaction(p), m_mainWindow(mw)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void SetTiltAnglesReaction::updateEnableState()
{
  bool enable = ActiveObjects::instance().activeDataSource() != nullptr;
  if (enable) {
    enable = ActiveObjects::instance().activeDataSource()->type() ==
             DataSource::TiltSeries;
  }
  parentAction()->setEnabled(enable);
}

void SetTiltAnglesReaction::showSetTiltAnglesUI(QMainWindow* window,
                                                DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeParentDataSource();
  if (!source) {
    return;
  }
  auto operators = source->operators();
  SetTiltAnglesOperator* op = nullptr;
  bool needToAddOp = false;
  if (operators.size() > 0) {
    op = qobject_cast<SetTiltAnglesOperator*>(operators[operators.size() - 1]);
  }
  if (!op) {
    op = new SetTiltAnglesOperator;
    needToAddOp = true;
  }
  EditOperatorDialog* dialog =
    new EditOperatorDialog(op, source, needToAddOp, window);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle("Set Tilt Angles");
  dialog->show();
  connect(op, SIGNAL(destroyed()), dialog, SLOT(reject()));
}
} // namespace tomviz
