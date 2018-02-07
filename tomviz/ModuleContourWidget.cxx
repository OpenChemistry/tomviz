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
#include "ModuleContourWidget.h"
#include "ui_LightingParametersForm.h"
#include "ui_ModuleContourWidget.h"

#include "vtkSMProxy.h"
#include "vtkSMSourceProxy.h"

#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"

namespace tomviz {

ModuleContourWidget::ModuleContourWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleContourWidget),
    m_uiLighting(new Ui::LightingParametersForm)
{
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  m_uiLighting->gbLighting->setCheckable(false);
  QWidget::layout()->addWidget(lightingWidget);
  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  m_ui->colorChooser->setShowAlphaChannel(false);

  const int leWidth = 50;
  m_ui->sliValue->setLineEditWidth(leWidth);
  m_ui->sliOpacity->setLineEditWidth(leWidth);
  m_uiLighting->sliAmbient->setLineEditWidth(leWidth);
  m_uiLighting->sliDiffuse->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecular->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecularPower->setLineEditWidth(leWidth);

  m_uiLighting->sliSpecularPower->setMaximum(150);
  m_uiLighting->sliSpecularPower->setMinimum(1);
  m_uiLighting->sliSpecularPower->setResolution(200);

  QStringList labelsRepre;
  labelsRepre << tr("Surface") << tr("Wireframe") << tr("Points");
  m_ui->cbRepresentation->addItems(labelsRepre);

  connect(m_ui->cbColorMapData, SIGNAL(toggled(bool)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->sliColorArrayMin, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->sliColorArrayMax, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_uiLighting->sliAmbient, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_uiLighting->sliDiffuse, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_uiLighting->sliSpecular, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_uiLighting->sliSpecularPower, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->sliValue, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->cbRepresentation, SIGNAL(currentTextChanged(const QString&)),
          this, SIGNAL(propertyChanged()));
  connect(m_ui->sliOpacity, SIGNAL(valueEdited(double)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->colorChooser, SIGNAL(chosenColorChanged(const QColor&)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->cbColorBy, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(propertyChanged()));
  connect(m_ui->cbSelectColor, SIGNAL(toggled(bool)), this,
          SIGNAL(useSolidColor(const bool)));
}

ModuleContourWidget::~ModuleContourWidget() = default;

void ModuleContourWidget::addPropertyLinks(pqPropertyLinks& links,
                                           vtkSMProxy* representation,
                                           vtkSMSourceProxy* contourFilter)
{
  links.addPropertyLink(m_ui->cbColorMapData, "checked", SIGNAL(toggled(bool)),
                        representation,
                        representation->GetProperty("MapScalars"), 0);
  links.addPropertyLink(m_ui->sliValue, "value", SIGNAL(valueEdited(double)),
                        contourFilter,
                        contourFilter->GetProperty("ContourValues"), 0);
  new pqWidgetRangeDomain(m_ui->sliValue, "minimum", "maximum",
                          contourFilter->GetProperty("ContourValues"), 0);

  pqSignalAdaptorComboBox* adaptor =
    new pqSignalAdaptorComboBox(m_ui->cbRepresentation);
  links.addPropertyLink(adaptor, "currentText",
                        SIGNAL(currentTextChanged(QString)), representation,
                        representation->GetProperty("Representation"));

  links.addPropertyLink(m_ui->sliOpacity, "value", SIGNAL(valueEdited(double)),
                        representation, representation->GetProperty("Opacity"),
                        0);

  links.addPropertyLink(m_uiLighting->sliAmbient, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("Ambient"), 0);

  links.addPropertyLink(m_uiLighting->sliDiffuse, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("Diffuse"), 0);

  links.addPropertyLink(m_uiLighting->sliSpecular, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("Specular"), 0);

  links.addPropertyLink(m_uiLighting->sliSpecularPower, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("SpecularPower"), 0);

  // Surface uses DiffuseColor and Wireframe uses AmbientColor so we have to set
  // both
  links.addPropertyLink(m_ui->colorChooser, "chosenColorRgbF",
                        SIGNAL(chosenColorChanged(const QColor&)),
                        representation,
                        representation->GetProperty("DiffuseColor"));

  links.addPropertyLink(m_ui->colorChooser, "chosenColorRgbF",
                        SIGNAL(chosenColorChanged(const QColor&)),
                        representation,
                        representation->GetProperty("AmbientColor"));
}

void ModuleContourWidget::addCategoricalPropertyLinks(
  pqPropertyLinks& links, vtkSMProxy* representation)
{
  pqSignalAdaptorComboBox* adaptor =
    new pqSignalAdaptorComboBox(m_ui->cbRepresentation);
  links.addPropertyLink(adaptor, "currentText",
                        SIGNAL(currentTextChanged(QString)), representation,
                        representation->GetProperty("Representation"));

  links.addPropertyLink(m_ui->sliOpacity, "value", SIGNAL(valueEdited(double)),
                        representation, representation->GetProperty("Opacity"),
                        0);

  links.addPropertyLink(m_uiLighting->sliAmbient, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("Ambient"), 0);

  links.addPropertyLink(m_uiLighting->sliDiffuse, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("Diffuse"), 0);

  links.addPropertyLink(m_uiLighting->sliSpecular, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("Specular"), 0);

  links.addPropertyLink(m_uiLighting->sliSpecularPower, "value",
                        SIGNAL(valueEdited(double)), representation,
                        representation->GetProperty("SpecularPower"), 0);

  // Surface uses DiffuseColor and Wireframe uses AmbientColor so we have to set
  // both
  links.addPropertyLink(m_ui->colorChooser, "chosenColorRgbF",
                        SIGNAL(chosenColorChanged(const QColor&)),
                        representation,
                        representation->GetProperty("DiffuseColor"));

  links.addPropertyLink(m_ui->colorChooser, "chosenColorRgbF",
                        SIGNAL(chosenColorChanged(const QColor&)),
                        representation,
                        representation->GetProperty("AmbientColor"));
}

void ModuleContourWidget::setUseSolidColor(const bool useSolid)
{
  m_ui->cbSelectColor->setChecked(useSolid);
}

QComboBox* ModuleContourWidget::getColorByComboBox()
{
  return m_ui->cbColorBy;
}

void ModuleContourWidget::setColorMapRangeDomain(const double range[2])
{
  bool minSignalBlocked = m_ui->sliColorArrayMin->signalsBlocked();
  m_ui->sliColorArrayMin->blockSignals(true);
  bool maxSignalBlocked = m_ui->sliColorArrayMax->signalsBlocked();
  m_ui->sliColorArrayMax->blockSignals(true);

  m_ui->sliColorArrayMin->setMinimum(range[0]);
  m_ui->sliColorArrayMin->setMaximum(range[1]);
  m_ui->sliColorArrayMax->setMinimum(range[0]);
  m_ui->sliColorArrayMax->setMaximum(range[1]);

  m_ui->sliColorArrayMin->blockSignals(minSignalBlocked);
  m_ui->sliColorArrayMax->blockSignals(maxSignalBlocked);
}

void ModuleContourWidget::setColorMapRange(const double range[2])
{
  bool minSignalBlocked = m_ui->sliColorArrayMin->signalsBlocked();
  m_ui->sliColorArrayMin->blockSignals(true);
  bool maxSignalBlocked = m_ui->sliColorArrayMax->signalsBlocked();
  m_ui->sliColorArrayMax->blockSignals(true);

  m_ui->sliColorArrayMin->setValue(range[0]);
  m_ui->sliColorArrayMax->setValue(range[1]);

  m_ui->sliColorArrayMin->blockSignals(minSignalBlocked);
  m_ui->sliColorArrayMax->blockSignals(maxSignalBlocked);
}

void ModuleContourWidget::getColorMapRange(double range[2]) const
{
  range[0] = m_ui->sliColorArrayMin->value();
  range[1] = m_ui->sliColorArrayMax->value();
}

}
