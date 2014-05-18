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
#include "ModulePropertiesPanel.h"
#include "ui_ModulePropertiesPanel.h"

#include "ActiveObjects.h"
#include "Module.h"
#include "ModuleManager.h"
#include "pqView.h"
#include "Utilities.h"
#include "vtkSMViewProxy.h"

namespace TEM
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

  this->connect(ui.AdvancedButton, SIGNAL(toggled(bool)), SLOT(updatePanel()));
  this->connect(ui.SearchLineEdit, SIGNAL(textChanged(QString)), SLOT(updatePanel()));
  this->connect(ui.Delete, SIGNAL(clicked()), SLOT(deleteModule()));
  this->connect(ui.ProxiesWidget, SIGNAL(changeFinished(vtkSMProxy*)), SLOT(render()));
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
  if (module)
    {
    module->addToPanel(ui.ProxiesWidget);
    }
  ui.ProxiesWidget->updateLayout();
  this->updatePanel();
  ui.Delete->setEnabled(module != NULL);
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::setView(vtkSMViewProxy* view)
{
  this->Internals->Ui.ProxiesWidget->setView(
    TEM::convert<pqView*>(view));
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::updatePanel()
{
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
  ui.ProxiesWidget->filterWidgets(
    ui.AdvancedButton->isChecked(),
    ui.SearchLineEdit->text());
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::deleteModule()
{
  ModuleManager::instance().removeModule(
    this->Internals->ActiveModule);
  this->render();
}

//-----------------------------------------------------------------------------
void ModulePropertiesPanel::render()
{
  pqView* view = TEM::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view)
    {
    view->render();
    }
}

}
