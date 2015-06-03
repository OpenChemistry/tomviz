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

#include "AlignWidget.h"

#include "DataSource.h"
#include "LoadDataReaction.h"

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
#include <vtkPointData.h>
#include <vtkDataArray.h>

#include <QTimer>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QKeyEvent>
#include <QButtonGroup>

namespace tomviz
{

AlignWidget::AlignWidget(DataSource* data, QWidget* p, Qt::WindowFlags f)
  : QWidget(p, f), timer(new QTimer(this)), frameRate(10), sliceIncrement(1),
    unalignedData(data), alignedData(NULL)
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
  mapper->Update();
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
  int gridrow = 0;
  v->addStretch(1);
  QLabel *keyGuide = new QLabel;
  keyGuide->setText("1. Pick an object, use the arrow\nkeys to minimize the wobble.\n"
                    "2. Use the S to move to the next\nslice, A to return to previous slice.\n"
                    "3. Repeat steps 1 and 2.\n\n"
                    "Note: The same object/point must\nbe used for all slices.");
  v->addWidget(keyGuide);
  v->addStretch(1);
  v->addLayout(grid);
  v->addStretch(1);
  QLabel *label = new QLabel("Current image:");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  currentSlice = new QSpinBox;
  currentSlice->setValue(1);
  currentSlice->setRange(mapper->GetSliceNumberMinValue(),
                         mapper->GetSliceNumberMaxValue());
  connect(currentSlice, SIGNAL(valueChanged(int)), SLOT(setSlice(int)));
  grid->addWidget(currentSlice, gridrow, 1, 1, 1, Qt::AlignLeft);

  gridrow++;
  label = new QLabel("Frame rate (fps):");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  QSpinBox *spin = new QSpinBox;
  spin->setRange(0, 50);
  spin->setValue(10);
  connect(spin, SIGNAL(valueChanged(int)), SLOT(setFrameRate(int)));
  grid->addWidget(spin, gridrow, 1, 1, 1, Qt::AlignLeft);

  gridrow++;
  label = new QLabel("Reference image:");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  QRadioButton* PrevButton = new QRadioButton("Prev");
  QRadioButton* NextButton = new QRadioButton("Next");
  PrevButton->setCheckable(true);
  NextButton->setCheckable(true);
  grid->addWidget(PrevButton,gridrow,1,1,1, Qt::AlignRight);
  grid->addWidget(NextButton,gridrow,2,1,1, Qt::AlignRight);
  referenceSliceMode = new QButtonGroup;
  referenceSliceMode->addButton(PrevButton);
  referenceSliceMode->addButton(NextButton);
  referenceSliceMode->setExclusive(true);
  PrevButton->setChecked(true);

  // Slice offsets
  gridrow++;
  label = new QLabel("Image shift:");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  currentSliceOffset = new QLabel("(0, 0)");
  grid->addWidget(currentSliceOffset, gridrow, 1, 1, 1, Qt::AlignLeft);

  // Add our buttons.
  gridrow++;
  QHBoxLayout *buttonLayout = new QHBoxLayout;
  QPushButton *button = new QPushButton("Start");
  connect(button, SIGNAL(clicked()), SLOT(startAlign()));
  buttonLayout->addWidget(button);
  button = new QPushButton("Stop");
  connect(button, SIGNAL(clicked()), SLOT(stopAlign()));
  buttonLayout->addWidget(button);
  grid->addLayout(buttonLayout, gridrow, 0, 1, 2, Qt::AlignCenter);

  gridrow++;
  button = new QPushButton("Create Aligned Data");
  connect(button, SIGNAL(clicked()), SLOT(doDataAlign()));
  grid->addWidget(button, gridrow, 0, 1, 2, Qt::AlignCenter);

  offsets.fill(vtkVector2i(0, 0), mapper->GetSliceNumberMaxValue() + 1);

