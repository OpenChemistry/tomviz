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

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "TranslateAlignOperator.h"
#include "Utilities.h"

#include "vtk_jsoncpp.h"

#include <QVTKWidget.h>
#include <vtkArrayDispatch.h>
#include <vtkAssume.h>
#include <vtkCamera.h>
#include <vtkDataArrayAccessor.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkPVArrayInformation.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionPresets.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkTrivialProducer.h>
#include <vtkScalarsToColors.h>
#include <vtkSmartPointer.h>
#include <vtkSMSourceProxy.h>
#include <vtkNew.h>
#include <vtkVector.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkInteractorStyleRubberBandZoom.h>

#include <QTimer>
#include <QComboBox>
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
#include <QHeaderView>
#include <QButtonGroup>

namespace tomviz
{

class ViewMode {
public:
  ViewMode(vtkImageData *data)
    : originalData(data), currentSlice(1), referenceSlice(0)
  {
    this->currentSliceOffset[0] = 0;
    this->currentSliceOffset[1] = 0;
    this->referenceSliceOffset[0] = 0;
    this->referenceSliceOffset[1] = 0;
  }
  virtual ~ViewMode() {}
  virtual void addToView(vtkRenderer* renderer) = 0;
  virtual void removeFromView(vtkRenderer* renderer) = 0;
  void currentSliceUpdated(int sliceNumber, vtkVector2i offset)
  {
    this->currentSlice = sliceNumber;
    this->currentSliceOffset[0] = offset[0];
    this->currentSliceOffset[1] = offset[1];
    this->update();
  }
  void referenceSliceUpdated(int sliceNumber, vtkVector2i offset)
  {
    this->referenceSlice = sliceNumber;
    this->referenceSliceOffset[0] = offset[0];
    this->referenceSliceOffset[1] = offset[1];
    this->update();
  }
  virtual void timeout() {}
  virtual double *bounds() const = 0;
  virtual void update() = 0;
protected:
  vtkSmartPointer<vtkImageData> originalData;
  int currentSlice;
  vtkVector2i currentSliceOffset;
  int referenceSlice;
  vtkVector2i referenceSliceOffset;
};

class ToggleSliceShownViewMode : public ViewMode
{
public:
  ToggleSliceShownViewMode(vtkImageData *data, vtkScalarsToColors *dataLUT)
    : ViewMode(data), showingCurrentSlice(false)
  {
    this->imageSliceMapper->SetInputData(data);
    this->imageSliceMapper->Update();
    this->imageSlice->SetMapper(this->imageSliceMapper.Get());
    if (dataLUT)
    {
      this->imageSlice->GetProperty()->SetLookupTable(dataLUT);
    }
  }
  void addToView(vtkRenderer* renderer) override
  {
    renderer->AddViewProp(this->imageSlice.Get());
  }
  void removeFromView(vtkRenderer* renderer) override
  {
    renderer->RemoveViewProp(this->imageSlice.Get());
  }
  void timeout() override
  {
    this->showingCurrentSlice = !this->showingCurrentSlice;
    this->update();
  }
  void update() override
  {
    if (this->showingCurrentSlice)
    {
      this->imageSliceMapper->SetSliceNumber(this->currentSlice);
      this->imageSliceMapper->Update();
      this->imageSlice->SetPosition(this->currentSliceOffset[0],
        this->currentSliceOffset[1], 0);
    }
    else // showing reference slice
    {
      this->imageSliceMapper->SetSliceNumber(this->referenceSlice);
      this->imageSliceMapper->Update();
      this->imageSlice->SetPosition(this->referenceSliceOffset[0],
        this->referenceSliceOffset[1], 0);
    }
  }
  double *bounds() const override
  {
    return this->imageSliceMapper->GetBounds();
  }
private:
  vtkNew<vtkImageSlice> imageSlice;
  vtkNew<vtkImageSliceMapper> imageSliceMapper;
  bool showingCurrentSlice;
};

class ShowDifferenceImageMode : public ViewMode
{
public:
  ShowDifferenceImageMode(vtkImageData *data)
    : ViewMode(data)
  {
    int extent[6];
    data->GetExtent(extent);
    extent[4] = 0;
    extent[5] = 0;
    this->xSize = extent[1] - extent[0] + 1;
    this->ySize = extent[3] - extent[2] + 1;
    this->diffImage->SetExtent(extent);
    this->diffImage->AllocateScalars(VTK_FLOAT, 1);
    this->imageSliceMapper->SetInputData(this->diffImage.Get());
    this->imageSliceMapper->Update();
    this->imageSlice->SetMapper(this->imageSliceMapper.Get());
    vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();

    vtkNew<vtkSMTransferFunctionManager> tfmgr;
    vtkSMProxy *lut = tfmgr->GetColorTransferFunction("AlignWidgetLUT", pxm);
    vtkScalarsToColors *dataLUT = vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
    vtkSmartPointer<vtkSMProxy> source;
    source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
    vtkTrivialProducer::SafeDownCast(source->GetClientSideObject())->SetOutput(data);
    vtkPVArrayInformation* ainfo = tomviz::scalarArrayInformation(
        vtkSMSourceProxy::SafeDownCast(source.Get()));
    if (ainfo != nullptr)
    {
      double range[2];
      ainfo->GetComponentRange(0, range);
      double lutRange[2] = { std::min(range[0], -range[1]),
                             std::max(range[1], -range[0]) };
      vtkNew<vtkSMTransferFunctionPresets> presets;
      vtkSMTransferFunctionProxy::ApplyPreset(lut, presets->GetFirstPresetWithName("Cool to Warm"));
      vtkSMTransferFunctionProxy::RescaleTransferFunction(lut, lutRange);
    }
    this->imageSlice->GetProperty()->SetLookupTable(dataLUT);
  }
  void addToView(vtkRenderer *renderer) override
  {
    renderer->AddViewProp(this->imageSlice.Get());
  }
  void removeFromView(vtkRenderer* renderer) override
  {
    renderer->RemoveViewProp(this->imageSlice.Get());
  }
  void update() override
  {
    int extent[6];
    this->originalData->GetExtent(extent);
    typedef vtkArrayDispatch::Dispatch2ByValueType
      <
        vtkArrayDispatch::AllTypes,
        vtkArrayDispatch::Reals
      > Dispatcher;
    if (!Dispatcher::Execute(this->originalData->GetPointData()->GetScalars(),
          this->diffImage->GetPointData()->GetScalars(), *this))
    {
      (*this)(this->originalData->GetPointData()->GetScalars(),
          this->diffImage->GetPointData()->GetScalars());
    }
    this->diffImage->Modified();
    this->imageSliceMapper->Update();
  }
  double *bounds() const override
  {
    return this->imageSliceMapper->GetBounds();
  }
  // Operator so that *this can be used with vtkArrayDispatch to compute the
  // difference image
  template <typename InputArray, typename OutputArray>
  void operator()(InputArray *input, OutputArray *output)
  {
    VTK_ASSUME(input->GetNumberOfComponents() == 1);
    VTK_ASSUME(output->GetNumberOfComponents() == 1);

    vtkDataArrayAccessor<InputArray> in(input);
    vtkDataArrayAccessor<OutputArray> out(output);

    for (vtkIdType j = 0; j < this->ySize; ++j)
    {
      for (vtkIdType i = 0; i < this->xSize; ++i)
      {
        vtkIdType destIdx = j * this->xSize + i;
        if (j + this->currentSliceOffset[1] < ySize &&
            j + currentSliceOffset[1] >= 0 &&
            i + this->currentSliceOffset[0] < this->xSize &&
            i + this->currentSliceOffset[0] >= 0)
        {
          // Index of the point in the current slice that corresponds to the given position
          vtkIdType currentSliceIdx = this->currentSlice * this->ySize * this->xSize +
            (j + this->currentSliceOffset[1]) * this->xSize + (i + this->currentSliceOffset[0]);
          // Index in the reference slice that corresponds to the given position
          vtkIdType referenceSliceIdx = this->referenceSlice * this->ySize * this->xSize +
            (j + this->referenceSliceOffset[1]) * this->xSize + (i + this->referenceSliceOffset[0]);
          // Compute the difference and set it to the output at the position
          out.Set(destIdx, 0, in.Get(currentSliceIdx, 0) -
              in.Get(referenceSliceIdx, 0));
        }
        else
        {
          // TODO - figure out what to do fort this region
          out.Set(destIdx, 0, 0);
        }
      }
    }
  }
private:
  vtkNew<vtkImageData> diffImage;
  vtkNew<vtkImageSliceMapper> imageSliceMapper;
  vtkNew<vtkImageSlice> imageSlice;
  vtkIdType xSize;
  vtkIdType ySize;
};

AlignWidget::AlignWidget(TranslateAlignOperator *op, QWidget* p)
  : EditOperatorWidget(p)
{
  this->timer = new QTimer(this);
  this->frameRate = 5;
  this->Op = op;
  this->unalignedData = op->getDataSource();
  this->widget = new QVTKWidget(this);
  this->widget->installEventFilter(this);
  QHBoxLayout *myLayout = new QHBoxLayout(this);
  myLayout->addWidget(this->widget);
  QVBoxLayout *v = new QVBoxLayout;
  myLayout->addLayout(v);
  this->setLayout(myLayout);
  this->setMinimumWidth(800);
  this->setMinimumHeight(600);
  this->setWindowTitle("Align data");

  // Grab the image data from the data source...
  vtkTrivialProducer *t =
    vtkTrivialProducer::SafeDownCast(this->unalignedData->producer()->GetClientSideObject());
  vtkScalarsToColors *lut =
    vtkScalarsToColors::SafeDownCast(this->unalignedData->colorMap()->GetClientSideObject());

  // Set up the rendering pipeline
  if (t)
  {
    vtkImageData *image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (image != nullptr)
    {
      this->modes.push_back(
          new ToggleSliceShownViewMode(image, lut));
      this->modes.push_back(
          new ShowDifferenceImageMode(image));
      this->modes[0]->addToView(this->renderer.Get());
      this->modes[0]->update();
    int extent[6];
      image->GetExtent(extent);
      this->minSliceNum = extent[4];
      this->maxSliceNum = extent[5];
    }
    else
    {
      this->minSliceNum = 0;
      this->maxSliceNum = 1;
    }
  }
  else
  {
    this->minSliceNum = 0;
    this->maxSliceNum = 1;
  }
  this->widget->GetRenderWindow()->AddRenderer(this->renderer.Get());
  this->renderer->SetBackground(1.0, 1.0, 1.0);
  this->renderer->SetViewport(0.0, 0.0,
                        1.0, 1.0);

  // Set up render window interaction.
  this->defaultInteractorStyle->SetRenderOnMouseMove(true);

  this->widget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      this->defaultInteractorStyle.Get());

