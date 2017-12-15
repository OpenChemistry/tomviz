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
#include "Utilities.h"
#include "pqView.h"

#include "vtkAlgorithm.h"
#include "vtkDataArray.h"
#include "vtkDataSet.h"
#include "vtkPointData.h"
#include "vtkSMViewProxy.h"

namespace tomviz {

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
  this->connect(&ActiveObjects::instance(),
                SIGNAL(viewChanged(vtkSMViewProxy*)),
                SLOT(setView(vtkSMViewProxy*)));

  /* Disabled the search box for now, uncomment to enable again.
    this->connect(ui.SearchBox, SIGNAL(advancedSearchActivated(bool)),
                  SLOT(updatePanel()));
    this->connect(ui.SearchBox, SIGNAL(textChanged(const QString&)),
                  SLOT(updatePanel()));
  */

  this->connect(ui.DetachColorMap, SIGNAL(clicked(bool)),
                SLOT(detachColorMap(bool)));
}

ModulePropertiesPanel::~ModulePropertiesPanel()
{
}

void ModulePropertiesPanel::setModule(Module* module)
{
  if (module != this->Internals->ActiveModule) {
    if (this->Internals->ActiveModule) {
      DataSource* dataSource = this->Internals->ActiveModule->dataSource();
      QObject::disconnect(dataSource, SIGNAL(dataChanged()), this,
                          SLOT(updatePanel()));
      this->Internals->ActiveModule->prepareToRemoveFromPanel(this);
    }

    if (module) {
      DataSource* dataSource = module->dataSource();
      QObject::connect(dataSource, SIGNAL(dataChanged()), this,
                       SLOT(updatePanel()));
    }
  }

  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;
  this->Internals->ActiveModule = module;

  QObject::disconnect(ui.ArrayNames, SIGNAL(activated(QString)), this,
    SLOT(arrayNameChanged(QString)));

  deleteLayoutContents(ui.PropertiesWidget->layout());

  ui.DetachColorMapWidget->setVisible(false);
  if (module) {
    module->addToPanel(ui.PropertiesWidget);
    if (module->isColorMapNeeded()) {
      ui.DetachColorMapWidget->setVisible(true);
      ui.DetachColorMap->setChecked(module->useDetachedColorMap());

      this->connect(module, &Module::colorMapChanged, this, [&]() {
        ui.DetachColorMap->setChecked(
          this->Internals->ActiveModule->useDetachedColorMap());
      });
    }

    // Add data arrays to the array name combo box
    ui.ArrayNames->clear();
    auto dataSource = module->dataSource();
    auto producer = dataSource->producer();
    auto tp = vtkAlgorithm::SafeDownCast(producer->GetClientSideObject());
    auto data = vtkDataSet::SafeDownCast(tp->GetOutputDataObject(0));
    if (data) {
      // Limit to point data
      auto pointData = data->GetPointData();
      for (int i = 0; i < pointData->GetNumberOfArrays(); ++i) {
        auto arrayName = pointData->GetArray(i)->GetName();
        if (arrayName) {
          ui.ArrayNames->addItem(arrayName);
        }
      }
    }
    auto currentArrayName = module->activeArray(0);
    int index = ui.ArrayNames->findText(currentArrayName);
    if (index >= 0) {
      ui.ArrayNames->setCurrentIndex(index);
    }
    QObject::connect(ui.ArrayNames, SIGNAL(activated(QString)), this,
                     SLOT(arrayNameChanged(QString)));
  }
  ui.PropertiesWidget->layout()->update();
  this->updatePanel();
}

void ModulePropertiesPanel::setView(vtkSMViewProxy* vtkNotUsed(view))
{
}

void ModulePropertiesPanel::updatePanel()
{
}

void ModulePropertiesPanel::detachColorMap(bool val)
{
  Module* module = this->Internals->ActiveModule;
  if (module) {
    module->setUseDetachedColorMap(val);
    this->setModule(module); // refreshes the module.
    emit module->renderNeeded();
  }
}

void ModulePropertiesPanel::arrayNameChanged(const QString& name)
{
  auto module = this->Internals->ActiveModule;
  if (module) {
    module->setActiveArray(0, name);
    this->setModule(module);
    emit module->renderNeeded();
  }
}
}
