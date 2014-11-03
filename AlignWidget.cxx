/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "AlignWidget.h"

#include "DataSource.h"

#include <QVTKWidget.h>
#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkTrivialProducer.h>
#include <vtkScalarsToColors.h>
#include <vtkSMSourceProxy.h>
#include <vtkNew.h>
#include <vtkVector.h>

#include <QTimer>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QKeyEvent>

namespace TEM
{

AlignWidget::AlignWidget(DataSource* data, QWidget* p, Qt::WindowFlags f)
  : QWidget(p, f), timer(new QTimer(this)), frameRate(10), sliceIncrement(1)
{
  widget = new QVTKWidget(this);
  widget->installEventFilter(this);
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->addWidget(widget);
  QVBoxLayout *v = new QVBoxLayout;
  layout->addLayout(v);
  setLayout(layout);
  setMinimumWidth(400);
  setMinimumHeight(300);
  setGeometry(-1, -1, 800, 600);
  setWindowTitle("Align data");

  // Grab the image data from the data source...
  vtkTrivialProducer *t =
      vtkTrivialProducer::SafeDownCast(data->producer()->GetClientSideObject());
  vtkImageData *imageData(NULL);
  if (t)
    {
    imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    }

  // Set up the rendering pipeline
  vtkNew<vtkRenderer> renderer;
  if (t)
    {
    mapper->SetInputConnection(t->GetOutputPort());
    }
  imageSlice->SetMapper(mapper.Get());
  renderer->AddViewProp(imageSlice.Get());
  widget->GetRenderWindow()->AddRenderer(renderer.Get());

  renderer->SetBackground(1.0, 1.0, 1.0);
  vtkCamera *camera = renderer->GetActiveCamera();
  renderer->SetViewport(0.0, 0.0,
                        1.0, 1.0);

  double *bounds = mapper->GetBounds();
  vtkVector3d point;
  point[0] = 0.5 * (bounds[0] + bounds[1]);
  point[1] = 0.5 * (bounds[2] + bounds[3]);
  point[2] = 0.5 * (bounds[4] + bounds[5]);
  camera->SetFocalPoint(point.GetData());
  point[2] += 500;
  camera->SetPosition(point.GetData());
  camera->SetViewUp(0.0, 1.0, 0.0);
  camera->ParallelProjectionOn();
  camera->SetParallelScale(128);

  vtkScalarsToColors *lut =
      vtkScalarsToColors::SafeDownCast(data->colorMap()->GetClientSideObject());
  if (lut)
    {
    imageSlice->GetProperty()->SetLookupTable(lut);
    }

  // Now to add the controls to the widget.
  QGridLayout *grid = new QGridLayout;
  v->addStretch(1);
  v->addLayout(grid);
  v->addStretch(1);
  QLabel *label = new QLabel("Current slice:");
  grid->addWidget(label, 0, 0, 1, 1, Qt::AlignRight);
  currentSlice = new QSpinBox;
  currentSlice->setValue(0);
  currentSlice->setRange(mapper->GetSliceNumberMinValue(),
                         mapper->GetSliceNumberMaxValue());
  connect(currentSlice, SIGNAL(valueChanged(int)), SLOT(setSlice(int)));
  grid->addWidget(currentSlice, 0, 1, 1, 1, Qt::AlignLeft);

  label = new QLabel("Frame rate (fps):");
  grid->addWidget(label, 1, 0, 1, 1, Qt::AlignRight);
  QSpinBox *spin = new QSpinBox;
  spin->setRange(0, 50);
  spin->setValue(10);
  connect(spin, SIGNAL(valueChanged(int)), SLOT(setFrameRate(int)));
  grid->addWidget(spin, 1, 1, 1, 1, Qt::AlignLeft);

  // Slice offsets
  label = new QLabel("Slice offset:");
  grid->addWidget(label, 2, 0, 1, 1, Qt::AlignRight);
  currentSliceOffset = new QLabel("(0, 0)");
  grid->addWidget(currentSliceOffset, 2, 1, 1, 1, Qt::AlignLeft);

  // Add our buttons.
  QHBoxLayout *buttonLayout = new QHBoxLayout;
  QPushButton *button = new QPushButton("Start");
  connect(button, SIGNAL(clicked()), SLOT(startAlign()));
  buttonLayout->addWidget(button);
  button = new QPushButton("Stop");
  connect(button, SIGNAL(clicked()), SLOT(stopAlign()));
  buttonLayout->addWidget(button);
  grid->addLayout(buttonLayout, 3, 0, 1, 2, Qt::AlignCenter);

  offsets.fill(vtkVector2i(0, 0), mapper->GetSliceNumberMaxValue() + 1);

  connect(timer, SIGNAL(timeout()), SLOT(changeSlice()));
  timer->start(100);
}

AlignWidget::~AlignWidget()
{
}

bool AlignWidget::eventFilter(QObject *object, QEvent *event)
{
  if (object == widget)
    {
    switch (event->type())
      {
      case QEvent::KeyPress:
        widgetKeyPress(static_cast<QKeyEvent *>(event));
        return true;
      case QEvent::MouseMove:
      case QEvent::MouseButtonRelease:
      case QEvent::MouseButtonPress:
        return true;
      default:
        return false;
      }
    }
  else
    {
    return false;
    }
}

void AlignWidget::setDataSource(DataSource *source)
{
}

void AlignWidget::changeSlice()
{
  int min = mapper->GetSliceNumberMinValue();
  int max = mapper->GetSliceNumberMaxValue();
  int i = mapper->GetSliceNumber() + sliceIncrement;
  sliceIncrement *= -1;
  if (i > max)
    {
    i = min;
    }
  else if (i < min)
    {
    i = max;
    }
  setSlice(i, false);
}

void AlignWidget::setSlice(int slice, bool resetInc)
{
  if (resetInc)
    {
    sliceIncrement = 1;
    currentSliceOffset->setText(QString("(%1, %2)").arg(offsets[slice][0])
        .arg(offsets[slice][1]));
    }
  mapper->SetSliceNumber(slice);
  applySliceOffset(slice);
}

void AlignWidget::setFrameRate(int rate)
{
  if (rate < 0)
    {
    frameRate = 0;
    }
  frameRate = rate;
  if (frameRate > 0)
    {
    timer->setInterval(1000.0 / frameRate);
    if (!timer->isActive())
      {
      timer->start();
      }
    }
  else
    {
    timer->stop();
    }
}

void AlignWidget::widgetKeyPress(QKeyEvent *key)
{
  vtkVector2i &offset = offsets[currentSlice->value()];
  switch (key->key())
    {
    case Qt::Key_Left:
      offset[0] -= 1;
      break;
    case Qt::Key_Right:
      offset[0] += 1;
      break;
    case Qt::Key_Up:
      offset[1] += 1;
      break;
    case Qt::Key_Down:
      offset[1] -= 1;
      break;
    default:
      // Nothing
      break;
    }
  applySliceOffset();
}

void AlignWidget::applySliceOffset(int sliceNumber)
{
  vtkVector2i offset(0, 0);
  if (sliceNumber == -1)
    {
    offset = offsets[currentSlice->value()];
    currentSliceOffset->setText(QString("(%1, %2)").arg(offset[0])
        .arg(offset[1]));
    }
  else
    {
    offset = offsets[sliceNumber];
    }
  imageSlice->SetPosition(offset[0], offset[1], 0);
  widget->update();
}

void AlignWidget::startAlign()
{
  if (!timer->isActive())
    {
    timer->start(1000.0 / frameRate);
    }
}

void AlignWidget::stopAlign()
{
  timer->stop();
  sliceIncrement = 1;
  setSlice(currentSlice->value());
}

}
