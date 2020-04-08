/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleContourWidget.h"
#include "ui_LightingParametersForm.h"
#include "ui_ModuleContourWidget.h"

#include <QDebug>

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

  for (const auto& label : labelsRepre)
    m_ui->cbRepresentation->addItem(label, label);

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
  connect(m_ui->cbRepresentation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ModuleContourWidget::onRepresentationIndexChanged);
  connect(m_ui->sliOpacity, &DoubleSliderWidget::valueEdited, this,
          &ModuleContourWidget::opacityChanged);
  connect(m_ui->colorChooser, &pqColorChooserButton::chosenColorChanged, this,
          &ModuleContourWidget::colorChanged);
  connect(m_ui->cbSelectColor, &QCheckBox::toggled, this,
          &ModuleContourWidget::useSolidColorToggled);
  connect(m_ui->cbColorByArray, &QCheckBox::toggled, this,
          &ModuleContourWidget::colorByArrayToggled);
  connect(m_ui->comboColorByArray,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ModuleContourWidget::onColorByArrayIndexChanged);
  connect(m_ui->comboContourByArray,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ModuleContourWidget::onContourByArrayIndexChanged);
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

  // Save and use the text in the item data
  for (const auto& opt : options)
    m_ui->comboColorByArray->addItem(opt, opt);
}

void ModuleContourWidget::setContourByArrayOptions(DataSource* ds,
                                                   Module* module)
{
  m_ui->comboContourByArray->setOptions(ds, module);
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
  for (int i = 0; i < m_ui->cbRepresentation->count(); ++i) {
    if (m_ui->cbRepresentation->itemData(i).toString() == representation) {
      m_ui->cbRepresentation->setCurrentIndex(i);
      return;
    }
  }

  qCritical() << "Could not find" << representation
              << "in representation options";
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
  for (int i = 0; i < m_ui->comboColorByArray->count(); ++i) {
    if (m_ui->comboColorByArray->itemData(i).toString() == name) {
      m_ui->comboColorByArray->setCurrentIndex(i);
      return;
    }
  }

  qCritical() << "Could not find" << name << "in ColorByArray options";
}

void ModuleContourWidget::setContourByArrayValue(int val)
{
  for (int i = 0; i < m_ui->comboContourByArray->count(); ++i) {
    if (m_ui->comboContourByArray->itemData(i).toInt() == val) {
      m_ui->comboContourByArray->setCurrentIndex(i);
      return;
    }
  }

  qCritical() << "Could not find" << val << "in ContourByArray options";
}

void ModuleContourWidget::onContourByArrayIndexChanged(int i)
{
  emit contourByArrayValueChanged(
    m_ui->comboContourByArray->itemData(i).toInt());
}

void ModuleContourWidget::onColorByArrayIndexChanged(int i)
{
  emit colorByArrayNameChanged(m_ui->comboColorByArray->itemData(i).toString());
}

void ModuleContourWidget::onRepresentationIndexChanged(int i)
{
  emit representationChanged(m_ui->cbRepresentation->itemData(i).toString());
}

} // namespace tomviz
