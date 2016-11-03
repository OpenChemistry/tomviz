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
#include "IntSliderWidget.h"

#include "pqLineEdit.h"
#include "vtkPVConfig.h"

// Qt includes
#include <QHBoxLayout>
#include <QIntValidator>
#include <QSlider>

namespace tomviz {

IntSliderWidget::IntSliderWidget(bool showLineEdit, QWidget* p) : QWidget(p)
{
  this->BlockUpdate = false;
  this->Value = 0;
  this->Minimum = 0;
  this->Maximum = 1;
  this->StrictRange = false;

  QHBoxLayout* l = new QHBoxLayout(this);
  l->setMargin(0);
  this->Slider = new QSlider(Qt::Horizontal, this);
  this->Slider->setRange(this->Minimum, this->Maximum);
  this->Slider->setFocusPolicy(Qt::StrongFocus);
  l->addWidget(this->Slider, 4);
  this->Slider->setObjectName("Slider");
  if (showLineEdit) {
    this->LineEdit = new pqLineEdit(this);
    l->addWidget(this->LineEdit);
    this->LineEdit->setObjectName("LineEdit");
    this->LineEdit->setValidator(new QIntValidator(this->LineEdit));
    this->LineEdit->setTextAndResetCursor(QString().setNum(this->Value));
  } else {
    this->LineEdit = nullptr;
  }

  QObject::connect(this->Slider, SIGNAL(valueChanged(int)), this,
                   SLOT(sliderChanged(int)));
  if (showLineEdit) {
    QObject::connect(this->LineEdit, SIGNAL(textChanged(const QString&)), this,
                     SLOT(textChanged(const QString&)));
    QObject::connect(this->LineEdit, SIGNAL(textChangedAndEditingFinished()),
                     this, SLOT(editingFinished()));
  }
}

IntSliderWidget::~IntSliderWidget()
{
}

void IntSliderWidget::setLineEditWidth(int width)
{
  if (this->LineEdit) {
    QSize s = this->LineEdit->sizeHint();
    QSize newSize(width, s.height());
    this->LineEdit->setFixedSize(newSize);
  }
}

void IntSliderWidget::setPageStep(int step)
{
  this->Slider->setPageStep(step);
}

int IntSliderWidget::value() const
{
  return this->Value;
}

void IntSliderWidget::setValue(int val)
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
      this->LineEdit->setTextAndResetCursor(QString().setNum(val));
      this->BlockUpdate = false;
    }
  }

  emit this->valueChanged(this->Value);
}

int IntSliderWidget::maximum() const
{
  return this->Maximum;
}

void IntSliderWidget::setMaximum(int val)
{
  this->Maximum = val;
  this->updateValidator();
  this->updateSlider();
}

int IntSliderWidget::minimum() const
{
  return this->Minimum;
}

void IntSliderWidget::setMinimum(int val)
{
  this->Minimum = val;
  this->updateValidator();
  this->updateSlider();
}

void IntSliderWidget::updateValidator()
{
  if (!this->LineEdit) {
    return;
  }
  if (this->StrictRange) {
    this->LineEdit->setValidator(
      new QIntValidator(this->minimum(), this->maximum(), this->LineEdit));
  } else {
    this->LineEdit->setValidator(new QIntValidator(this->LineEdit));
  }
}

bool IntSliderWidget::strictRange() const
{
  if (!this->LineEdit) {
    return true;
  }
  const QIntValidator* dv =
    qobject_cast<const QIntValidator*>(this->LineEdit->validator());
  return dv->bottom() == this->minimum() && dv->top() == this->maximum();
}

void IntSliderWidget::setStrictRange(bool s)
{
  this->StrictRange = s;
  this->updateValidator();
}

void IntSliderWidget::sliderChanged(int val)
{
  if (!this->BlockUpdate) {
    this->BlockUpdate = true;
    if (this->LineEdit) {
      this->LineEdit->setTextAndResetCursor(QString().setNum(val));
    }
    this->setValue(val);
    emit this->valueEdited(val);
    this->BlockUpdate = false;
  }
}

void IntSliderWidget::textChanged(const QString& text)
{
  if (!this->BlockUpdate) {
    int val = text.toInt();
    this->BlockUpdate = true;
    int sliderVal = val - this->Minimum;
    this->Slider->setValue(sliderVal);
    this->setValue(val);
    this->BlockUpdate = false;
  }
}

void IntSliderWidget::editingFinished()
{
  emit this->valueEdited(this->Value);
}

void IntSliderWidget::updateSlider()
{
  this->Slider->blockSignals(true);
  int v = this->Value - this->Minimum;
  this->Slider->setRange(this->Minimum, this->Maximum);
  this->Slider->setValue(v);
  this->Slider->blockSignals(false);
}
}
