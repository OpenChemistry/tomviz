/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModulePropertiesPanel.h"
#include "ui_ModulePropertiesPanel.h"

#include "ActiveObjects.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Utilities.h"
#include "pqView.h"
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
  this->connect(&ActiveObjects::instance(), &ActiveObjects::moduleChanged,
                this, &ModulePropertiesPanel::setModule);
  this->connect(&ActiveObjects::instance(),
                QOverload<vtkSMViewProxy*>::of(&ActiveObjects::viewChanged),
                this, &ModulePropertiesPanel::setView);

  /* Disabled the search box for now, uncomment to enable again.
    this->connect(ui.SearchBox, &pqSearchBox::advancedSearchActivated,
                  this, &ModulePropertiesPanel::updatePanel);
    this->connect(ui.SearchBox, &pqSearchBox::textChanged,
                  this, &ModulePropertiesPanel::updatePanel);
  */

  this->connect(ui.DetachColorMap, &QAbstractButton::clicked, this,
                &ModulePropertiesPanel::detachColorMap);
}

ModulePropertiesPanel::~ModulePropertiesPanel() {}

void ModulePropertiesPanel::setModule(Module* module)
{
  if (module != this->Internals->ActiveModule) {
    if (this->Internals->ActiveModule) {
      DataSource* dataSource = this->Internals->ActiveModule->dataSource();
      if (dataSource) {
        QObject::disconnect(dataSource, &DataSource::dataChanged, this,
                            &ModulePropertiesPanel::updatePanel);
      }
      this->Internals->ActiveModule->prepareToRemoveFromPanel(this);
    }

    if (module) {
      DataSource* dataSource = module->dataSource();
      if (dataSource) {
        QObject::connect(dataSource, &DataSource::dataChanged, this,
                         &ModulePropertiesPanel::updatePanel);
      }
    }
  }

  this->Internals->ActiveModule = module;
  Ui::ModulePropertiesPanel& ui = this->Internals->Ui;

  deleteLayoutContents(ui.PropertiesWidget->layout());

  ui.DetachColorMapWidget->setVisible(false);
  if (module) {
    module->addToPanel(ui.PropertiesWidget);
    if (module->isColorMapNeeded()) {
      ui.DetachColorMapWidget->setVisible(true);
      ui.DetachColorMap->setChecked(module->useDetachedColorMap());
      ui.DetachColorMap->setEnabled(!module->isOpacityMapped());

      this->connect(module, &Module::opacityEnforced, this,
                    &ModulePropertiesPanel::onEnforcedOpacity);

      this->connect(module, &Module::colorMapChanged, this, [&]() {
        ui.DetachColorMap->setChecked(
          this->Internals->ActiveModule->useDetachedColorMap());
      });
    }
  }
  ui.PropertiesWidget->layout()->update();
  this->updatePanel();
}

void ModulePropertiesPanel::setView(vtkSMViewProxy* vtkNotUsed(view)) {}

void ModulePropertiesPanel::updatePanel() {}

void ModulePropertiesPanel::onEnforcedOpacity(bool val)
{
  auto ui = this->Internals->Ui;
  ui.DetachColorMap->setEnabled(!val);
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
} // namespace tomviz