  /* Some test offsets.
  offsets[1] = vtkVector2i(10, 0);
  offsets[3] = vtkVector2i(-10, 0);
  offsets[5] = vtkVector2i(0, 10);
  offsets[7] = vtkVector2i(0, -10);
  offsets[9] = vtkVector2i(10, 10);
  offsets[11] = vtkVector2i(-10, -10); */

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
  if (i > max)  //This makes stack circular
    {
    i = min;
    }
  else if (i < min)
    {
    i = max;
    }
  setSlice(i, false);
}

void AlignWidget::changeSlice(int delta)
{
  int min = mapper->GetSliceNumberMinValue();
  int max = mapper->GetSliceNumberMaxValue();
  int i = currentSlice->value() + delta;
  sliceIncrement = 1;
  if (i > max)  //This makes stack circular
    {
    i = min;
    }
  else if (i < min)
    {
    i = max;
    }
  currentSlice->setValue(i);
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
    case Qt::Key_K:
    case Qt::Key_S:
      changeSlice(1);
      return;
    case Qt::Key_J:
    case Qt::Key_A:
      changeSlice(-1);
      return;
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

namespace
{
vtkImageData* imageData(DataSource *source)
{
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  return vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
}

// We are assuming an image that begins at 0, 0, 0.
vtkIdType imageIndex(const vtkVector3i &incs, const vtkVector3i &pos)
{
  return pos[0] * incs[0] + pos[1] * incs[1] + pos[2] * incs[2];
}

template<typename T>
void applyImageOffsets(T* in, T* out, vtkImageData *image,
                       const QVector<vtkVector2i> &offsets)
{
  // We know that the input and output images are the same size, with the
  // supplied offsets applied to each slice. Copy the pixels, applying offsets.
  int *extents = image->GetExtent();
  vtkVector3i extent(extents[1] - extents[0] + 1,
                     extents[3] - extents[2] + 1,
                     extents[5] - extents[4] + 1);
  vtkVector3i incs(1,
                   1 * extent[0],
                   1 * extent[0] * extent[1]);

  // Zero out our output array, we should do this more intelligently in future.
  T *ptr = out;
  for (int i = 0; i < extent[0] * extent[1] * extent[2]; ++i)
    {
    *ptr++ = 0;
    }

  // We need to go slice by slice, applying the pixel offsets to the new image.
  for (int i = 0; i < extent[2]; ++i)
    {
    vtkVector2i offset = offsets[i];
    int idx = imageIndex(incs, vtkVector3i(0, 0, i));
    T *inPtr = in + idx;
    T* outPtr = out + idx;
    for (int y = 0; y < extent[1]; ++y)
      {
      if (y + offset[1] >= extent[1])
        {
        break;
        }
      else if (y + offset[1] < 0)
        {
        inPtr += incs[1];
        outPtr += incs[1];
        continue;
        }
      for (int x = 0; x < extent[0]; ++x)
        {
        if (x + offset[0] >= extent[0])
          {
          inPtr += offset[0];
          outPtr += offset[0];
          break;
          }
        else if (x + offset[0] < 0)
          {
          ++inPtr;
          ++outPtr;
          continue;
          }
        *(outPtr + offset[0] + incs[1] * offset[1]) = *inPtr;
        ++inPtr;
        ++outPtr;
        }
      }
    }
}
}

void AlignWidget::doDataAlign()
{
  bool firstAdded = false;
  if (!alignedData)
    {
    alignedData = unalignedData->clone(true);
    QString name = alignedData->producer()->GetAnnotation("TomViz.Label");
    name = "Aligned_" + name;
    alignedData->producer()->SetAnnotation("TomViz.Label", name.toAscii().data());
    firstAdded = true;
    }
  vtkImageData *in = imageData(unalignedData);
  vtkImageData *out = imageData(alignedData);

  switch (in->GetScalarType())
    {
    vtkTemplateMacro(
      applyImageOffsets(reinterpret_cast<VTK_TT*>(in->GetScalarPointer()),
                        reinterpret_cast<VTK_TT*>(out->GetScalarPointer()),
                        in, offsets));
    }
  alignedData->dataModified();

  if (firstAdded)
    {
    LoadDataReaction::dataSourceAdded(alignedData);
    }
}

}
