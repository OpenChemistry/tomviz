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
#include "TranslateAlignOperator.h"

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
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkInteractorStyleRubberBandZoom.h>

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
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QButtonGroup>

namespace tomviz
{

AlignWidget::AlignWidget(TranslateAlignOperator *op, QWidget* p)
  : EditOperatorWidget(p), timer(new QTimer(this)), frameRate(5),
    Op(op), unalignedData(op->getDataSource())
{
  widget = new QVTKWidget(this);
  widget->installEventFilter(this);
  QHBoxLayout *myLayout = new QHBoxLayout(this);
  myLayout->addWidget(widget);
  QVBoxLayout *v = new QVBoxLayout;
  myLayout->addLayout(v);
  setLayout(myLayout);
  setMinimumWidth(800);
  setMinimumHeight(600);
  setWindowTitle("Align data");

  // Grab the image data from the data source...
  vtkTrivialProducer *t =
      vtkTrivialProducer::SafeDownCast(this->unalignedData->producer()->GetClientSideObject());

  // Set up the rendering pipeline
  if (t)
  {
    mapper->SetInputConnection(t->GetOutputPort());
  }
  mapper->Update();
  imageSlice->SetMapper(mapper.Get());
  renderer->AddViewProp(imageSlice.Get());
  widget->GetRenderWindow()->AddRenderer(renderer.Get());

  // Set up render window interaction.
  this->defaultInteractorStyle->SetRenderOnMouseMove(true);

  this->widget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      this->defaultInteractorStyle.Get());

  renderer->SetBackground(1.0, 1.0, 1.0);
  renderer->SetViewport(0.0, 0.0,
                        1.0, 1.0);

  this->resetCamera();

  vtkScalarsToColors *lut =
      vtkScalarsToColors::SafeDownCast(this->unalignedData->colorMap()->GetClientSideObject());
  if (lut)
  {
    imageSlice->GetProperty()->SetLookupTable(lut);
  }

  // Now to add the controls to the widget.
  QHBoxLayout *viewControls = new QHBoxLayout;
  QPushButton *zoomToBox = new QPushButton(QIcon(":/pqWidgets/Icons/pqZoomToSelection24.png"),"Zoom to Selection");
  this->connect(zoomToBox, SIGNAL(pressed()), this, SLOT(zoomToSelectionStart()));
  viewControls->addWidget(zoomToBox);
  QPushButton *resetCamera = new QPushButton(QIcon(":/pqWidgets/Icons/pqResetCamera24.png"), "Reset");
  this->connect(resetCamera, SIGNAL(pressed()), this, SLOT(resetCamera()));
  viewControls->addWidget(resetCamera);
  v->addLayout(viewControls);

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
  referenceSlice=0;
  currentSlice->setRange(mapper->GetSliceNumberMinValue(),
                         mapper->GetSliceNumberMaxValue());
  connect(currentSlice, SIGNAL(valueChanged(int)), SLOT(setSlice(int)));
  connect(currentSlice, SIGNAL(valueChanged(int)), SLOT(updateReference()));
  grid->addWidget(currentSlice, gridrow, 1, 1, 1, Qt::AlignLeft);
  label = new QLabel("Shortcut: (A/S)");
  grid->addWidget(label, gridrow, 2, 1, 1, Qt::AlignRight);

  ++gridrow;
  label = new QLabel("Frame rate (fps):");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  QSpinBox *spin = new QSpinBox;
  spin->setRange(0, 50);
  spin->setValue(5);
  connect(spin, SIGNAL(valueChanged(int)), SLOT(setFrameRate(int)));
  grid->addWidget(spin, gridrow, 1, 1, 1, Qt::AlignLeft);

  // Reference image controls
  ++gridrow;
  label = new QLabel("Reference image:");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  prevButton = new QRadioButton("Prev");
  nextButton = new QRadioButton("Next");
  statButton = new QRadioButton("Static:");
  prevButton->setCheckable(true);
  nextButton->setCheckable(true);
  statButton->setCheckable(true);
  grid->addWidget(prevButton, gridrow, 1, 1, 1, Qt::AlignLeft);
  grid->addWidget(nextButton, gridrow, 2, 1, 1, Qt::AlignLeft);
  ++gridrow;
  grid->addWidget(statButton, gridrow, 1, 1, 1, Qt::AlignLeft);
  statRefNum = new QSpinBox;
  statRefNum->setValue(0);
  statRefNum->setRange(mapper->GetSliceNumberMinValue(),
                       mapper->GetSliceNumberMaxValue());
  connect(statRefNum, SIGNAL(valueChanged(int)), SLOT(updateReference()));
  grid->addWidget(statRefNum, gridrow, 2, 1, 1, Qt::AlignLeft);
  statRefNum->setEnabled(false);
  connect(statButton, SIGNAL(toggled(bool)), statRefNum, SLOT(setEnabled(bool)));

