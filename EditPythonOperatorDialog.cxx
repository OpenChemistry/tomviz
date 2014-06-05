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

#include <QPointer>

namespace TEM
{

class EditPythonOperatorDialog::EPODInternals
{
public:
  Ui::EditPythonOperatorDialog Ui;
  QPointer<OperatorPython> Operator;
};

//-----------------------------------------------------------------------------
EditPythonOperatorDialog::EditPythonOperatorDialog(
  OperatorPython* op, QWidget* parentObject)
  : Superclass(parentObject),
  Internals (new EditPythonOperatorDialog::EPODInternals())
{
  Q_ASSERT(op);
  this->Internals->Operator = op;
  Ui::EditPythonOperatorDialog& ui = this->Internals->Ui;
  ui.setupUi(this);

  ui.name->setText(op->label());
  if (!op->script().isEmpty())
    {
    ui.script->setPlainText(op->script());
    }

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
  this->Internals->Operator->setLabel(ui.name->text());
  this->Internals->Operator->setScript(ui.script->toPlainText());
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------



}
