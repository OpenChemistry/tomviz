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
#include "AddExpressionReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "OperatorPython.h"
#include "pqCoreUtilities.h"
#include "EditPythonOperatorDialog.h"

namespace tomviz
{
//-----------------------------------------------------------------------------
AddExpressionReaction::AddExpressionReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
AddExpressionReaction::~AddExpressionReaction()
{
}

//-----------------------------------------------------------------------------
void AddExpressionReaction::updateEnableState()
{
  this->parentAction()->setEnabled(
    ActiveObjects::instance().activeDataSource() != NULL);
}

//-----------------------------------------------------------------------------
OperatorPython* AddExpressionReaction::addExpression(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
    {
    return NULL;
    }

  OperatorPython *opPython = new OperatorPython();
  QSharedPointer<Operator> op(opPython);
  opPython->setLabel("Transform Data");

  // Create a non-modal dialog, delete it once it has been closed.
  EditPythonOperatorDialog *dialog =
      new EditPythonOperatorDialog(op, pqCoreUtilities::mainWidget());
  dialog->setAttribute(Qt::WA_DeleteOnClose, true);
  connect(dialog, SIGNAL(accepted()), SLOT(addOperator()));
  dialog->show();
  return NULL;
}

void AddExpressionReaction::addOperator()
{
  EditPythonOperatorDialog *dialog =
      qobject_cast<EditPythonOperatorDialog*>(sender());
  if (!dialog)
    return;

  DataSource *source = ActiveObjects::instance().activeDataSource();
  if (!source)
    {
    return;
    }
  source->addOperator(dialog->op());
}

}