  referenceSliceMode = new QButtonGroup;
  referenceSliceMode->addButton(prevButton);
  referenceSliceMode->addButton(nextButton);
  referenceSliceMode->addButton(statButton);
  referenceSliceMode->setExclusive(true);
  prevButton->setChecked(true);
  connect(referenceSliceMode, SIGNAL(buttonClicked(int)), SLOT(updateReference()));

  // Slice offsets
  ++gridrow;
  currentSliceOffset = new QLabel("Image shift (Shortcut: arrow keys): (0, 0)");
  grid->addWidget(currentSliceOffset, gridrow, 0, 1, 3, Qt::AlignLeft);

  // Add our buttons.
  ++gridrow;
  QHBoxLayout *buttonLayout = new QHBoxLayout;
  startButton = new QPushButton("Start");
  connect(startButton, SIGNAL(clicked()), SLOT(startAlign()));
  buttonLayout->addWidget(startButton);
  startButton->setEnabled(false);
  stopButton = new QPushButton("Stop");
  connect(stopButton, SIGNAL(clicked()), SLOT(stopAlign()));
  buttonLayout->addWidget(stopButton);
  grid->addLayout(buttonLayout, gridrow, 0, 1, 2, Qt::AlignCenter);

  gridrow++;
  offsetTable = new QTableWidget(this);
  grid->addWidget(offsetTable, gridrow, 0, 1, 3, Qt::AlignCenter);
  offsets.fill(vtkVector2i(0, 0), mapper->GetSliceNumberMaxValue() + 1);

  const QVector<vtkVector2i> &oldOffsets = this->Op->getAlignOffsets();

  offsetTable->setRowCount(offsets.size());
  offsetTable->setColumnCount(3);
  QTableWidgetItem* item = new QTableWidgetItem();
  item->setText("Slice #");
  offsetTable->setHorizontalHeaderItem(0,item);
  item = new QTableWidgetItem();
  item->setText("X offset");
  offsetTable->setHorizontalHeaderItem(1,item);
  item = new QTableWidgetItem();
  item->setText("Y offset");
  offsetTable->setHorizontalHeaderItem(2,item);
  for (int i = 0; i < oldOffsets.size(); ++i)
  {
    this->offsets[i] = oldOffsets[i];
  }

  for (int i = 0; i < offsets.size(); ++i)
  {
    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(i));
    item->setFlags(Qt::ItemIsEnabled);
    offsetTable->setItem(i, 0, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(offsets[i][0]));
    offsetTable->setItem(i, 1, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(offsets[i][1]));
    offsetTable->setItem(i, 2, item);
  }
  offsetTable->resizeColumnsToContents();
  currentSliceOffset->setText(QString("Image shift (Shortcut: arrow keys): (%1, %2)")
      .arg(offsets[currentSlice->value()][0]).arg(offsets[currentSlice->value()][1]));

  /* Some test offsets.
  offsets[1] = vtkVector2i(10, 0);
  offsets[3] = vtkVector2i(-10, 0);
  offsets[5] = vtkVector2i(0, 10);
  offsets[7] = vtkVector2i(0, -10);
  offsets[9] = vtkVector2i(10, 10);
  offsets[11] = vtkVector2i(-10, -10); */

  connect(timer, SIGNAL(timeout()), SLOT(changeSlice()));
  connect(timer, SIGNAL(timeout()), widget, SLOT(update()));
  connect(offsetTable, SIGNAL(cellChanged(int, int)), SLOT(sliceOffsetEdited(int, int)));
  timer->start(100);
}

AlignWidget::~AlignWidget()
{
}

