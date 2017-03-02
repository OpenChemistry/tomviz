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
#include "ui_ModuleContourWidget.h"
#include "ui_LightingParametersForm.h"

#include "vtkVolumeMapper.h"

namespace tomviz {

ModuleContourWidget::ModuleContourWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleContourWidget),
  m_uiLighting(new Ui::LightingParametersForm)
{
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  QWidget::layout()->addWidget(lightingWidget);

  QStringList labelsRepre;
  labelsRepre << tr("Surface") << tr("Wireframe") << tr("Points");
  m_ui->cbRepresentation->addItems(labelsRepre);

  connect(m_uiLighting->gbLighting, SIGNAL(toggled(bool)), this,
          SIGNAL(lightingToggled(const bool)));
  connect(m_uiLighting->sliAmbient, SIGNAL(sliderChanged(int)), this,
          SLOT(onAmbientChanged(const int)));
  connect(m_uiLighting->sliDiffuse, SIGNAL(sliderChanged(int)), this,
          SLOT(onDiffuseChanged(const int)));
  connect(m_uiLighting->sliSpecular, SIGNAL(sliderChanged(int)), this,
          SLOT(onSpecularChanged(const int)));
  connect(m_uiLighting->sliSpecularPower, SIGNAL(sliderChanged(int)), this,
          SLOT(onSpecularPowerChanged(const int)));
}

void ModuleContourWidget::setLighting(const bool enable)
{
  m_uiLighting->gbLighting->setChecked(enable);
}

void ModuleContourWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(static_cast<int>(value * 100));
}

void ModuleContourWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(static_cast<int>(value * 100));
}

void ModuleContourWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(static_cast<int>(value * 100));
}

void ModuleContourWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(static_cast<int>(value * 100));
}

void ModuleContourWidget::onAmbientChanged(const int value)
{
  emit ambientChanged(static_cast<double>(value) / 100.0);
}

void ModuleContourWidget::onDiffuseChanged(const int value)
{
  emit diffuseChanged(static_cast<double>(value) / 100.0);
}

void ModuleContourWidget::onSpecularChanged(const int value)
{
  emit specularChanged(static_cast<double>(value) / 100.0);
}

void ModuleContourWidget::onSpecularPowerChanged(const int value)
{
  emit specularPowerChanged(static_cast<double>(value) / 2.0);
}
}