  this->renderer->SetBackground(1.0, 1.0, 1.0);
  this->renderer->SetViewport(0.0, 0.0,
                        1.0, 1.0);

  this->resetCamera();

  // Now to add the controls to the widget.
  QHBoxLayout *viewControls = new QHBoxLayout;
  QPushButton *zoomToBox = new QPushButton(QIcon(":/pqWidgets/Icons/pqZoomToSelection24.png"),"Zoom to Selection");
  this->connect(zoomToBox, SIGNAL(pressed()),
                this, SLOT(zoomToSelectionStart()));
  viewControls->addWidget(zoomToBox);
  QPushButton *resetCamera = new QPushButton(QIcon(":/pqWidgets/Icons/pqResetCamera24.png"), "Reset");
  this->connect(resetCamera, SIGNAL(pressed()), this, SLOT(resetCamera()));
  viewControls->addWidget(resetCamera);
  v->addLayout(viewControls);

  this->currentMode = 0;
  this->modeSelect = new QComboBox;
  this->modeSelect->addItem("Toggle Images");
  this->modeSelect->addItem("Show Difference");
  this->modeSelect->setCurrentIndex(0);
  this->connect(this->modeSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(changeMode(int)));
  v->addWidget(this->modeSelect);

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
  this->currentSlice = new QSpinBox;
  this->currentSlice->setValue(1);
  this->currentSlice->setRange(this->minSliceNum,
                                          this->maxSliceNum);
  connect(this->currentSlice, SIGNAL(valueChanged(int)), SLOT(setSlice(int)));
  connect(this->currentSlice, SIGNAL(valueChanged(int)), SLOT(updateReference()));
  grid->addWidget(this->currentSlice, gridrow, 1, 1, 1, Qt::AlignLeft);
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
  this->prevButton = new QRadioButton("Prev");
  this->nextButton = new QRadioButton("Next");
  this->statButton = new QRadioButton("Static:");
  this->prevButton->setCheckable(true);
  this->nextButton->setCheckable(true);
  this->statButton->setCheckable(true);
  grid->addWidget(this->prevButton, gridrow, 1, 1, 1, Qt::AlignLeft);
  grid->addWidget(this->nextButton, gridrow, 2, 1, 1, Qt::AlignLeft);
  ++gridrow;
  grid->addWidget(this->statButton, gridrow, 1, 1, 1, Qt::AlignLeft);
  this->statRefNum = new QSpinBox;
  this->statRefNum->setValue(0);
  this->statRefNum->setRange(this->minSliceNum,
                       this->maxSliceNum);
  connect(this->statRefNum, SIGNAL(valueChanged(int)), SLOT(updateReference()));
  grid->addWidget(this->statRefNum, gridrow, 2, 1, 1, Qt::AlignLeft);
  this->statRefNum->setEnabled(false);
  connect(this->statButton, SIGNAL(toggled(bool)), this->statRefNum, SLOT(setEnabled(bool)));

