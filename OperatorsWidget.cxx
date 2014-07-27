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
#include "OperatorsWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditPythonOperatorDialog.h"
#include "OperatorPython.h"
#include "pqCoreUtilities.h"

#include <QHeaderView>
#include <QPointer>
#include <QMap>

namespace TEM
{

class OperatorsWidget::OWInternals
{
public:
  QPointer<DataSource> ADataSource;
  QMap<QTreeWidgetItem*, QPointer<Operator> > ItemMap;
};

//-----------------------------------------------------------------------------
OperatorsWidget::OperatorsWidget(QWidget* parentObject) :
  Superclass(parentObject),
  Internals(new OperatorsWidget::OWInternals())
{
  this->connect(&ActiveObjects::instance(),
    SIGNAL(dataSourceChanged(DataSource*)),
    SLOT(setDataSource(DataSource*)));
  this->connect(this,
    SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
    SLOT(itemDoubleClicked(QTreeWidgetItem*)));
}

//-----------------------------------------------------------------------------
OperatorsWidget::~OperatorsWidget()
{
}

//-----------------------------------------------------------------------------
void OperatorsWidget::setDataSource(DataSource* ds)
{
  if (this->Internals->ADataSource == ds)
    {
    return;
    }
  this->clear();
  if (this->Internals->ADataSource)
    {
    this->disconnect(this->Internals->ADataSource);
    }
  this->Internals->ADataSource = ds;
  if (!ds)
    {
    return;
    }

  this->connect(ds, SIGNAL(operatorAdded(Operator*)),
                SLOT(operatorAdded(Operator*)));

  foreach (QSharedPointer<Operator> op, ds->operators())
    {
    this->operatorAdded(op.data());
    }
}

//-----------------------------------------------------------------------------
void OperatorsWidget::operatorAdded(Operator* op)
{
  QTreeWidgetItem* item = new QTreeWidgetItem();
  item->setText(0, op->label());
  item->setIcon(0, op->icon());
  this->addTopLevelItem(item);
  this->Internals->ItemMap[item] = op;
}

//-----------------------------------------------------------------------------
void OperatorsWidget::itemDoubleClicked(QTreeWidgetItem* item)
{
  Operator* op = this->Internals->ItemMap[item];
  Q_ASSERT(op);

  OperatorPython* opPyhon = qobject_cast<OperatorPython*>(op);
  Q_ASSERT(opPyhon);
  //edit op.

  EditPythonOperatorDialog dialog (opPyhon, pqCoreUtilities::mainWidget());
  if (dialog.exec() == QDialog::Accepted)
    {
    // update label in case it changed.
    item->setText(0, op->label());
    }
}

}
