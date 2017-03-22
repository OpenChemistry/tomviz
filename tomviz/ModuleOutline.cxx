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
#include "ModuleOutline.h"

#include "DataSource.h"
#include "Utilities.h"
#include "pqProxiesWidget.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"
#include <pqColorChooserButton.h>
#include <vtkGridAxes3DActor.h>
#include <vtkPVRenderView.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

using pugi::xml_attribute;
using pugi::xml_node;

ModuleOutline::ModuleOutline(QObject* parentObject) : Superclass(parentObject)
{
}

ModuleOutline::~ModuleOutline()
{
  this->finalize();
}

QIcon ModuleOutline::icon() const
{
  return QIcon(":/icons/pqProbeLocation.png");
}

bool ModuleOutline::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!this->Superclass::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  vtkSMSessionProxyManager* pxm = data->producer()->GetSessionProxyManager();

  // Create the outline filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "OutlineFilter"));

  this->OutlineFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(this->OutlineFilter);
  controller->PreInitializeProxy(this->OutlineFilter);
  vtkSMPropertyHelper(this->OutlineFilter, "Input").Set(data->producer());
  controller->PostInitializeProxy(this->OutlineFilter);
  controller->RegisterPipelineProxy(this->OutlineFilter);

  // Create the representation for it.
  this->OutlineRepresentation =
    controller->Show(this->OutlineFilter, 0, vtkView);
  vtkSMPropertyHelper(this->OutlineRepresentation, "Position")
    .Set(data->displayPosition(), 3);
  Q_ASSERT(this->OutlineRepresentation);
  // vtkSMPropertyHelper(this->OutlineRepresentation,
  //                    "Representation").Set("Outline");
  this->OutlineRepresentation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  // Init the grid axes
  initializeGridAxes(data, vtkView);
  updateGridAxesColor(offWhite);

  return true;
}

bool ModuleOutline::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(this->OutlineRepresentation);
  controller->UnRegisterProxy(this->OutlineFilter);

  if (m_view) {
    m_view->GetRenderer()->RemoveActor(m_gridAxes.Get());
  }

  this->OutlineFilter = nullptr;
  this->OutlineRepresentation = nullptr;
  return true;
}

bool ModuleOutline::serialize(pugi::xml_node& ns) const
{
  xml_node rootNode = ns.append_child("properties");

  xml_node visibilityNode = rootNode.append_child("visibility");
  visibilityNode.append_attribute("enabled") = visibility();

  xml_node gridAxesNode = rootNode.append_child("grid_axes");
  gridAxesNode.append_attribute("enabled") = m_gridAxes->GetVisibility() > 0;
  gridAxesNode.append_attribute("grid") = m_gridAxes->GetGenerateGrid();

  xml_node color = gridAxesNode.append_child("color");
  double rgb[3];
  m_gridAxes->GetProperty()->GetDiffuseColor(rgb);
  color.append_attribute("r") = rgb[0];
  color.append_attribute("g") = rgb[1];
  color.append_attribute("b") = rgb[2];

  return true;
}

bool ModuleOutline::deserialize(const pugi::xml_node& ns)
{
  xml_node rootNode = ns.child("properties");
  if (!rootNode) {
    return false;
  }

  xml_node node = rootNode.child("visibility");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      setVisibility(att.as_bool());
    }
  }

  node = rootNode.child("grid_axes");
  if (node) {
    xml_attribute att = node.attribute("enabled");
    if (att) {
      m_gridAxes->SetVisibility(att.as_bool() ? 1 : 0);
    }
    att = node.attribute("grid");
    if (att) {
      m_gridAxes->SetGenerateGrid(att.as_bool());
    }
    xml_node color = node.child("color");
    if (color) {
      double rgb[3];
      att = color.attribute("r");
      if (att) {
        rgb[0] = att.as_double();
      }
      att = color.attribute("g");
      if (att) {
        rgb[1] = att.as_double();
      }
      att = color.attribute("b");
      if (att) {
        rgb[2] = att.as_double();
      }
      updateGridAxesColor(rgb);
    }
  }

  return Module::deserialize(ns);
}

bool ModuleOutline::setVisibility(bool val)
{
  Q_ASSERT(this->OutlineRepresentation);
  vtkSMPropertyHelper(this->OutlineRepresentation, "Visibility")
    .Set(val ? 1 : 0);
  this->OutlineRepresentation->UpdateVTKObjects();
  m_gridAxes->SetVisibility(val ? 1 : 0);
  return true;
}

