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

#include "DataSource.h"
#include "EditOperatorWidget.h"
#include "Operator.h"

#include <pqApplicationCore.h>
#include <pqPythonSyntaxHighlighter.h>
#include <pqSettings.h>

#include <QDialogButtonBox>
#include <QPointer>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

namespace tomviz
{

class EditOperatorDialog::EODInternals
{
public:
  QPointer<Operator> Op;
  EditOperatorWidget *Widget;
  bool needsToBeAdded;
  DataSource *dataSource;

  void savePosition(const QPoint& pos)
  {
    QSettings *settings = pqApplicationCore::instance()->settings();
    QString settingName = QString("Edit%1OperatorDialogPosition")
                          .arg(Op->label());
    settings->setValue(settingName, QVariant(pos));
  }

  QVariant loadPosition()
  {
    QSettings *settings = pqApplicationCore::instance()->settings();
    QString settingName = QString("Edit%1OperatorDialogPosition")
                          .arg(Op->label());
    return settings->value(settingName);
  }
};

EditOperatorDialog::EditOperatorDialog(Operator* op, DataSource *dataSource,
                                       bool needToAddOperator, QWidget* p)
  : Superclass(p), Internals(new EditOperatorDialog::EODInternals())
{
  Q_ASSERT(op);
  this->Internals->Op = op;
  this->Internals->dataSource = dataSource;
  this->Internals->needsToBeAdded = needToAddOperator;
  if (needToAddOperator)
  {
    op->setParent(this);
  }

  QVariant position = this->Internals->loadPosition();
  if (!position.isNull())
  {
    this->move(position.toPoint());
  }

  QVBoxLayout* vLayout = new QVBoxLayout(this);
  if (op->hasCustomUI())
  {
    EditOperatorWidget* opWidget = op->getEditorContents(this);
    if (!opWidget)
    {
      vtkSmartPointer<vtkImageData> snapshotImage =
          dataSource->getCopyOfImagePriorTo(op);
      opWidget = op->getEditorContentsWithData(this, snapshotImage);
    }
    vLayout->addWidget(opWidget);
    this->Internals->Widget = opWidget;
    const double *dsPosition = dataSource->displayPosition();
    opWidget->dataSourceMoved(dsPosition[0], dsPosition[1], dsPosition[2]);
    QObject::connect(dataSource, &DataSource::displayPositionChanged, opWidget,
                     &EditOperatorWidget::dataSourceMoved);
  }
  else
  {
    this->Internals->Widget = nullptr;
  }
  QDialogButtonBox* dialogButtons = new QDialogButtonBox(
      QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Ok,
      Qt::Horizontal, this);
  vLayout->addWidget(dialogButtons);

  this->setLayout(vLayout);
  this->connect(dialogButtons, SIGNAL(accepted()), SLOT(accept()));
  this->connect(dialogButtons, SIGNAL(rejected()), SLOT(reject()));

  this->connect(dialogButtons->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
                   SLOT(onApply()));
  this->connect(this, SIGNAL(accepted()), SLOT(onApply()));
  this->connect(this, SIGNAL(accepted()), SLOT(onClose()));
  this->connect(this, SIGNAL(rejected()), SLOT(onClose()));
}

EditOperatorDialog::~EditOperatorDialog()
{
}

Operator* EditOperatorDialog::op()
{
  return this->Internals->Op;
}

void EditOperatorDialog::onApply()
{
  if (this->Internals->Widget)
  {
    this->Internals->Widget->applyChangesToOperator();
  }
  if (this->Internals->needsToBeAdded)
  {
    this->Internals->dataSource->addOperator(this->Internals->Op);
    this->Internals->needsToBeAdded = false;
  }
}

void EditOperatorDialog::onClose()
{
  this->Internals->savePosition(this->pos());
}

}
