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
#include "ModulePropertiesPanel.h"
#include "ui_ModulePropertiesPanel.h"

#include "ActiveObjects.h"
#include "Module.h"
#include "ModuleManager.h"
#include "pqView.h"
#include "Utilities.h"
#include "vtkSMViewProxy.h"

namespace tomviz
{

class ModulePropertiesPanel::MPPInternals
{
public:
  Ui::ModulePropertiesPanel Ui;
  QPointer<Module> ActiveModule;
};

ModulePropertiesPanel::ModulePropertiesPanel(QWidget* parentObject)
  : Superclass(parentObject),
    Internals(new ModulePropertiesPanel::MPPInternals())
{
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
  ui.setupUi(this);

  // Show active module in the "Module Properties" panel.
  this->connect(&ActiveObjects::instance(), SIGNAL(moduleChanged(Module*)),
                SLOT(setModule(Module*)));
  this->connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(setView(vtkSMViewProxy*)));

/* Disabled the search box for now, uncomment to enable again.
  this->connect(ui.SearchBox, SIGNAL(advancedSearchActivated(bool)),
                SLOT(updatePanel()));
  this->connect(ui.SearchBox, SIGNAL(textChanged(const QString&)),
                SLOT(updatePanel()));
*/
  this->connect(ui.Delete, SIGNAL(clicked()), SLOT(deleteModule()));
  this->connect(ui.ProxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)),
                SLOT(render()));

  this->connect(ui.DetachColorMap, SIGNAL(clicked(bool)),
                SLOT(detachColorMap(bool)));
  this->connect(ui.ColorByLabelMap, SIGNAL(clicked(bool)),
                SLOT(colorByLabelMap(bool)));
}

ModulePropertiesPanel::~ModulePropertiesPanel()
{
}

void ModulePropertiesPanel::setModule(Module* module)
{
  if (module != this->Internals->ActiveModule)
  {
    if (this->Internals->ActiveModule)
    {
      DataSource* dataSource = this->Internals->ActiveModule->dataSource();
      QObject::disconnect(dataSource, SIGNAL(dataChanged()),
                          this, SLOT(updatePanel()));
    }

    if (module)
    {
      DataSource* dataSource = module->dataSource();
      QObject::connect(dataSource, SIGNAL(dataChanged()),
                       this, SLOT(updatePanel()));
    }
  }

  this->Internals->ActiveModule = module;
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
  ui.ProxiesWidget->clear();
  ui.DetachColorMap->setVisible(false);
  ui.ColorByLabelMap->setVisible(false);
  if (module)
  {
    module->addToPanel(ui.ProxiesWidget);
    if (module->isColorMapNeeded())
    {
      ui.DetachColorMap->setVisible(true);
      ui.DetachColorMap->setChecked(module->useDetachedColorMap());
      ui.ColorByLabelMap->setVisible(true);
      ui.ColorByLabelMap->setChecked(module->colorByLabelMap());
    }
  }
  ui.ProxiesWidget->updateLayout();
  this->updatePanel();
  ui.Delete->setEnabled(module != nullptr);
}

void ModulePropertiesPanel::setView(vtkSMViewProxy* view)
{
  this->Internals->Ui.ProxiesWidget->setView(
    tomviz::convert<pqView*>(view));
}

void ModulePropertiesPanel::updatePanel()
{
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
//  ui.ProxiesWidget->filterWidgets(ui.SearchBox->isAdvancedSearchActive(),
//                                  ui.SearchBox->text());

  if (this->Internals->ActiveModule)
  {
    DataSource* dataSource = this->Internals->ActiveModule->dataSource();
    ui.ColorByLabelMap->setVisible(dataSource && dataSource->hasLabelMap());
  }
}

void ModulePropertiesPanel::deleteModule()
{
  ModuleManager::instance().removeModule(this->Internals->ActiveModule);
  this->render();
}

void ModulePropertiesPanel::render()
{
  pqView* view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
  {
    view->render();
  }
}

void ModulePropertiesPanel::detachColorMap(bool val)
{
  Module* module = this->Internals->ActiveModule;
  if (module)
  {
    module->setUseDetachedColorMap(val);
    this->setModule(module); // refreshes the module.
    this->render();
  }
}

void ModulePropertiesPanel::colorByLabelMap(bool val)
{
  Module* module = this->Internals->ActiveModule;
  if (module)
  {
    module->setColorByLabelMap(val);
    this->setModule(module); // refreshs the module.
    this->render();
  }
}

}