bool ModuleOutline::visibility() const
{
  if (this->OutlineRepresentation) {
    return vtkSMPropertyHelper(this->OutlineRepresentation, "Visibility")
             .GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleOutline::addToPanel(QWidget* panel)
{
  Q_ASSERT(panel && this->OutlineRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QHBoxLayout* layout = new QHBoxLayout;
  QLabel* label = new QLabel("Color");
  layout->addWidget(label);
  layout->addStretch();
  pqColorChooserButton* colorSelector = new pqColorChooserButton(panel);
  colorSelector->setShowAlphaChannel(false);
  layout->addWidget(colorSelector);

  // Show Grid?
  QHBoxLayout* showGridLayout = new QHBoxLayout;
  QCheckBox* showGrid = new QCheckBox(QString("Show Grid"));
  showGrid->setChecked(m_gridAxes->GetGenerateGrid());

  connect(showGrid, &QCheckBox::stateChanged, this, [this](int state) {
    this->m_gridAxes->SetGenerateGrid(state == Qt::Checked);
    emit this->renderNeeded();
  });

  showGridLayout->addWidget(showGrid);

  // Show Axes?
  QHBoxLayout* showAxesLayout = new QHBoxLayout;
  QCheckBox* showAxes = new QCheckBox(QString("Show Axes"));
  showAxes->setChecked(m_gridAxes->GetVisibility());
  // Disable "Show Grid" if axes not enabled
  if (!showAxes->isChecked()) {
    showGrid->setEnabled(false);
  }
  connect(showAxes, &QCheckBox::stateChanged, this,
          [this, showGrid](int state) {
            this->m_gridAxes->SetVisibility(state == Qt::Checked);
            // Uncheck "Show Grid" and disable it
            if (state == Qt::Unchecked) {
              showGrid->setChecked(false);
              showGrid->setEnabled(false);
            } else {
              showGrid->setEnabled(true);
            }

            emit this->renderNeeded();
          });
  showAxesLayout->addWidget(showAxes);

  QVBoxLayout* panelLayout = new QVBoxLayout;
  panelLayout->addItem(layout);
  panelLayout->addItem(showAxesLayout);
  panelLayout->addItem(showGridLayout);
  panelLayout->addStretch();
  panel->setLayout(panelLayout);

  m_links.addPropertyLink(
    colorSelector, "chosenColorRgbF", SIGNAL(chosenColorChanged(const QColor&)),
    this->OutlineRepresentation,
    this->OutlineRepresentation->GetProperty("DiffuseColor"));

  this->connect(colorSelector, &pqColorChooserButton::chosenColorChanged,
                [this](const QColor& color) {
                  double rgb[3];
                  rgb[0] = color.redF();
                  rgb[1] = color.greenF();
                  rgb[2] = color.blueF();
                  updateGridAxesColor(rgb);

                });
  this->connect(colorSelector, &pqColorChooserButton::chosenColorChanged, this,
                &ModuleOutline::dataUpdated);
}

void ModuleOutline::dataUpdated()
{
  m_links.accept();
  emit this->renderNeeded();
}

void ModuleOutline::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(this->OutlineRepresentation, "Position").Set(pos, 3);
  this->OutlineRepresentation->UpdateVTKObjects();
  this->m_gridAxes->SetPosition(newX, newY, newZ);
}

//-----------------------------------------------------------------------------
bool ModuleOutline::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == this->OutlineFilter.Get()) ||
         (proxy == this->OutlineRepresentation.Get());
}

std::string ModuleOutline::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == this->OutlineFilter.Get()) {
    return "Outline";
  } else if (proxy == this->OutlineRepresentation.Get()) {
    return "Representation";
  } else {
    qWarning("Unknown proxy passed to module outline in save animation");
    return "";
  }
}

vtkSMProxy* ModuleOutline::getProxyForString(const std::string& str)
{
  if (str == "Outline") {
    return this->OutlineFilter.Get();
  } else if (str == "Representation") {
    return this->OutlineRepresentation.Get();
  } else {
    return nullptr;
  }
}

void ModuleOutline::updateGridAxesBounds(DataSource* dataSource)
{
  Q_ASSERT(dataSource);
  double bounds[6];
  dataSource->getBounds(bounds);
  m_gridAxes->SetGridBounds(bounds);
}
void ModuleOutline::initializeGridAxes(DataSource* data,
                                       vtkSMViewProxy* vtkView)
{

  updateGridAxesBounds(data);
  m_gridAxes->SetVisibility(0);
  m_gridAxes->SetGenerateGrid(false);

  // Work around a bug in vtkGridAxes3DActor. GetProperty() returns the
  // vtkProperty associated with a single face, so to get a property associated
  // with all the faces, we need to create a new one and set it.
  vtkNew<vtkProperty> prop;
  prop->DeepCopy(m_gridAxes->GetProperty());
  this->m_gridAxes->SetProperty(prop.Get());

  // Set the titles
  updateGridAxesUnit(data);

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_view->GetRenderer()->AddActor(m_gridAxes.Get());

  connect(data, &DataSource::dataPropertiesChanged, this, [this]() {
    auto dataSource = qobject_cast<DataSource*>(sender());
    this->updateGridAxesBounds(dataSource);
    this->updateGridAxesUnit(dataSource);
    dataSource->producer()->MarkModified(nullptr);
    dataSource->producer()->UpdatePipeline();
    emit this->renderNeeded();

  });
}

void ModuleOutline::updateGridAxesColor(double* color)
{
  for (int i = 0; i < 6; i++) {
    vtkNew<vtkTextProperty> prop;
    prop->SetColor(color);
    m_gridAxes->SetTitleTextProperty(i, prop.Get());
    m_gridAxes->SetLabelTextProperty(i, prop.Get());
  }
  m_gridAxes->GetProperty()->SetDiffuseColor(color);
  vtkSMPropertyHelper(this->OutlineRepresentation, "DiffuseColor")
    .Set(color, 3);
  this->OutlineRepresentation->UpdateVTKObjects();
}

void ModuleOutline::updateGridAxesUnit(DataSource* dataSource)
{
  QString xTitle = QString("X (%1)").arg(dataSource->getUnits(0));
  QString yTitle = QString("Y (%1)").arg(dataSource->getUnits(1));
  QString zTitle = QString("Z (%1)").arg(dataSource->getUnits(2));
  m_gridAxes->SetXTitle(xTitle.toUtf8().data());
  m_gridAxes->SetYTitle(yTitle.toUtf8().data());
  m_gridAxes->SetZTitle(zTitle.toUtf8().data());
}

} // end of namespace tomviz
