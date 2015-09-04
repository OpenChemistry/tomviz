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
#include "EditOperatorDialog.h"

#include "EditOperatorWidget.h"
#include "Operator.h"

#include "pqPythonSyntaxHighlighter.h"

#include <QDialogButtonBox>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>


namespace tomviz
{

class EditOperatorDialog::EODInternals
{
public:
  QSharedPointer<Operator> Op;
};

//-----------------------------------------------------------------------------
EditOperatorDialog::EditOperatorDialog(
  QSharedPointer<Operator> &o, QWidget* parentObject)
  : Superclass(parentObject),
  Internals (new EditOperatorDialog::EODInternals())
{
  Q_ASSERT(o);
  this->Internals->Op = o;

  QVBoxLayout* vLayout = new QVBoxLayout(this);
  EditOperatorWidget* opWidget = o->getEditorContents(this);
  vLayout->addWidget(opWidget);
  QDialogButtonBox* dialogButtons = new QDialogButtonBox(
      QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Ok,
      Qt::Horizontal, this);
  vLayout->addWidget(dialogButtons);

  this->setLayout(vLayout);
  this->connect(dialogButtons, SIGNAL(accepted()), SLOT(accept()));
  this->connect(dialogButtons, SIGNAL(rejected()), SLOT(reject()));

  QObject::connect(dialogButtons->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
                   opWidget, SLOT(applyChangesToOperator()));
  this->connect(dialogButtons->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
                   SLOT(onApply()));
  QObject::connect(this, SIGNAL(accepted()), opWidget, SLOT(applyChangesToOperator()));
  this->connect(this, SIGNAL(accepted()), SLOT(onApply()));
}

//-----------------------------------------------------------------------------
EditOperatorDialog::~EditOperatorDialog()
{
}

QSharedPointer<Operator>& EditOperatorDialog::op()
{
  return this->Internals->Op;
}

void EditOperatorDialog::onApply()
{
  emit applyChanges();
}

}
