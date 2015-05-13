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
#include "EditPythonOperatorDialog.h"
#include "ui_EditPythonOperatorDialog.h"

#include "OperatorPython.h"

#include "pqPythonSyntaxHighlighter.h"

#include <QPointer>

namespace TEM
{

class EditPythonOperatorDialog::EPODInternals
{
public:
  Ui::EditPythonOperatorDialog Ui;
  QSharedPointer<Operator> Op;
};

//-----------------------------------------------------------------------------
EditPythonOperatorDialog::EditPythonOperatorDialog(
  QSharedPointer<Operator> &op, QWidget* parentObject)
  : Superclass(parentObject),
  Internals (new EditPythonOperatorDialog::EPODInternals())
{
  Q_ASSERT(op);
  this->Internals->Op = op;
  Ui::EditPythonOperatorDialog& ui = this->Internals->Ui;
  ui.setupUi(this);

  OperatorPython* opPython = qobject_cast<OperatorPython*>(op.data());
  Q_ASSERT(opPython);

  ui.name->setText(opPython->label());
  if (!opPython->script().isEmpty())
    {
    ui.script->setPlainText(opPython->script());
    }
  new pqPythonSyntaxHighlighter(ui.script, this);

  this->connect(this, SIGNAL(accepted()), SLOT(acceptChanges()));
}

//-----------------------------------------------------------------------------
EditPythonOperatorDialog::~EditPythonOperatorDialog()
{
}

//-----------------------------------------------------------------------------
void EditPythonOperatorDialog::acceptChanges()
{
  Ui::EditPythonOperatorDialog& ui = this->Internals->Ui;
  OperatorPython* opPython =
      qobject_cast<OperatorPython*>(this->Internals->Op.data());
  Q_ASSERT(opPython);
  opPython->setLabel(ui.name->text());
  opPython->setScript(ui.script->toPlainText());
}

QSharedPointer<Operator>& EditPythonOperatorDialog::op()
{
  return this->Internals->Op;
}

}
