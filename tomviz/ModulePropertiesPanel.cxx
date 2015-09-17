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

//-----------------------------------------------------------------------------
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

  this->connect(ui.SearchBox, SIGNAL(advancedSearchActivated(bool)),
                SLOT(updatePanel()));
  this->connect(ui.SearchBox, SIGNAL(textChanged(const QString&)),
                SLOT(updatePanel()));

  this->connect(ui.Delete, SIGNAL(clicked()), SLOT(deleteModule()));
  this->connect(ui.ProxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)),
                SLOT(render()));

  this->connect(ui.DetachColorMap, SIGNAL(clicked(bool)),
                SLOT(detachColorMap(bool)));
}

//-----------------------------------------------------------------------------
ModulePropertiesPanel::~ModulePropertiesPanel()
{
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::setModule(Module* module)
{
  this->Internals->ActiveModule = module;
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
  ui.ProxiesWidget->clear();
  ui.DetachColorMap->setVisible(false);
  if (module)
  {
    module->addToPanel(ui.ProxiesWidget);
    if (module->isColorMapNeeded())
    {
      ui.DetachColorMap->setVisible(true);
      ui.DetachColorMap->setChecked(module->useDetachedColorMap());
    }
  }
  ui.ProxiesWidget->updateLayout();
  this->updatePanel();
  ui.Delete->setEnabled(module != NULL);
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::setView(vtkSMViewProxy* view)
{
  this->Internals->Ui.ProxiesWidget->setView(
    tomviz::convert<pqView*>(view));
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::updatePanel()
{
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
  ui.ProxiesWidget->filterWidgets(ui.SearchBox->isAdvancedSearchActive(),
                                  ui.SearchBox->text());
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::deleteModule()
{
  ModuleManager::instance().removeModule(this->Internals->ActiveModule);
  this->render();
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::render()
{
  pqView* view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
  {
    view->render();
  }
}

//-----------------------------------------------------------------------------
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

}
