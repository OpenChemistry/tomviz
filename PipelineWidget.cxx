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
#include "PipelineWidget.h"

#include "ActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqPipelineSource.h"
#include "pqServerManagerModel.h"
#include "Utilities.h"

namespace TEM
{
class PipelineWidget::PWInternals
{
public:
  typedef QMap<pqPipelineSource*, QTreeWidgetItem*> DataProducerItemsMap;
  DataProducerItemsMap DataProducerItems;

  pqPipelineSource* dataProducer(QTreeWidgetItem* item) const
    {
    for (DataProducerItemsMap::const_iterator iter = this->DataProducerItems.begin();
      iter != this->DataProducerItems.end(); ++iter)
      {
      if (iter.value() == item)
        {
        return iter.key();
        }
      }
    return NULL;
    }
};

//-----------------------------------------------------------------------------
PipelineWidget::PipelineWidget(QWidget* parentObject)
   : Superclass(parentObject),
   Internals(new PipelineWidget::PWInternals())
{
  // track registration of new proxies to update the widget.
  pqServerManagerModel* smmodel = pqApplicationCore::instance()->getServerManagerModel();
  this->connect(smmodel, SIGNAL(sourceAdded(pqPipelineSource*)),
    SLOT(sourceAdded(pqPipelineSource*)));
  this->connect(smmodel, SIGNAL(sourceRemoved(pqPipelineSource*)),
    SLOT(sourceRemoved(pqPipelineSource*)));

  // track selection to update ActiveObjects.
  this->connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
    SLOT(currentItemChanged(QTreeWidgetItem*)));
  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(pqPipelineSource*)),
    SLOT(setCurrent(pqPipelineSource*)));
}

//-----------------------------------------------------------------------------
PipelineWidget::~PipelineWidget()
{
  // this->Internals is a QScopedPointer. No need to delete here.
}

//-----------------------------------------------------------------------------
void PipelineWidget::sourceAdded(pqPipelineSource* source)
{
  if (TEM::isDataProducer(source))
    {
    this->addDataProducer(source);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::sourceRemoved(pqPipelineSource* source)
{
  if (TEM::isDataProducer(source))
    {
    this->removeDataProducer(source);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::addDataProducer(pqPipelineSource* producer)
{
  Q_ASSERT(this->Internals->DataProducerItems.contains(producer) == false);

  QTreeWidgetItem* item = new QTreeWidgetItem();
  item->setText(0, TEM::label(producer));
  item->setIcon(0, QIcon(":/pqWidgets/Icons/pqInspect22.png"));
  this->addTopLevelItem(item);

  this->Internals->DataProducerItems[producer] = item;
}

//-----------------------------------------------------------------------------
void PipelineWidget::removeDataProducer(pqPipelineSource* producer)
{
  if (this->Internals->DataProducerItems.contains(producer))
    {
    int index = this->indexOfTopLevelItem(this->Internals->DataProducerItems[producer]);
    Q_ASSERT(index >= 0);
    QTreeWidgetItem* item = this->takeTopLevelItem(index);
    Q_ASSERT(item == this->Internals->DataProducerItems[producer]);
    delete item;

    this->Internals->DataProducerItems.remove(producer);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::currentItemChanged(QTreeWidgetItem* item)
{
  int index = this->indexOfTopLevelItem(item);
  if (index == -1)
    {
    // selected item is a plot. FIXME: determine the data producer for that plot
    // and activate it.
    }
  else
    {
    // selected item is a producer.
    pqPipelineSource* dataProducer = this->Internals->dataProducer(item);
    ActiveObjects::instance().setActiveDataSource(dataProducer);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::setCurrent(pqPipelineSource* source)
{
  if (QTreeWidgetItem* item = this->Internals->DataProducerItems.value(source, NULL))
    {
    this->setCurrentItem(item);
    }
}

//-----------------------------------------------------------------------------
} // end of namespace TEM
