/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "SetTiltAnglesReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "SetTiltAnglesOperator.h"

#include <QMainWindow>

namespace tomviz
{

SetTiltAnglesReaction::SetTiltAnglesReaction(QAction *p, QMainWindow *mw)
  : Superclass(p), mainWindow(mw)
{
  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  updateEnableState();
}

SetTiltAnglesReaction::~SetTiltAnglesReaction()
{
}

void SetTiltAnglesReaction::updateEnableState()
{
  bool enable = ActiveObjects::instance().activeDataSource() != nullptr;
  if (enable)
  {
    enable = ActiveObjects::instance().activeDataSource()->type() == DataSource::TiltSeries;
  }
  parentAction()->setEnabled(enable);
}

void SetTiltAnglesReaction::showSetTiltAnglesUI(QMainWindow *window, DataSource *source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
  {
    return;
  }
  auto operators = source->operators();
  SetTiltAnglesOperator *op = nullptr;
  bool needToAddOp = false;
  if (operators.size() > 0)
  {
    op = qobject_cast<SetTiltAnglesOperator*>(operators[operators.size() - 1]);
  }
  if (!op)
  {
    op = new SetTiltAnglesOperator;
    needToAddOp = true;
  }
  EditOperatorDialog *dialog = new EditOperatorDialog(op, source, needToAddOp, window);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

}
