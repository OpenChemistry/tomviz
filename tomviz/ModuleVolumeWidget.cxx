#include "ModuleVolumeWidget.h"
#include "ui_ModuleVolumeWidget.h"

#include "vtkVolumeMapper.h"

namespace tomviz {

ModuleVolumeWidget::ModuleVolumeWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleVolumeWidget)
{
  m_ui->setupUi(this);

  QStringList labelsBlending;
  labelsBlending << tr("Composite") << tr("Max") << tr("Min") << tr("Average")
                 << tr("Additive");
  m_ui->cbBlending->addItems(labelsBlending);

  QStringList labelsInterp;
  labelsInterp << tr("Nearest Neighbor") << tr("Linear");
  m_ui->cbInterpolation->addItems(labelsInterp);

  connect(m_ui->cbJittering, SIGNAL(toggled(bool)), this,
          SIGNAL(jitteringToggled(const bool)));
  connect(m_ui->gbLighting, SIGNAL(toggled(bool)), this,
          SIGNAL(lightingToggled(const bool)));
  connect(m_ui->cbBlending, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onBlendingChanged(const int)));
  connect(m_ui->cbInterpolation, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(interpolationChanged(const int)));
  connect(m_ui->sliAmbient, SIGNAL(valueChanged(int)), this,
          SLOT(onAmbientChanged(const int)));
  connect(m_ui->sliDiffuse, SIGNAL(valueChanged(int)), this,
          SLOT(onDiffuseChanged(const int)));
  connect(m_ui->sliSpecular, SIGNAL(valueChanged(int)), this,
          SLOT(onSpecularChanged(const int)));
  connect(m_ui->sliSpecularPower, SIGNAL(valueChanged(int)), this,
          SLOT(onSpecularPowerChanged(const int)));
  connect(m_ui->cbGradientOpac, SIGNAL(toggled(bool)), this,
          SIGNAL(gradientOpacityChanged(const bool)));
}

void ModuleVolumeWidget::setJittering(const bool enable)
{
  m_ui->cbJittering->setChecked(enable);
}

void ModuleVolumeWidget::setBlendingMode(const int mode)
{
  m_ui->gbLighting->setEnabled(this->usesLighting(mode));
  m_ui->cbBlending->setCurrentIndex(static_cast<int>(mode));
}

void ModuleVolumeWidget::setInterpolationType(const int type)
{
  m_ui->cbInterpolation->setCurrentIndex(type);
}

void ModuleVolumeWidget::setLighting(const bool enable)
{
  m_ui->gbLighting->setChecked(enable);
}

void ModuleVolumeWidget::setAmbient(const double value)
{
  m_ui->sliAmbient->setValue(static_cast<int>(value * 100));
}

void ModuleVolumeWidget::setDiffuse(const double value)
{
  m_ui->sliDiffuse->setValue(static_cast<int>(value * 100));
}

void ModuleVolumeWidget::setSpecular(const double value)
{
  m_ui->sliSpecular->setValue(static_cast<int>(value * 100));
}

void ModuleVolumeWidget::setSpecularPower(const double value)
{
  m_ui->sliSpecularPower->setValue(static_cast<int>(value * 100));
}

void ModuleVolumeWidget::setGradientOpacityEnabled(const bool enabled)
{
  m_ui->cbGradientOpac->setChecked(enabled);
}

void ModuleVolumeWidget::onBlendingChanged(const int mode)
{
  m_ui->gbLighting->setEnabled(this->usesLighting(mode));
  emit blendingChanged(mode);
}

bool ModuleVolumeWidget::usesLighting(const int mode) const
{
  if (mode == vtkVolumeMapper::COMPOSITE_BLEND) {
    return true;
  }

  return false;
}

void ModuleVolumeWidget::onAmbientChanged(const int value)
{
  emit ambientChanged(static_cast<double>(value) / 100.0);
}

void ModuleVolumeWidget::onDiffuseChanged(const int value)
{
  emit diffuseChanged(static_cast<double>(value) / 100.0);
}

void ModuleVolumeWidget::onSpecularChanged(const int value)
{
  emit specularChanged(static_cast<double>(value) / 100.0);
}

void ModuleVolumeWidget::onSpecularPowerChanged(const int value)
{
  emit specularPowerChanged(static_cast<double>(value) / 2.0);
}
}