  this->referenceSliceMode = new QButtonGroup;
  this->referenceSliceMode->addButton(this->prevButton);
  this->referenceSliceMode->addButton(this->nextButton);
  this->referenceSliceMode->addButton(this->statButton);
  this->referenceSliceMode->setExclusive(true);
  this->prevButton->setChecked(true);
  connect(this->referenceSliceMode, SIGNAL(buttonClicked(int)), SLOT(updateReference()));

  // Slice offsets
  ++gridrow;
  this->currentSliceOffset = new QLabel("Image shift (Shortcut: arrow keys): (0, 0)");
  grid->addWidget(this->currentSliceOffset, gridrow, 0, 1, 3, Qt::AlignLeft);

  // Add our buttons.
  ++gridrow;
  QHBoxLayout *buttonLayout = new QHBoxLayout;
  this->startButton = new QPushButton("Start");
  connect(this->startButton, SIGNAL(clicked()), SLOT(startAlign()));
  buttonLayout->addWidget(this->startButton);
  this->startButton->setEnabled(false);
  this->stopButton = new QPushButton("Stop");
  connect(this->stopButton, SIGNAL(clicked()), SLOT(stopAlign()));
  buttonLayout->addWidget(this->stopButton);
  grid->addLayout(buttonLayout, gridrow, 0, 1, 2, Qt::AlignCenter);

