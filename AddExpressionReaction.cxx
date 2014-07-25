/******************************************************************************

  This source file is part of the TEM tomography project.

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


namespace TEM
{
//-----------------------------------------------------------------------------
AddExpressionReaction::AddExpressionReaction(QAction* parentObject)
  :Superclass(parentObject)
{
  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
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
  source = source? source : ActiveObjects::instance().activeDataSource();
  if (!source)
    {
    return NULL;
    }

  QSharedPointer<OperatorPython> op(new OperatorPython());
  op->setLabel("Transform Data");
  EditPythonOperatorDialog dialog (op.data(), pqCoreUtilities::mainWidget());
  if (dialog.exec() == QDialog::Accepted)
    {
    source->addOperator(op);
    }
  return NULL;
}



}
