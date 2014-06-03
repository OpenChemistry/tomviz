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
#include "ModuleManager.h"

#include "DataSource.h"
#include "Module.h"
#include "ModuleFactory.h"
#include <QPointer>

namespace TEM
{

class ModuleManager::MMInternals
{
public:
  QList<QPointer<DataSource> > DataSources;
  QList<QPointer<Module> > Modules;
};

//-----------------------------------------------------------------------------
ModuleManager::ModuleManager(QObject* parentObject)
  : Superclass(parentObject),
  Internals(new ModuleManager::MMInternals())
{
}

//-----------------------------------------------------------------------------
ModuleManager::~ModuleManager()
{
  // Internals is a QScopedPointer.
}

//-----------------------------------------------------------------------------
ModuleManager& ModuleManager::instance()
{
  static ModuleManager theInstance;
  return theInstance;
}

//-----------------------------------------------------------------------------
void ModuleManager::addDataSource(DataSource* dataSource)
{
  if (dataSource && !this->Internals->DataSources.contains(dataSource))
    {
    dataSource->setParent(this);
    this->Internals->DataSources.push_back(dataSource);
    emit this->dataSourceAdded(dataSource);
    }
}

//-----------------------------------------------------------------------------
void ModuleManager::removeDataSource(DataSource* dataSource)
{
  if (this->Internals->DataSources.removeOne(dataSource))
    {
    emit this->dataSourceRemoved(dataSource);
    delete dataSource;
    }
}

//-----------------------------------------------------------------------------
void ModuleManager::addModule(Module* module)
{
  if (!this->Internals->Modules.contains(module))
    {
    module->setParent(this);
    this->Internals->Modules.push_back(module);
    emit this->moduleAdded(module);
    }
}

//-----------------------------------------------------------------------------
void ModuleManager::removeModule(Module* module)
{
  if (this->Internals->Modules.removeOne(module))
    {
    emit this->moduleRemoved(module);
    delete module;
    }
}

//-----------------------------------------------------------------------------
void ModuleManager::removeAllModules()
{
  foreach (Module* module, this->Internals->Modules)
    {
    emit this->moduleRemoved(module);
    delete module;
    }
}

//-----------------------------------------------------------------------------
Module* ModuleManager::createAndAddModule(
  const QString& type, DataSource* dataSource,
  vtkSMViewProxy* view)
{
  if (!view || !dataSource)
    {
    return NULL;
    }

  // Create an outline module for the source in the active view.
  Module* module = ModuleFactory::createModule(type, dataSource, view);
  if (module)
    {
    this->addModule(module);
    }
  return module;
}

//-----------------------------------------------------------------------------
QList<Module*> ModuleManager::findModulesGeneric(
  DataSource* dataSource, vtkSMViewProxy* view)
{
  QList<Module*> modules;
  foreach (Module* module, this->Internals->Modules)
    {
    if (module && module->dataSource() == dataSource &&
      (view == NULL || view == module->view()))
      {
      modules.push_back(module);
      }
    }
  return modules;
}


} // end of namesapce TEM