  gridrow++;
  this->offsetTable = new QTableWidget(this);
  this->offsetTable->verticalHeader()->setVisible(false);
  grid->addWidget(this->offsetTable, gridrow, 0, 1, 3, Qt::AlignCenter);
  this->offsets.fill(vtkVector2i(0, 0), this->maxSliceNum + 1);

  const QVector<vtkVector2i> &oldOffsets = this->Op->getAlignOffsets();

  this->offsetTable->setRowCount(this->offsets.size());
  this->offsetTable->setColumnCount(4);
  QTableWidgetItem* item = new QTableWidgetItem();
  item->setText("Slice #");
  this->offsetTable->setHorizontalHeaderItem(0,item);
  item = new QTableWidgetItem();
  item->setText("X offset");
  this->offsetTable->setHorizontalHeaderItem(1,item);
  item = new QTableWidgetItem();
  item->setText("Y offset");
  this->offsetTable->setHorizontalHeaderItem(2,item);
  item = new QTableWidgetItem();
  item->setText("Tilt angle");
  this->offsetTable->setHorizontalHeaderItem(3,item);
  for (int i = 0; i < oldOffsets.size(); ++i)
  {
    this->offsets[i] = oldOffsets[i];
  }

  QVector<double> tiltAngles = this->unalignedData->getTiltAngles();

  for (int i = 0; i < this->offsets.size(); ++i)
  {
    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(i));
    item->setFlags(Qt::ItemIsEnabled);
    this->offsetTable->setItem(i, 0, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(this->offsets[i][0]));
    this->offsetTable->setItem(i, 1, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(this->offsets[i][1]));
    this->offsetTable->setItem(i, 2, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
    item->setFlags(Qt::ItemIsEnabled);
    this->offsetTable->setItem(i, 3, item);
  }
  this->offsetTable->resizeColumnsToContents();
  this->currentSliceOffset->setText(QString("Image shift (Shortcut: arrow keys): (%1, %2)")
      .arg(this->offsets[this->currentSlice->value()][0])
      .arg(this->offsets[this->currentSlice->value()][1]));

  connect(this->timer, SIGNAL(timeout()), SLOT(onTimeout()));
  connect(this->offsetTable, SIGNAL(cellChanged(int, int)), SLOT(sliceOffsetEdited(int, int)));
  this->timer->start(200);
}

AlignWidget::~AlignWidget()
{
  qDeleteAll(this->modes);
  this->modes.clear();
}