bool AlignWidget::eventFilter(QObject *object, QEvent *e)
{
  if (object == widget)
  {
    switch (e->type())
    {
      case QEvent::KeyPress:
        widgetKeyPress(static_cast<QKeyEvent *>(e));
        return true;
      case QEvent::KeyRelease:
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

void AlignWidget::changeSlice()
{
  // Does not change currentSlice, display only.
  int i = mapper->GetSliceNumber();
  if (i == currentSlice->value())
  {
    i = referenceSlice;
  }
  else
  {
    i = currentSlice->value();
  }
  setSlice(i, false);
}

void AlignWidget::changeSlice(int delta)
{
  // Changes currentSlice.
  int min = mapper->GetSliceNumberMinValue();
  int max = mapper->GetSliceNumberMaxValue();
  int i = currentSlice->value() + delta;

  // This makes stack circular.
  if (i > max)
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
  // Does not change currentSlice, display only.
  if (resetInc)
  {
    currentSliceOffset->setText(QString("Image shift (Shortcut: arrow keys): (%1, %2)")
        .arg(offsets[slice][0]).arg(offsets[slice][1]));
  }
  mapper->SetSliceNumber(slice);
  applySliceOffset(slice);
}

void AlignWidget::updateReference()
{
  int min = mapper->GetSliceNumberMinValue();
  int max = mapper->GetSliceNumberMaxValue();

  if (prevButton->isChecked())
  {
    referenceSlice = currentSlice->value() - 1;
  }
  else if (nextButton->isChecked())
  {
    referenceSlice = currentSlice->value() + 1;
  }
  else if (statButton->isChecked())
  {
    referenceSlice = statRefNum->value();
  }

  // This makes the stack circular.
  if (referenceSlice > max)
  {
    referenceSlice = min;
  }
  else if (referenceSlice < min)
  {
    referenceSlice = max;
  }
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
  bool updateTable = false;
  switch (key->key())
  {
  case Qt::Key_Left:
    offset[0] -= 1;
    updateTable = true;
    break;
  case Qt::Key_Right:
    offset[0] += 1;
    updateTable = true;
    break;
  case Qt::Key_Up:
    offset[1] += 1;
    updateTable = true;
    break;
  case Qt::Key_Down:
    offset[1] -= 1;
    updateTable = true;
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
  if (updateTable)
  {
    int sliceNumber = this->currentSlice->value();
    QTableWidgetItem *item = this->offsetTable->item(sliceNumber, 1);
    item->setData(Qt::DisplayRole, QString::number(offset[0]));
    item = this->offsetTable->item(sliceNumber, 2);
    item->setData(Qt::DisplayRole, QString::number(offset[1]));
  }
  applySliceOffset();
}

void AlignWidget::applySliceOffset(int sliceNumber)
{
  vtkVector2i offset(0, 0);
  if (sliceNumber == -1)
  {
    offset = offsets[currentSlice->value()];
    currentSliceOffset->setText(QString("Image shift (Shortcut: arrow keys): (%1, %2)").arg(offset[0])
        .arg(offset[1]));
  }
  else
  {
    offset = offsets[sliceNumber];
  }
  imageSlice->SetPosition(offset[0], offset[1], 0);
}

void AlignWidget::startAlign()
{
  if (!timer->isActive())
  {
    timer->start(1000.0 / frameRate);
  }
  startButton->setEnabled(false);
  stopButton->setEnabled(true);
}

void AlignWidget::stopAlign()
{
  timer->stop();
  setSlice(currentSlice->value());
  startButton->setEnabled(true);
  stopButton->setEnabled(false);
}

void AlignWidget::zoomToSelectionStart()
{
  this->widget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      this->zoomToBoxInteractorStyle.Get());
  this->observerId = this->widget->GetRenderWindow()->GetInteractor()->AddObserver(
      vtkCommand::LeftButtonReleaseEvent, this, &AlignWidget::zoomToSelectionFinished);
}

void AlignWidget::zoomToSelectionFinished()
{
  this->widget->GetRenderWindow()->GetInteractor()
      ->RemoveObserver(this->observerId);
  this->widget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      this->defaultInteractorStyle.Get());
}

void AlignWidget::applyChangesToOperator()
{
  if (this->Op)
  {
    this->Op->setAlignOffsets(this->offsets);
  }
}

void AlignWidget::resetCamera()
{
  vtkCamera *camera = renderer->GetActiveCamera();
  double *bounds = mapper->GetBounds();
  vtkVector3d point;
  point[0] = 0.5 * (bounds[0] + bounds[1]);
  point[1] = 0.5 * (bounds[2] + bounds[3]);
  point[2] = 0.5 * (bounds[4] + bounds[5]);
  camera->SetFocalPoint(point.GetData());
  point[2] += 50 + 0.5 * (bounds[4] + bounds[5]);
  camera->SetPosition(point.GetData());
  camera->SetViewUp(0.0, 1.0, 0.0);
  camera->ParallelProjectionOn();
  double parallelScale;
  if (bounds[1] - bounds[0] <
        bounds[3] - bounds[2])
  {
    parallelScale = 0.5 * (bounds[3] - bounds[2] + 1);
  }
  else
  {
    parallelScale = 0.5 * (bounds[1] - bounds[0] + 1);
  }
  camera->SetParallelScale(parallelScale);
  double clippingRange[2];
  camera->GetClippingRange(clippingRange);
  clippingRange[1] = clippingRange[0] + (bounds[5] - bounds[4] + 50);
  camera->SetClippingRange(clippingRange);
}

void AlignWidget::sliceOffsetEdited(int slice, int offsetComponent)
{
  QTableWidgetItem* item = this->offsetTable->item(slice, offsetComponent);
  QString str = item->data(Qt::DisplayRole).toString();
  bool ok;
  int offset = str.toInt(&ok);
  if (ok)
  {
    this->offsets[slice][offsetComponent - 1] = offset;
  }
  if (slice == this->currentSlice->value())
  {
    applySliceOffset();
  }
}

}
