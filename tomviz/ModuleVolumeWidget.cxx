#include "ModuleVolumeWidget.h"
#include "ui_LightingParametersForm.h"
#include "ui_ModuleVolumeWidget.h"

#include "vtkVolumeMapper.h"

namespace tomviz {

ModuleVolumeWidget::ModuleVolumeWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleVolumeWidget),
    m_uiLighting(new Ui::LightingParametersForm)
{
  std::cout << "->> construct ModVolWidget ...\n";
  m_ui->setupUi(this);

  QWidget* lightingWidget = new QWidget;
  m_uiLighting->setupUi(lightingWidget);
  QWidget::layout()->addWidget(lightingWidget);
  qobject_cast<QBoxLayout*>(QWidget::layout())->addStretch();

  const int leWidth = 50;
  m_uiLighting->sliAmbient->setLineEditWidth(leWidth);
  m_uiLighting->sliDiffuse->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecular->setLineEditWidth(leWidth);
  m_uiLighting->sliSpecularPower->setLineEditWidth(leWidth);

  m_uiLighting->sliSpecularPower->setMaximum(150);
  m_uiLighting->sliSpecularPower->setMinimum(1);
  m_uiLighting->sliSpecularPower->setResolution(200);

  QStringList labelsBlending;
  labelsBlending << tr("Composite") << tr("Max") << tr("Min") << tr("Average")
                 << tr("Additive");
  m_ui->cbBlending->addItems(labelsBlending);

  QStringList labelsInterp;
  labelsInterp << tr("Nearest Neighbor") << tr("Linear");
  m_ui->cbInterpolation->addItems(labelsInterp);

  connect(m_ui->cbJittering, SIGNAL(toggled(bool)), this,
          SIGNAL(jitteringToggled(const bool)));
  connect(m_ui->cbBlending, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onBlendingChanged(const int)));
  connect(m_ui->cbInterpolation, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(interpolationChanged(const int)));
  connect(m_ui->cbGradientOpac, SIGNAL(toggled(bool)), this,
          SIGNAL(gradientOpacityChanged(const bool)));

  connect(m_uiLighting->gbLighting, SIGNAL(toggled(bool)), this,
          SIGNAL(lightingToggled(const bool)));
  connect(m_uiLighting->sliAmbient, SIGNAL(valueEdited(double)), this,
          SIGNAL(ambientChanged(const double)));
  connect(m_uiLighting->sliDiffuse, SIGNAL(valueEdited(double)), this,
          SIGNAL(diffuseChanged(const double)));
  connect(m_uiLighting->sliSpecular, SIGNAL(valueEdited(double)), this,
          SIGNAL(specularChanged(const double)));
  connect(m_uiLighting->sliSpecularPower, SIGNAL(valueEdited(double)), this,
          SIGNAL(specularPowerChanged(const double)));
}

ModuleVolumeWidget::~ModuleVolumeWidget()
{
  std::cout << "->> Destruct ModVolWidget !\n";
}

void ModuleVolumeWidget::setJittering(const bool enable)
{
  m_ui->cbJittering->setChecked(enable);
}

void ModuleVolumeWidget::setBlendingMode(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  m_ui->cbBlending->setCurrentIndex(static_cast<int>(mode));
}

void ModuleVolumeWidget::setInterpolationType(const int type)
{
  m_ui->cbInterpolation->setCurrentIndex(type);
}

void ModuleVolumeWidget::setLighting(const bool enable)
{
  m_uiLighting->gbLighting->setChecked(enable);
}

void ModuleVolumeWidget::setAmbient(const double value)
{
  m_uiLighting->sliAmbient->setValue(value);
}

void ModuleVolumeWidget::setDiffuse(const double value)
{
  m_uiLighting->sliDiffuse->setValue(value);
}

void ModuleVolumeWidget::setSpecular(const double value)
{
  m_uiLighting->sliSpecular->setValue(value);
}

void ModuleVolumeWidget::setSpecularPower(const double value)
{
  m_uiLighting->sliSpecularPower->setValue(value);
}

void ModuleVolumeWidget::setGradientOpacityEnabled(const bool enabled)
{
  m_ui->cbGradientOpac->setChecked(enabled);
}

void ModuleVolumeWidget::onBlendingChanged(const int mode)
{
  m_uiLighting->gbLighting->setEnabled(usesLighting(mode));
  emit blendingChanged(mode);
}

bool ModuleVolumeWidget::usesLighting(const int mode) const
{
  if (mode == vtkVolumeMapper::COMPOSITE_BLEND) {
    return true;
  }

  return false;
}
}
