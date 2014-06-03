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

#include "DataSource.h"
#include "ActiveObjects.h"
#include "Module.h"
#include "ModuleManager.h"
#include "pqApplicationCore.h"
#include "pqPipelineSource.h"
#include "pqServerManagerModel.h"
#include "pqView.h"
#include "Utilities.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include <QHeaderView>

namespace TEM
{

static const int EYE_COLUMN = 0;
static const int MODULE_COLUMN = 1;

class PipelineWidget::PWInternals
{
public:
  typedef QMap<DataSource*, QTreeWidgetItem*> DataProducerItemsMap;
  DataProducerItemsMap DataProducerItems;

  typedef QMap<Module*, QTreeWidgetItem*> ModuleItemsMap;
  ModuleItemsMap ModuleItems;

  DataSource* dataProducer(QTreeWidgetItem* item) const
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

  Module* module(QTreeWidgetItem* item) const
    {
    for (ModuleItemsMap::const_iterator iter = this->ModuleItems.begin();
         iter != this->ModuleItems.end(); ++iter)
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
  // track selection to update ActiveObjects.
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(setCurrent(DataSource*)));
  this->connect(&ActiveObjects::instance(), SIGNAL(moduleChanged(Module*)),
                SLOT(setCurrent(Module*)));
  this->connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(setActiveView(vtkSMViewProxy*)));

  // update ActiveObjects when user interacts.
  this->connect(this,
                SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
                SLOT(currentItemChanged(QTreeWidgetItem*)));

  // toggle module visibility.
  this->connect(this,
                SIGNAL(itemClicked(QTreeWidgetItem*, int)),
                SLOT(onItemClicked(QTreeWidgetItem*, int)));

  // track ModuleManager.
  this->connect(&ModuleManager::instance(), SIGNAL(moduleAdded(Module*)),
                SLOT(moduleAdded(Module*)));
  this->connect(&ModuleManager::instance(), SIGNAL(moduleRemoved(Module*)),
                SLOT(moduleRemoved(Module*)));

  this->connect(&ModuleManager::instance(), SIGNAL(dataSourceAdded(DataSource*)),
                SLOT(dataSourceAdded(DataSource*)));
  this->connect(&ModuleManager::instance(), SIGNAL(dataSourceRemoved(DataSource*)),
                SLOT(dataSourceRemoved(DataSource*)));

  this->header()->setStretchLastSection(true);
}

//-----------------------------------------------------------------------------
PipelineWidget::~PipelineWidget()
{
  // this->Internals is a QScopedPointer. No need to delete here.
}

//-----------------------------------------------------------------------------
void PipelineWidget::dataSourceAdded(DataSource* datasource)
{
  Q_ASSERT(this->Internals->DataProducerItems.contains(datasource) == false);

  QTreeWidgetItem* item = new QTreeWidgetItem();
  item->setText(MODULE_COLUMN, TEM::label(datasource->producer()));
  item->setIcon(MODULE_COLUMN, QIcon(":/pqWidgets/Icons/pqInspect22.png"));
  item->setIcon(EYE_COLUMN, QIcon());
  item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
  this->addTopLevelItem(item);

  this->Internals->DataProducerItems[datasource] = item;
}

//-----------------------------------------------------------------------------
void PipelineWidget::dataSourceRemoved(DataSource* datasource)
{
  if (this->Internals->DataProducerItems.contains(datasource))
    {
    int index = this->indexOfTopLevelItem(this->Internals->DataProducerItems[datasource]);
    Q_ASSERT(index >= 0);
    QTreeWidgetItem* item = this->takeTopLevelItem(index);
    Q_ASSERT(item == this->Internals->DataProducerItems[datasource]);
    delete item;

    this->Internals->DataProducerItems.remove(datasource);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::moduleAdded(Module* module)
{
  Q_ASSERT(module);

  DataSource* dataSource = module->dataSource();
  Q_ASSERT(dataSource);
  Q_ASSERT(this->Internals->DataProducerItems.contains(dataSource));

  QTreeWidgetItem* parentItem = this->Internals->DataProducerItems[dataSource];
  QTreeWidgetItem* child = new QTreeWidgetItem(parentItem);
  child->setText(MODULE_COLUMN, module->label());
  child->setIcon(MODULE_COLUMN, module->icon());
  child->setIcon(EYE_COLUMN,
    module->visibility()?
    QIcon(":/pqWidgets/Icons/pqEyeball16.png"):
    QIcon(":/pqWidgets/Icons/pqEyeballd16.png"));
  parentItem->setExpanded(true);

  this->Internals->ModuleItems[module] = child;
}

//-----------------------------------------------------------------------------
void PipelineWidget::moduleRemoved(Module* module)
{
  Q_ASSERT(module);

  DataSource* dataSource = module->dataSource();
  Q_ASSERT(dataSource);
  Q_ASSERT(this->Internals->DataProducerItems.contains(dataSource));
  Q_ASSERT(this->Internals->ModuleItems.contains(module));

  QTreeWidgetItem* parentItem = this->Internals->DataProducerItems[dataSource];
  QTreeWidgetItem* child = this->Internals->ModuleItems[module];
  this->Internals->ModuleItems.remove(module);

  parentItem->removeChild(child);
  delete child;
}

//-----------------------------------------------------------------------------
void PipelineWidget::onItemClicked(QTreeWidgetItem* item, int col)
{
  int index = this->indexOfTopLevelItem(item);
  if (index == -1 && // selected item is a plot.
      col == EYE_COLUMN)
    {
    Module* module = this->Internals->module(item);
    module->setVisibility(!module->visibility());
    item->setIcon(EYE_COLUMN,
      module->visibility()?
      QIcon(":/pqWidgets/Icons/pqEyeball16.png"):
      QIcon(":/pqWidgets/Icons/pqEyeballd16.png"));
    if (pqView* view = TEM::convert<pqView*>(module->view()))
      {
      view->render();
      }
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
    Module* module = this->Internals->module(item);
    ActiveObjects::instance().setActiveModule(module);
    }
  else
    {
    // selected item is a producer.
    DataSource* dataProducer = this->Internals->dataProducer(item);
    ActiveObjects::instance().setActiveDataSource(dataProducer);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::setCurrent(DataSource* source)
{
  if (QTreeWidgetItem* item = this->Internals->DataProducerItems.value(source, NULL))
    {
    this->setCurrentItem(item);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::setCurrent(Module* module)
{
  if (QTreeWidgetItem* item = this->Internals->ModuleItems.value(module, NULL))
    {
    this->setCurrentItem(item);
    }
}

//-----------------------------------------------------------------------------
void PipelineWidget::setActiveView(vtkSMViewProxy* view)
{
  for (PWInternals::ModuleItemsMap::iterator iter =
    this->Internals->ModuleItems.begin();
    iter != this->Internals->ModuleItems.end(); ++iter)
    {
    Module* module = iter.key();
    QTreeWidgetItem* item = iter.value();

    bool item_enabled = (view == module->view());
    item->setDisabled(!item_enabled);

    QFont font = item->font(MODULE_COLUMN);
    font.setItalic(!item_enabled);
    item->setFont(MODULE_COLUMN, font);
    }
}

} // end of namespace TEM
