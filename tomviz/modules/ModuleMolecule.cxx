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
#include "IntSliderWidget.h"
#include "Utilities.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"
#include "vtkAlgorithm.h"
#include "vtkImageData.h"
#include "vtkImageReslice.h"
#include "vtkMolecule.h"
#include "vtkNew.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"
#include <vtkPVDiscretizableColorTransferFunction.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
namespace tomviz {

ModuleMolecule::ModuleMolecule(QObject* parentObject)
  : Module(parentObject)
{}

ModuleMolecule::~ModuleMolecule()
{
  finalize();
}

QIcon ModuleMolecule::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqGroup24.png");
}

bool ModuleMolecule::initialize(DataSource* dataSource,
                                       vtkSMViewProxy* view, OperatorResult* result)
{
  qDebug() << "Initializing ModuleMolecule";
  return Module::initialize(dataSource, view, result);
}

bool ModuleMolecule::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_representation);
  // controller->UnRegisterProxy(m_passThrough);

  // m_passThrough = nullptr;
  m_representation = nullptr;
  return true;
}

bool ModuleMolecule::setVisibility(bool val)
{
  return true;
}

bool ModuleMolecule::visibility() const
{
  return false;
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
  m_links.accept();
  emit renderNeeded();
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
  if (json["properties"].isObject() && m_representation) {
    return true;
  }
  return false;
}

void ModuleMolecule::dataSourceMoved(double newX, double newY,
                                            double newZ)
{
  double pos[3] = { newX, newY, newZ };

}

//-----------------------------------------------------------------------------
bool ModuleMolecule::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return false;
}

std::string ModuleMolecule::getStringForProxy(vtkSMProxy* proxy)
{
  return "Representation";
  // if (proxy == m_representation.Get()) {
  //   return "Representation";
  // } else {
  //   qWarning(
  //     "Unknown proxy passed to module molecule");
  //   return "";
  // }
}

vtkSMProxy* ModuleMolecule::getProxyForString(const std::string& str)
{
  return nullptr;
  // if (str == "Representation") {
  //   return m_representation.Get();
  // } else {
  //   return nullptr;
  // }
}

} // namespace tomviz
