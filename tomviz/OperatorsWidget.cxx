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
#include "OperatorsWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "OperatorPython.h"
#include "pqCoreUtilities.h"

#include <QHeaderView>
#include <QSharedPointer>
#include <QMap>

namespace tomviz
{

class OperatorsWidget::OWInternals
{
public:
  QPointer<DataSource> ADataSource;
  QMap<QTreeWidgetItem*, QSharedPointer<Operator> > ItemMap;
};

//-----------------------------------------------------------------------------
OperatorsWidget::OperatorsWidget(QWidget* parentObject) :
  Superclass(parentObject),
  Internals(new OperatorsWidget::OWInternals())
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(setDataSource(DataSource*)));
  connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
          SLOT(itemDoubleClicked(QTreeWidgetItem*)));
  connect(this, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
          SLOT(onItemClicked(QTreeWidgetItem*, int)));

  this->header()->setResizeMode(0, QHeaderView::Stretch);
  this->header()->setResizeMode(1, QHeaderView::Fixed);
  this->header()->resizeSection(1, 25);
  this->header()->setStretchLastSection(false);
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
    this->Internals->ADataSource->disconnect();
    }
  this->Internals->ADataSource = ds;
  if (!ds)
    {
    return;
    }

  this->connect(ds, SIGNAL(operatorAdded(QSharedPointer<Operator>&)),
                SLOT(operatorAdded(QSharedPointer<Operator>&)));

  foreach (QSharedPointer<Operator> op, ds->operators())
    {
    this->operatorAdded(op);
    }
}

//-----------------------------------------------------------------------------
void OperatorsWidget::operatorAdded(QSharedPointer<Operator> &op)
{
  QTreeWidgetItem* item = new QTreeWidgetItem();
  item->setText(0, op->label());
  item->setIcon(0, op->icon());
  item->setIcon(1, QIcon(":/QtWidgets/Icons/pqDelete32.png"));
  this->addTopLevelItem(item);
  this->Internals->ItemMap[item] = op;
  this->connect(op.data(), SIGNAL(labelModified()), SLOT(updateOperatorLabel()));
}

//-----------------------------------------------------------------------------
void OperatorsWidget::itemDoubleClicked(QTreeWidgetItem* item)
{
  QSharedPointer<Operator> op = this->Internals->ItemMap[item];
  Q_ASSERT(op);

  // Create a non-modal dialog, delete it once it has been closed.
  EditOperatorDialog *dialog =
    new EditOperatorDialog(op, pqCoreUtilities::mainWidget());
  dialog->setAttribute(Qt::WA_DeleteOnClose, true);
  dialog->show();
}

void OperatorsWidget::onItemClicked(QTreeWidgetItem *item, int col)
{
  QSharedPointer<Operator> op = this->Internals->ItemMap[item];
  Q_ASSERT(op);
  if (col == 1 && op)
    {
    this->Internals->ADataSource->removeOperator(op);
    this->takeTopLevelItem(this->indexOfTopLevelItem(item));
    }
}

void OperatorsWidget::updateOperatorLabel()
{
  Operator *op =
      qobject_cast<Operator*>(sender());

  // Find the item, and update the text.
  QMap<QTreeWidgetItem*, QSharedPointer<Operator> >::iterator i =
      this->Internals->ItemMap.begin();
  while (i != this->Internals->ItemMap.constEnd())
    {
    if (i.value().data() == op)
      {
      i.key()->setText(0, op->label());
      return;
      }
    ++i;
    }
}

}
