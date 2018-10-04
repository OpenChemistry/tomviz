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
#include "MoleculeSource.h"

#include "ModuleFactory.h"
#include "ModuleManager.h"
#include "Pipeline.h"

#include <vtkMolecule.h>
#include <vtkSMViewProxy.h>

#include <QJsonArray>

namespace tomviz {

MoleculeSource::MoleculeSource(vtkMolecule* molecule, QObject* parent)
  : QObject(parent), m_molecule(vtkSmartPointer<vtkMolecule>::Take(molecule))
{
}

MoleculeSource::~MoleculeSource()
{
}

QJsonObject MoleculeSource::serialize() const
{
  QJsonObject json = m_json;
  // Serialize the modules...
  auto modules = ModuleManager::instance().findModulesGeneric(this, nullptr);
  QJsonArray jModules;
  foreach (Module* module, modules) {
    QJsonObject jModule = module->serialize();
    jModule["type"] = ModuleFactory::moduleType(module);
    jModule["viewId"] = static_cast<int>(module->view()->GetGlobalID());

    jModules.append(jModule);
  }
  if (!jModules.isEmpty()) {
    json["modules"] = jModules;
  }

  return json;
}

bool MoleculeSource::deserialize(const QJsonObject& state)
{
  // Check for modules on the data source first.
  if (state.contains("modules") && state["modules"].isArray()) {
    auto moduleArray = state["modules"].toArray();
    for (int i = 0; i < moduleArray.size(); ++i) {
      auto moduleObj = moduleArray[i].toObject();
      auto viewId = moduleObj["viewId"].toInt();
      auto viewProxy = ModuleManager::instance().lookupView(viewId);
      auto type = moduleObj["type"].toString();
      auto m =
        ModuleManager::instance().createAndAddModule(type, this, viewProxy);
      m->deserialize(moduleObj);
    }
  }
  return true;
}

void MoleculeSource::setFileName(const QString fileName)
{
  auto reader = m_json.value("reader").toObject(QJsonObject());
  QJsonValue file(fileName);
  reader["fileName"] = file;
  m_json["reader"] = reader;
}

/// Returns the name of the file used to load the data source.
QString MoleculeSource::fileName() const
{
  auto reader = m_json.value("reader").toObject(QJsonObject());
  QString file;
  if (reader.contains("fileName")) {
    file = reader["fileName"].toString();
  }
  return file;
}

/// Set the label for the data source.
void MoleculeSource::setLabel(const QString& label)
{
  m_json["label"] = label;
}

/// Returns the name of the filename used from the originalDataSource.
QString MoleculeSource::label() const
{
  if (m_json.contains("label")) {
    return m_json["label"].toString();
  } else {
    return QFileInfo(fileName()).baseName();
  }
}

vtkMolecule* MoleculeSource::molecule() const
{
  return m_molecule.GetPointer();
}

} // namespace tomviz
