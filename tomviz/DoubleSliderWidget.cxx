/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DoubleSliderWidget.h"

#include "pqLineEdit.h"

// Qt includes
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSlider>

namespace tomviz {

DoubleSliderWidget::DoubleSliderWidget(bool showLineEdit, QWidget* p)
  : QWidget(p)
{
  this->BlockUpdate = false;
  this->Value = 0;
  this->Minimum = 0;
  this->Maximum = 1;
  this->Resolution = 100;
  this->StrictRange = false;
  this->SliderTracking = true;

  QHBoxLayout* l = new QHBoxLayout(this);
  l->setMargin(0);
  this->Slider = new QSlider(Qt::Horizontal, this);
  this->Slider->setRange(0, this->Resolution);
  l->addWidget(this->Slider, 4);
  this->Slider->setObjectName("Slider");
  if (showLineEdit) {
    this->LineEdit = new pqLineEdit(this);
    l->addWidget(this->LineEdit);
    this->LineEdit->setObjectName("LineEdit");
    this->LineEdit->setValidator(new QDoubleValidator(this->LineEdit));
    this->LineEdit->setTextAndResetCursor(QString().setNum(this->Value));
  } else {
    this->LineEdit = nullptr;
  }

  QObject::connect(this->Slider, SIGNAL(valueChanged(int)), this,
                   SLOT(sliderChanged(int)));
  QObject::connect(this->Slider, SIGNAL(sliderReleased()), this,
                   SLOT(onSliderReleased()));
  if (showLineEdit) {
    QObject::connect(this->LineEdit, SIGNAL(textChanged(const QString&)), this,
                     SLOT(textChanged(const QString&)));
    QObject::connect(this->LineEdit, SIGNAL(textChangedAndEditingFinished()),
                     this, SLOT(editingFinished()));
  }
}

DoubleSliderWidget::~DoubleSliderWidget() {}

void DoubleSliderWidget::setLineEditWidth(int width)
{
  if (this->LineEdit) {
    QSize s = this->LineEdit->sizeHint();
    QSize newSize(width, s.height());
    this->LineEdit->setFixedSize(newSize);
  }
}

int DoubleSliderWidget::resolution() const
{
  return this->Resolution;
}

void DoubleSliderWidget::setResolution(int val)
{
  this->Resolution = val;
  this->Slider->setRange(0, this->Resolution);
  this->updateSlider();
}

double DoubleSliderWidget::value() const
{
  return this->Value;
}

void DoubleSliderWidget::setValue(double val)
{
  if (this->Value == val) {
    return;
  }

  this->Value = val;

  if (!this->BlockUpdate) {
    // set the slider
    this->updateSlider();

    // set the text
    if (this->LineEdit) {
      this->BlockUpdate = true;
      this->LineEdit->setTextAndResetCursor(
        QString().setNum(val, 'g', 8));
      this->BlockUpdate = false;
    }
  }

  emit this->valueChanged(this->Value);
}

bool DoubleSliderWidget::sliderTracking() const
{
  return this->SliderTracking;
}

void DoubleSliderWidget::setSliderTracking(bool b)
{
  this->SliderTracking = b;
}

double DoubleSliderWidget::maximum() const
{
  return this->Maximum;
}

void DoubleSliderWidget::setMaximum(double val)
{
  this->Maximum = val;
  this->updateValidator();
  this->updateSlider();
}

double DoubleSliderWidget::minimum() const
{
  return this->Minimum;
}

void DoubleSliderWidget::setMinimum(double val)
{
  this->Minimum = val;
  this->updateValidator();
  this->updateSlider();
}

void DoubleSliderWidget::updateValidator()
{
  if (!this->LineEdit) {
    return;
  }
  if (this->StrictRange) {
    this->LineEdit->setValidator(new QDoubleValidator(
      this->minimum(), this->maximum(), 100, this->LineEdit));
  } else {
    this->LineEdit->setValidator(new QDoubleValidator(this->LineEdit));
  }
}

bool DoubleSliderWidget::strictRange() const
{
  if (!this->LineEdit) {
    return true;
  }
  const QDoubleValidator* dv =
    qobject_cast<const QDoubleValidator*>(this->LineEdit->validator());
  return dv->bottom() == this->minimum() && dv->top() == this->maximum();
}

void DoubleSliderWidget::setStrictRange(bool s)
{
  this->StrictRange = s;
  this->updateValidator();
}

void DoubleSliderWidget::sliderChanged(int val)
{
  if (this->SliderTracking) {
    // If slider tracking is turned on, update for every slider value
    setValueFromSlider(val);
  } else {
    // If slider tracking is off, wait until the mouse is released to
    // update, but still display the new value in the text.
    if (this->LineEdit) {
      double fraction = val / static_cast<double>(this->Resolution);
      double range = this->Maximum - this->Minimum;
      double v = (fraction * range) + this->Minimum;

      QSignalBlocker blocked(this->LineEdit);
      this->LineEdit->setTextAndResetCursor(QString().setNum(v));
    }
  }
}

void DoubleSliderWidget::onSliderReleased()
{
  if (!this->SliderTracking) {
    // We are supposed to update the slider when it is released.
    // Do so now.
    setValueFromSlider(this->Slider->value());
  }
}

void DoubleSliderWidget::setValueFromSlider(int val)
{
  if (!this->BlockUpdate) {
    double fraction = val / static_cast<double>(this->Resolution);
    double range = this->Maximum - this->Minimum;
    double v = (fraction * range) + this->Minimum;
    this->BlockUpdate = true;
    if (this->LineEdit) {
      this->LineEdit->setTextAndResetCursor(QString().setNum(v));
    }
    this->setValue(v);
    emit this->valueEdited(v);
    this->BlockUpdate = false;
  }
}

void DoubleSliderWidget::textChanged(const QString& text)
{
  if (!this->BlockUpdate) {
    double val = text.toDouble();
    this->BlockUpdate = true;
    double range = this->Maximum - this->Minimum;
    double fraction = (val - this->Minimum) / range;
    int sliderVal = qRound(fraction * static_cast<double>(this->Resolution));
    this->Slider->setValue(sliderVal);
    this->setValue(val);
    this->BlockUpdate = false;
  }
}

void DoubleSliderWidget::editingFinished()
{
  emit this->valueEdited(this->Value);
}

void DoubleSliderWidget::updateSlider()
{
  this->Slider->blockSignals(true);
  double range = this->Maximum - this->Minimum;
  double fraction = (this->Value - this->Minimum) / range;
  int v = qRound(fraction * static_cast<double>(this->Resolution));
  this->Slider->setValue(v);
  this->Slider->blockSignals(false);
}
} // namespace tomviz
