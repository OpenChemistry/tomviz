/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleScaleCubeWidget.h"
#include "ui_ModuleScaleCubeWidget.h"

#include <QDoubleValidator>
#include <QTextStream>

namespace tomviz {

ModuleScaleCubeWidget::ModuleScaleCubeWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleScaleCubeWidget)
{
  m_ui->setupUi(this);
  m_ui->leSideLength->setValidator(new QDoubleValidator(this));

  connect(m_ui->chbAdaptiveScaling, SIGNAL(toggled(bool)), this,
          SIGNAL(adaptiveScalingToggled(const bool)));
  connect(m_ui->leSideLength, &QLineEdit::editingFinished, this,
          [&] { sideLengthChanged(m_ui->leSideLength->text().toDouble()); });
  connect(m_ui->chbAnnotation, SIGNAL(toggled(bool)), this,
          SIGNAL(annotationToggled(const bool)));
  connect(m_ui->colorChooserButton, &pqColorChooserButton::chosenColorChanged,
          this, &ModuleScaleCubeWidget::boxColorChanged);
  connect(m_ui->textColorChooserButton,
          &pqColorChooserButton::chosenColorChanged, this,
          &ModuleScaleCubeWidget::textColorChanged);
}

ModuleScaleCubeWidget::~ModuleScaleCubeWidget() = default;

void ModuleScaleCubeWidget::setAdaptiveScaling(const bool choice)
{
  m_ui->chbAdaptiveScaling->setChecked(choice);
}

void ModuleScaleCubeWidget::setSideLength(const double length)
{
  m_ui->leSideLength->setText(QString::number(length));
}

void ModuleScaleCubeWidget::setAnnotation(const bool choice)
{
  m_ui->chbAnnotation->setChecked(choice);
}

void ModuleScaleCubeWidget::setLengthUnit(const QString unit)
{
  m_ui->tlLengthUnit->setText(unit);
}

void ModuleScaleCubeWidget::setPosition(const double x, const double y,
                                        const double z)
{
  QString s;
  QTextStream(&s) << "(" << QString::number(x, 'f', 4) << ", "
                  << QString::number(y, 'f', 4) << ", "
                  << QString::number(z, 'f', 4) << ")";
  m_ui->tlPosition->setText(s);
}

void ModuleScaleCubeWidget::setPositionUnit(const QString unit)
{
  m_ui->tlPositionUnit->setText(unit);
}

void ModuleScaleCubeWidget::setBoxColor(const QColor& color)
{
  m_ui->colorChooserButton->setChosenColor(color);
}

void ModuleScaleCubeWidget::setTextColor(const QColor& color)
{
  m_ui->textColorChooserButton->setChosenColor(color);
}

void ModuleScaleCubeWidget::onAdaptiveScalingChanged(const bool state)
{
  emit adaptiveScalingToggled(state);
}

void ModuleScaleCubeWidget::onSideLengthChanged(const double length)
{
  emit sideLengthChanged(length);
}

void ModuleScaleCubeWidget::onAnnotationChanged(const bool state)
{
  emit annotationToggled(state);
}
} // namespace tomviz
