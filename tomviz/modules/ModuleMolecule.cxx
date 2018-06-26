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

  QFormLayout* layout = new QFormLayout;

  auto ballSlider = new DoubleSliderWidget(true);
  ballSlider->setLineEditWidth(50);
  ballSlider->setMaximum(4.0);
  ballSlider->setValue(m_moleculeMapper->GetAtomicRadiusScaleFactor());
  layout->addRow("Ball Radius", ballSlider);

  auto stickSlider = new DoubleSliderWidget(true);
  stickSlider->setLineEditWidth(50);
  stickSlider->setMaximum(2.0);
  stickSlider->setValue(m_moleculeMapper->GetBondRadius());
  layout->addRow("Stick Radius", stickSlider);

  panel->setLayout(layout);

  connect(ballSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleMolecule::ballRadiusChanged);

  connect(stickSlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleMolecule::bondRadiusChanged);
}

void ModuleMolecule::ballRadiusChanged(double val)
{
  m_moleculeMapper->SetAtomicRadiusScaleFactor(val);
  if (m_view) {
    m_view->GetRenderer()->Render();
  }
}

void ModuleMolecule::bondRadiusChanged(double val)
{
  m_moleculeMapper->SetBondRadius(val);
  if (m_view) {
    m_view->GetRenderer()->Render();
  }
}

QJsonObject ModuleMolecule::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["ballRadius"] = m_moleculeMapper->GetAtomicRadiusScaleFactor();
  props["stickRadius"] = m_moleculeMapper->GetBondRadius();

  json["properties"] = props;
  return json;
}

bool ModuleMolecule::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    ballRadiusChanged(props["ballRadius"].toDouble());
    bondRadiusChanged(props["stickRadius"].toDouble());
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