bool AlignWidget::eventFilter(QObject *object, QEvent *e)
{
  if (object == this->widget)
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

void AlignWidget::onTimeout()
{
  if (this->modes.length() > 0)
  {
    this->modes[this->currentMode]->timeout();
  }
  this->widget->update();
}

void AlignWidget::changeSlice(int delta)
{
  // Changes currentSlice.
  int min = this->minSliceNum;
  int max = this->maxSliceNum;
  int i = this->currentSlice->value() + delta;

  // This makes stack circular.
  if (i > max)
  {
    i = min;
  }
  else if (i < min)
  {
    i = max;
  }
  this->currentSlice->setValue(i);
  this->setSlice(i, false);
}

void AlignWidget::setSlice(int slice, bool resetInc)
{
  // Does not change currentSlice, display only.
  if (resetInc)
  {
    this->currentSliceOffset->setText(QString(
          "Image shift (Shortcut: arrow keys): (%1, %2)")
           .arg(this->offsets[slice][0]).arg(this->offsets[slice][1]));
  }
  this->applySliceOffset(slice);
}

void AlignWidget::updateReference()
{
  int min = this->minSliceNum;
  int max = this->maxSliceNum;

  int refSlice = 0;

  if (this->prevButton->isChecked())
  {
    refSlice = this->currentSlice->value() - 1;
  }
  else if (this->nextButton->isChecked())
  {
    refSlice = this->currentSlice->value() + 1;
  }
  else if (this->statButton->isChecked())
  {
    refSlice = this->statRefNum->value();
  }

  // This makes the stack circular.
  if (refSlice > max)
  {
    refSlice = min;
  }
  else if (refSlice < min)
  {
    refSlice = max;
  }
  this->referenceSlice = refSlice;
  for (int i = 0; i < this->modes.length(); ++i)
  {
    this->modes[i]->referenceSliceUpdated(
        referenceSlice, this->offsets[referenceSlice]);
  }
}

void AlignWidget::setFrameRate(int rate)
{
  if (rate < 0)
  {
    this->frameRate = 0;
  }
  this->frameRate = rate;
  if (this->frameRate > 0)
  {
    this->timer->setInterval(1000.0 / this->frameRate);
    if (!this->timer->isActive())
    {
      this->timer->start();
    }
  }
  else
  {
    this->timer->stop();
  }
}

void AlignWidget::widgetKeyPress(QKeyEvent *key)
{
  vtkVector2i &offset = this->offsets[this->currentSlice->value()];
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
  this->applySliceOffset();
}

void AlignWidget::changeMode(int mode)
{
  if (this->modes.length() == 0)
  {
    return;
  }
  this->modes[this->currentMode]->removeFromView(this->renderer.Get());
  this->currentMode = mode;
  this->modes[this->currentMode]->addToView(this->renderer.Get());
  this->modes[this->currentMode]->update();
  this->resetCamera();
}

void AlignWidget::applySliceOffset(int sliceNumber)
{
  vtkVector2i offset(0, 0);
  if (sliceNumber == -1)
  {
    sliceNumber = this->currentSlice->value();
    offset = this->offsets[this->currentSlice->value()];
    this->currentSliceOffset->setText(QString("Image shift (Shortcut: arrow keys): (%1, %2)").arg(offset[0])
        .arg(offset[1]));
  }
  else
  {
    offset = this->offsets[sliceNumber];
  }
  for (int i = 0; i < this->modes.length(); ++i)
  {
    this->modes[i]->currentSliceUpdated(sliceNumber, offset);
  }
}

void AlignWidget::startAlign()
{
  if (!this->timer->isActive())
  {
    this->timer->start(1000.0 / this->frameRate);
  }
  this->startButton->setEnabled(false);
  this->stopButton->setEnabled(true);
}

void AlignWidget::stopAlign()
{
  this->timer->stop();
  setSlice(this->currentSlice->value());
  this->startButton->setEnabled(true);
  this->stopButton->setEnabled(false);
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
  if (this->modes.length() == 0)
  {
    return;
  }
  vtkCamera *camera = this->renderer->GetActiveCamera();
  double *bounds = this->modes[this->currentMode]->bounds();
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
  if (bounds[1] - bounds[0] < bounds[3] - bounds[2])
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
    this->applySliceOffset();
  }
  if (slice == this->referenceSlice)
  {
    this->applySliceOffset(this->referenceSlice);
  }
}

}
