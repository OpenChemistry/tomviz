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
#include "ModuleMolecule.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "OperatorResult.h"
#include "Utilities.h"
#include "vtkMolecule.h"
#include "vtkNew.h"
#include "vtkRenderer.h"
#include "vtkSMViewProxy.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
namespace tomviz {

ModuleMolecule::ModuleMolecule(QObject* parentObject) : Module(parentObject)
{
}

ModuleMolecule::~ModuleMolecule()
{
  finalize();
}

QIcon ModuleMolecule::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqGroup24.png");
}

bool ModuleMolecule::initializeWithResult(DataSource* dataSource,
                                          vtkSMViewProxy* view,
                                          OperatorResult* result)
{
  if (!Module::initializeWithResult(dataSource, view, result)) {
    return false;
  }

  vtkMolecule* molecule = vtkMolecule::SafeDownCast(result->dataObject());
  if (molecule == nullptr) {
    return false;
  }

  m_moleculeMapper->SetInputData(molecule);
  m_moleculeMapper->UseBallAndStickSettings();
  m_moleculeActor->SetMapper(m_moleculeMapper);

  m_view = vtkPVRenderView::SafeDownCast(view->GetClientSideView());
  m_view->GetRenderer()->AddActor(m_moleculeActor);
  m_view->Update();

  return true;
}

bool ModuleMolecule::finalize()
{
  if (m_view) {
    m_view->GetRenderer()->RemoveActor(m_moleculeActor);
  }
  return true;
}

bool ModuleMolecule::setVisibility(bool val)
{
  m_moleculeActor->SetVisibility(val);
  m_view->Update();
  return true;
}

bool ModuleMolecule::visibility() const
{
  return m_moleculeActor->GetVisibility();
}

void ModuleMolecule::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;

  panel->setLayout(layout);
}

void ModuleMolecule::dataUpdated()
{
}

QJsonObject ModuleMolecule::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  json["properties"] = props;
  return json;
}

bool ModuleMolecule::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    return true;
  }
  return false;
}

void ModuleMolecule::dataSourceMoved(double, double, double)
{
}

//-----------------------------------------------------------------------------
bool ModuleMolecule::isProxyPartOfModule(vtkSMProxy*)
{
  return false;
}

std::string ModuleMolecule::getStringForProxy(vtkSMProxy*)
{
  return "";
}

vtkSMProxy* ModuleMolecule::getProxyForString(const std::string&)
{
  return nullptr;
}

} // namespace tomviz
