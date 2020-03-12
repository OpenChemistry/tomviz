/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleContourWidget.h"
#include "ui_LightingParametersForm.h"
#include "ui_ModuleContourWidget.h"

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

  connect(m_ui->cbColorMapData, &QCheckBox::toggled, this,
          &ModuleContourWidget::colorMapDataToggled);
  connect(m_uiLighting->sliAmbient, &DoubleSliderWidget::valueEdited, this,
          &ModuleContourWidget::ambientChanged);
  connect(m_uiLighting->sliDiffuse, &DoubleSliderWidget::valueEdited, this,
          &ModuleContourWidget::diffuseChanged);
  connect(m_uiLighting->sliSpecular, &DoubleSliderWidget::valueEdited, this,
          &ModuleContourWidget::specularChanged);
  connect(m_uiLighting->sliSpecularPower, &DoubleSliderWidget::valueEdited,
          this, &ModuleContourWidget::specularPowerChanged);
  connect(m_ui->sliValue, &DoubleSliderWidget::valueEdited, this,
          &ModuleContourWidget::isoChanged);
  connect(m_ui->cbRepresentation, &QComboBox::currentTextChanged, this,
          &ModuleContourWidget::representationChanged);
  connect(m_ui->sliOpacity, &DoubleSliderWidget::valueEdited, this,
          &ModuleContourWidget::opacityChanged);
  connect(m_ui->colorChooser, &pqColorChooserButton::chosenColorChanged, this,
          &ModuleContourWidget::colorChanged);
  connect(m_ui->cbSelectColor, &QCheckBox::toggled, this,
          &ModuleContourWidget::useSolidColorToggled);
  connect(m_ui->cbColorByArray, &QCheckBox::toggled, this,
          &ModuleContourWidget::colorByArrayToggled);
  connect(m_ui->comboColorByArray, &QComboBox::currentTextChanged, this,
          &ModuleContourWidget::colorByArrayNameChanged);
}

ModuleContourWidget::~ModuleContourWidget() = default;

void ModuleContourWidget::setIsoRange(double range[2])
{
  m_ui->sliValue->setMinimum(range[0]);
  m_ui->sliValue->setMaximum(range[1]);
}

void ModuleContourWidget::setColorByArrayOptions(const QStringList& options)
{
  m_ui->comboColorByArray->clear();
  m_ui->comboColorByArray->addItems(options);
}

void ModuleContourWidget::setColorMapData(const bool state)
{
  m_ui->cbColorMapData->setChecked(state);
}

void ModuleContourWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void ModuleContourWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void ModuleContourWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void ModuleContourWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void ModuleContourWidget::setIso(const double value)
{
  m_ui->sliValue->setValue(value);
}

void ModuleContourWidget::setRepresentation(const QString& representation)
{
  m_ui->cbRepresentation->setCurrentText(representation);
}

void ModuleContourWidget::setOpacity(const double value)
{
  m_ui->sliOpacity->setValue(value);
}

void ModuleContourWidget::setColor(const QColor& color)
{
  m_ui->colorChooser->setChosenColor(color);
}

void ModuleContourWidget::setUseSolidColor(const bool state)
{
  m_ui->cbSelectColor->setChecked(state);
}

void ModuleContourWidget::setColorByArray(const bool state)
{
  m_ui->cbColorByArray->setChecked(state);
}

void ModuleContourWidget::setColorByArrayName(const QString& name)
{
  m_ui->comboColorByArray->setCurrentText(name);
}

} // namespace tomviz
