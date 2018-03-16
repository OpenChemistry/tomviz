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
#include "SpinBox.h"
#include "TranslateAlignOperator.h"
#include "QVTKGLWidget.h"
#include "Utilities.h"

#include <vtk_jsoncpp.h>

#include <pqPresetDialog.h>
#include <pqView.h>
#include <vtkPVArrayInformation.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionPresets.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>

#include <vtkArrayDispatch.h>
#include <vtkAssume.h>
#include <vtkCamera.h>
#include <vtkDataArray.h>
#include <vtkDataArrayAccessor.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkInteractorStyleRubberBandZoom.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>
#include <vtkVector.h>

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace tomviz {

namespace {
void renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}
}

class ViewMode
{
public:
  ViewMode(vtkImageData* data) : m_originalData(data)
  {
    m_currentSliceOffset[0] = 0;
    m_currentSliceOffset[1] = 0;
    m_referenceSliceOffset[0] = 0;
    m_referenceSliceOffset[1] = 0;
  }
  virtual ~ViewMode() {}
  virtual void addToView(vtkRenderer* renderer) = 0;
  virtual void removeFromView(vtkRenderer* renderer) = 0;
  void currentSliceUpdated(int sliceNumber, vtkVector2i offset)
  {
    m_currentSlice = sliceNumber;
    m_currentSliceOffset[0] = offset[0];
    m_currentSliceOffset[1] = offset[1];
    update();
  }
  void referenceSliceUpdated(int sliceNumber, vtkVector2i offset)
  {
    m_referenceSlice = sliceNumber;
    m_referenceSliceOffset[0] = offset[0];
    m_referenceSliceOffset[1] = offset[1];
    update();
  }
  void brightnessAndContrast(double& brightness, double& contrast)
  {
    vtkScalarsToColors* dataLUT =
      vtkScalarsToColors::SafeDownCast(getLUT()->GetClientSideObject());
    if (dataLUT) {
      double* adjustedRange = dataLUT->GetRange();
      double dataRange[2];
      range(dataRange);

      brightness = adjustedRange[1] / dataRange[1];
      contrast = 1. - (adjustedRange[0] - dataRange[0]) / dataRange[1];
    }
  }
  void setBrightnessAndContrast(double brightness, double contrast)
  {
    // clamp brightness and contrast between 0. and 1.
    brightness = brightness < 0. ? 0. : brightness > 1. ? 1. : brightness;
    contrast = contrast < 0. ? 0. : contrast > 1. ? 1. : contrast;

    vtkScalarsToColors* dataLUT =
      vtkScalarsToColors::SafeDownCast(getLUT()->GetClientSideObject());
    if (dataLUT) {
      double dataRange[2];
      range(dataRange);
      double adjustedRange[2];
      adjustedRange[1] = dataRange[1] * brightness;
      adjustedRange[0] = dataRange[0] + dataRange[1] * (1. - contrast);

      vtkSMTransferFunctionProxy::RescaleTransferFunction(getLUT(),
                                                          adjustedRange);
    }
  }
  virtual void range(double r[2]) { m_originalData->GetScalarRange(r); }

  virtual void timeout() {}
  virtual void timerStopped() {}
  virtual double* bounds() const = 0;
  virtual void update() = 0;
  virtual vtkSMProxy* getLUT() = 0;

protected:
  vtkSmartPointer<vtkImageData> m_originalData;
  int m_currentSlice = 1;
  vtkVector2i m_currentSliceOffset;
  int m_referenceSlice = 0;
  vtkVector2i m_referenceSliceOffset;
};

class ToggleSliceShownViewMode : public ViewMode
{
public:
  ToggleSliceShownViewMode(vtkImageData* data, vtkSMProxy* lutProxy)
    : ViewMode(data)
  {
    m_imageSlice->GetProperty()->SetInterpolationTypeToNearest();
    m_imageSliceMapper->SetInputData(data);
    m_imageSliceMapper->Update();
    m_imageSlice->SetMapper(m_imageSliceMapper.Get());
    m_lut = lutProxy;
    vtkScalarsToColors* dataLUT =
      vtkScalarsToColors::SafeDownCast(lutProxy->GetClientSideObject());
    if (dataLUT) {
      m_imageSlice->GetProperty()->SetLookupTable(dataLUT);
    }
    data->GetSpacing(m_spacing);
  }
  void addToView(vtkRenderer* renderer) override
  {
    renderer->AddViewProp(m_imageSlice.Get());
  }
  void removeFromView(vtkRenderer* renderer) override
  {
    renderer->RemoveViewProp(m_imageSlice.Get());
  }
  void timeout() override
  {
    m_showingCurrentSlice = !m_showingCurrentSlice;
    update();
  }
  void timerStopped() override
  {
    m_showingCurrentSlice = true;
    update();
  }
  void update() override
  {
    if (m_showingCurrentSlice) {
      m_imageSliceMapper->SetSliceNumber(m_currentSlice);
      m_imageSliceMapper->Update();
      m_imageSlice->SetPosition(m_currentSliceOffset[0] * m_spacing[0],
                                m_currentSliceOffset[1] * m_spacing[1], 0);
    } else // showing reference slice
    {
      m_imageSliceMapper->SetSliceNumber(m_referenceSlice);
      m_imageSliceMapper->Update();
      m_imageSlice->SetPosition(m_referenceSliceOffset[0] * m_spacing[0],
                                m_referenceSliceOffset[1] * m_spacing[1], 0);
    }
  }
  double* bounds() const override { return m_imageSliceMapper->GetBounds(); }
  vtkSMProxy* getLUT() override { return m_lut; }
private:
  vtkNew<vtkImageSlice> m_imageSlice;
  vtkNew<vtkImageSliceMapper> m_imageSliceMapper;
  vtkSmartPointer<vtkSMProxy> m_lut;
  double m_spacing[3];
  bool m_showingCurrentSlice = false;
};

class ShowDifferenceImageMode : public ViewMode
{
public:
  ShowDifferenceImageMode(vtkImageData* data) : ViewMode(data)
  {
    int extent[6];
    data->GetExtent(extent);
    extent[4] = 0;
    extent[5] = 0;
    m_xSize = extent[1] - extent[0] + 1;
    m_ySize = extent[3] - extent[2] + 1;
    m_diffImage->SetExtent(extent);
    m_diffImage->AllocateScalars(VTK_FLOAT, 1);
    m_imageSliceMapper->SetInputData(m_diffImage.Get());
    m_imageSliceMapper->Update();
    m_imageSlice->SetMapper(m_imageSliceMapper.Get());
    vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();

    vtkNew<vtkSMTransferFunctionManager> tfmgr;
    m_lut = tfmgr->GetColorTransferFunction("AlignWidgetLUT", pxm);
    vtkScalarsToColors* dataLUT =
      vtkScalarsToColors::SafeDownCast(m_lut->GetClientSideObject());
    vtkNew<vtkSMTransferFunctionPresets> presets;
    vtkSMTransferFunctionProxy::ApplyPreset(
      m_lut, presets->GetFirstPresetWithName("Cool to Warm (Extended)"));
    m_imageSlice->GetProperty()->SetLookupTable(dataLUT);
  }
  void addToView(vtkRenderer* renderer) override
  {
    renderer->AddViewProp(m_imageSlice.Get());
  }
  void removeFromView(vtkRenderer* renderer) override
  {
    renderer->RemoveViewProp(m_imageSlice.Get());
  }
  void update() override
  {
    int extent[6];
    m_originalData->GetExtent(extent);
    typedef vtkArrayDispatch::Dispatch2ByValueType<vtkArrayDispatch::AllTypes,
                                                   vtkArrayDispatch::Reals>
      Dispatcher;
    if (!Dispatcher::Execute(m_originalData->GetPointData()->GetScalars(),
                             m_diffImage->GetPointData()->GetScalars(),
                             *this)) {
      (*this)(m_originalData->GetPointData()->GetScalars(),
              m_diffImage->GetPointData()->GetScalars());
    }
    m_diffImage->Modified();
    m_imageSliceMapper->Update();
  }
  virtual void range(double r[2]) override
  {
    vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
    vtkSmartPointer<vtkSMProxy> source;
    source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
    vtkTrivialProducer::SafeDownCast(source->GetClientSideObject())
      ->SetOutput(m_originalData);
    vtkPVArrayInformation* ainfo = tomviz::scalarArrayInformation(
      vtkSMSourceProxy::SafeDownCast(source.Get()));

    if (ainfo != nullptr) {
      double range[2];
      ainfo->GetComponentRange(0, range);
      r[0] = std::min(range[0], -range[1]);
      r[1] = std::max(range[1], -range[0]);
    }
  }
  double* bounds() const override { return m_imageSliceMapper->GetBounds(); }
  vtkSMProxy* getLUT() override { return m_lut; }
  // Operator so that *this can be used with vtkArrayDispatch to compute the
  // difference image
  template <typename InputArray, typename OutputArray>
  void operator()(InputArray* input, OutputArray* output)
  {
    VTK_ASSUME(input->GetNumberOfComponents() == 1);
    VTK_ASSUME(output->GetNumberOfComponents() == 1);

    vtkDataArrayAccessor<InputArray> in(input);
    vtkDataArrayAccessor<OutputArray> out(output);

    for (vtkIdType j = 0; j < m_ySize; ++j) {
      for (vtkIdType i = 0; i < m_xSize; ++i) {
        vtkIdType destIdx = j * m_xSize + i;
        if (j - m_currentSliceOffset[1] < m_ySize &&
            j - m_currentSliceOffset[1] >= 0 &&
            i - m_currentSliceOffset[0] < m_xSize &&
            i - m_currentSliceOffset[0] >= 0) {
          // Index of the point in the current slice that corresponds to the
          // given position
          vtkIdType currentSliceIdx = m_currentSlice * m_ySize * m_xSize +
                                      (j - m_currentSliceOffset[1]) * m_xSize +
                                      (i - m_currentSliceOffset[0]);
          // Index in the reference slice that corresponds to the given position
          vtkIdType referenceSliceIdx =
            m_referenceSlice * m_ySize * m_xSize +
            (j - m_referenceSliceOffset[1]) * m_xSize +
            (i - m_referenceSliceOffset[0]);
          // Compute the difference and set it to the output at the position
          out.Set(destIdx, 0,
                  in.Get(currentSliceIdx, 0) - in.Get(referenceSliceIdx, 0));
        } else {
          // TODO - figure out what to do fort this region
          out.Set(destIdx, 0, 0);
        }
      }
    }
  }

private:
  vtkNew<vtkImageData> m_diffImage;
  vtkNew<vtkImageSliceMapper> m_imageSliceMapper;
  vtkNew<vtkImageSlice> m_imageSlice;
  vtkSmartPointer<vtkSMProxy> m_lut;
  vtkIdType m_xSize;
  vtkIdType m_ySize;
};

AlignWidget::AlignWidget(TranslateAlignOperator* op,
                         vtkSmartPointer<vtkImageData> imageData, QWidget* p)
  : EditOperatorWidget(p)
{
  m_timer = new QTimer(this);
  m_operator = op;
  m_inputData = imageData;
  m_widget = new QVTKGLWidget(this);
  m_widget->installEventFilter(this);

  // Use a horizontal layout, main GUI to the left, controls/text to the right.
  QHBoxLayout* myLayout = new QHBoxLayout(this);
  myLayout->addWidget(m_widget, 5);
  m_widget->setMinimumWidth(400);
  m_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QVBoxLayout* v = new QVBoxLayout;
  v->setSizeConstraint(QLayout::SetMinimumSize);
  myLayout->addLayout(v);
  setLayout(myLayout);
  setMinimumWidth(800);
  setMinimumHeight(600);
  setWindowTitle("Align data");

  // Grab the image data from the data source...
  vtkSMProxy* lut = op->getDataSource()->colorMap();

  // Set up the rendering pipeline
  if (imageData) {
    m_modes.push_back(new ToggleSliceShownViewMode(imageData, lut));
    m_modes.push_back(new ShowDifferenceImageMode(imageData));
    m_modes[0]->addToView(m_renderer.Get());
    m_modes[0]->update();
    int extent[6];
    imageData->GetExtent(extent);
    m_minSliceNum = extent[4];
    m_maxSliceNum = extent[5];
  } else {
    m_minSliceNum = 0;
    m_maxSliceNum = 1;
  }
  m_widget->GetRenderWindow()->AddRenderer(m_renderer.Get());

  // Set up render window interaction.
  m_defaultInteractorStyle->SetRenderOnMouseMove(true);

  m_widget->GetInteractor()->SetInteractorStyle(m_defaultInteractorStyle.Get());

  m_renderer->SetBackground(1.0, 1.0, 1.0);
  m_renderer->SetViewport(0.0, 0.0, 1.0, 1.0);

  resetCamera();

  // Now to add the controls to the widget.
  QHBoxLayout* viewControls = new QHBoxLayout;
  QPushButton* zoomToBox = new QPushButton(
    QIcon(":/pqWidgets/Icons/pqZoomToSelection24.png"), "Zoom to Selection");
  connect(zoomToBox, SIGNAL(pressed()), this, SLOT(zoomToSelectionStart()));
  viewControls->addWidget(zoomToBox);
  QPushButton* resetCamera = new QPushButton(
    QIcon(":/pqWidgets/Icons/pqResetCamera24.png"), "Reset View");
  connect(resetCamera, SIGNAL(pressed()), this, SLOT(resetCamera()));
  viewControls->addWidget(resetCamera);

  v->addLayout(viewControls);

  static const double sliderRange[2] = { 0., 100. };

  QWidget* brightnessAndContrastWidget = new QWidget;
  QFormLayout* brightnessAndContrastControls = new QFormLayout;
  brightnessAndContrastWidget->setLayout(brightnessAndContrastControls);
  QSlider* brightness = new QSlider(Qt::Horizontal);
  brightness->setMinimum(sliderRange[0]);
  brightness->setMaximum(sliderRange[1]);
  connect(brightness, &QSlider::valueChanged, this,
          // use a lambda to convert integer (0,100) to float (0.,1.)
          [&](int i) {
            double bAndC[2] = { 0.0, 0.0 };
            // grab current values
            m_modes[m_currentMode]->brightnessAndContrast(bAndC[0], bAndC[1]);
            // set the new values
            m_modes[m_currentMode]->setBrightnessAndContrast(
              ((static_cast<double>(i) - sliderRange[0]) / sliderRange[1]),
              bAndC[1]);
            m_widget->GetRenderWindow()->Render();
          });
  brightness->setValue(sliderRange[1]);
  brightnessAndContrastControls->addRow("Brightness", brightness);

  QSlider* contrast = new QSlider(Qt::Horizontal);
  contrast->setMinimum(sliderRange[0]);
  contrast->setMaximum(sliderRange[1]);
  connect(contrast, &QSlider::valueChanged, this, [&](int i) {
    double bAndC[2] = { 0.0, 0.0 };
    m_modes[m_currentMode]->brightnessAndContrast(bAndC[0], bAndC[1]);
    m_modes[m_currentMode]->setBrightnessAndContrast(
      bAndC[0], ((static_cast<double>(i) - sliderRange[0]) / sliderRange[1]));
    m_widget->GetRenderWindow()->Render();
  });
  contrast->setValue(sliderRange[1]);
  brightnessAndContrastControls->addRow("Contrast", contrast);
  v->addWidget(brightnessAndContrastWidget);

  m_currentMode = 0;
  QHBoxLayout* optionsLayout = new QHBoxLayout;
  m_modeSelect = new QComboBox;
  m_modeSelect->addItem("Toggle Images");
  m_modeSelect->addItem("Show Difference");
  m_modeSelect->setCurrentIndex(0);
  connect(m_modeSelect, SIGNAL(currentIndexChanged(int)), this,
          SLOT(changeMode(int)));
  optionsLayout->addWidget(m_modeSelect);

  QToolButton* presetSelectorButton = new QToolButton;
  presetSelectorButton->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites16.png"));
  presetSelectorButton->setToolTip("Choose preset color map");
  connect(presetSelectorButton, SIGNAL(clicked()), this,
          SLOT(onPresetClicked()));
  optionsLayout->addWidget(presetSelectorButton);
  v->addLayout(optionsLayout);

  // get tilt angles and determine initial reference image
  QVector<double> tiltAngles;
  auto fd = imageData->GetFieldData();
  if (fd->HasArray("tilt_angles")) {
    auto angles = fd->GetArray("tilt_angles");
    tiltAngles.resize(angles->GetNumberOfTuples());
    for (int i = 0; i < tiltAngles.size(); ++i) {
      tiltAngles[i] = angles->GetTuple1(i);
    }
  }

  int startRef = tiltAngles.indexOf(0); // use 0-degree image by default
  if (startRef == -1) {
    startRef = (m_minSliceNum + m_maxSliceNum) / 2;
  }

  QLabel* keyGuide = new QLabel;
  keyGuide->setWordWrap(true);
  keyGuide->setTextFormat(Qt::RichText);
  keyGuide->setText(
    "1. Pick an object, use the arrow keys to minimize the wobble.<br />"
    "2. S moves to the next slice, A returns to the previous slice.<br />"
    "3. Repeat steps 1 and 2.<br />"
    "<i>Note: Must use the same object/point all slices.</i>");
  keyGuide->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  v->addWidget(keyGuide);

  QGridLayout* grid = new QGridLayout;
  int gridrow = 0;
  v->addLayout(grid);
  QLabel* label = new QLabel("Current image:");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  m_currentSlice = new SpinBox;
  m_currentSlice->setValue(startRef + 1);
  m_currentSlice->setRange(m_minSliceNum, m_maxSliceNum);
  m_currentSlice->installEventFilter(this);
  connect(m_currentSlice, SIGNAL(editingFinished()), this,
          SLOT(currentSliceEdited()));
  grid->addWidget(m_currentSlice, gridrow, 1, 1, 1, Qt::AlignLeft);
  label = new QLabel("Shortcut: (A/S)");
  grid->addWidget(label, gridrow, 2, 1, 2, Qt::AlignLeft);

  // Reference image controls
  ++gridrow;
  label = new QLabel("Reference image:");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  m_prevButton = new QRadioButton("Prev");
  m_nextButton = new QRadioButton("Next");
  m_statButton = new QRadioButton("Static");
  m_prevButton->setCheckable(true);
  m_nextButton->setCheckable(true);
  m_statButton->setCheckable(true);
  m_refNum = new QSpinBox;
  m_refNum->setValue(startRef);
  m_refNum->setRange(m_minSliceNum, m_maxSliceNum);
  m_refNum->installEventFilter(this);
  connect(m_refNum, SIGNAL(valueChanged(int)), SLOT(updateReference()));
  grid->addWidget(m_refNum, gridrow, 1, 1, 1, Qt::AlignLeft);
  m_refNum->setEnabled(false);
  connect(m_statButton, SIGNAL(toggled(bool)), m_refNum,
          SLOT(setEnabled(bool)));

  grid->addWidget(m_prevButton, gridrow, 2, 1, 1, Qt::AlignLeft);
  grid->addWidget(m_nextButton, gridrow, 3, 1, 1, Qt::AlignLeft);
  grid->addWidget(m_statButton, gridrow, 4, 1, 1, Qt::AlignLeft);

  m_referenceSliceMode = new QButtonGroup;
  m_referenceSliceMode->addButton(m_prevButton);
  m_referenceSliceMode->addButton(m_nextButton);
  m_referenceSliceMode->addButton(m_statButton);
  m_referenceSliceMode->setExclusive(true);
  m_prevButton->setChecked(true);
  connect(m_referenceSliceMode, SIGNAL(buttonClicked(int)),
          SLOT(updateReference()));

  ++gridrow;
  label = new QLabel("Frame rate (fps):");
  grid->addWidget(label, gridrow, 0, 1, 1, Qt::AlignRight);
  QSpinBox* spin = new QSpinBox;
  spin->setRange(0, 50);
  spin->setValue(5);
  spin->installEventFilter(this);
  connect(spin, SIGNAL(valueChanged(int)), SLOT(setFrameRate(int)));
  grid->addWidget(spin, gridrow, 1, 1, 1, Qt::AlignLeft);

  // Slice offsets
  ++gridrow;
  m_currentSliceOffset =
    new QLabel("Image shift (Shortcut: arrow keys): (0, 0)");
  v->addWidget(m_currentSliceOffset);

  // Add our buttons.
  QHBoxLayout* buttonLayout = new QHBoxLayout;
  buttonLayout->addStretch();
  m_startButton = new QPushButton("Start");
  connect(m_startButton, SIGNAL(clicked()), SLOT(startAlign()));
  buttonLayout->addWidget(m_startButton);
  m_startButton->setEnabled(false);
  m_stopButton = new QPushButton("Stop");
  connect(m_stopButton, SIGNAL(clicked()), SLOT(stopAlign()));
  buttonLayout->addWidget(m_stopButton);
  buttonLayout->addStretch();
  v->addLayout(buttonLayout);

  m_offsetTable = new QTableWidget(this);
  m_offsetTable->verticalHeader()->setVisible(false);
  v->addWidget(m_offsetTable, 2);
  m_offsets.fill(vtkVector2i(0, 0), m_maxSliceNum + 1);

  const QVector<vtkVector2i>& oldOffsets = m_operator->getAlignOffsets();

  m_offsetTable->setRowCount(m_offsets.size());
  m_offsetTable->setColumnCount(4);
  QTableWidgetItem* item = new QTableWidgetItem();
  item->setText("Slice #");
  m_offsetTable->setHorizontalHeaderItem(0, item);
  item = new QTableWidgetItem();
  item->setText("X offset");
  m_offsetTable->setHorizontalHeaderItem(1, item);
  item = new QTableWidgetItem();
  item->setText("Y offset");
  m_offsetTable->setHorizontalHeaderItem(2, item);
  item = new QTableWidgetItem();
  item->setText("Tilt angle");
  m_offsetTable->setHorizontalHeaderItem(3, item);
  for (int i = 0; i < oldOffsets.size(); ++i) {
    m_offsets[i] = oldOffsets[i];
  }

  // show initial current and reference image
  setSlice(m_currentSlice->value());
  updateReference();

  for (int i = 0; i < m_offsets.size(); ++i) {
    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(i));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_offsetTable->setItem(i, 0, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(m_offsets[i][0]));
    m_offsetTable->setItem(i, 1, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(m_offsets[i][1]));
    m_offsetTable->setItem(i, 2, item);

    item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_offsetTable->setItem(i, 3, item);
  }
  m_offsetTable->resizeColumnsToContents();
  m_currentSliceOffset->setText(
    QString("Image shift (Shortcut: arrow keys): (%1, %2)")
      .arg(m_offsets[m_currentSlice->value()][0])
      .arg(m_offsets[m_currentSlice->value()][1]));

  connect(m_timer, SIGNAL(timeout()), SLOT(onTimeout()));
  connect(m_offsetTable, SIGNAL(cellChanged(int, int)),
          SLOT(sliceOffsetEdited(int, int)));
  changeSlice(0);
  m_timer->start(200);
}

AlignWidget::~AlignWidget()
{
  qDeleteAll(m_modes);
  m_modes.clear();
}

bool AlignWidget::eventFilter(QObject* object, QEvent* e)
{
  if (object == m_widget) {
    switch (e->type()) {
      case QEvent::KeyPress:
        widgetKeyPress(static_cast<QKeyEvent*>(e));
        return true;
      case QEvent::KeyRelease:
        return true;
      default:
        return false;
    }
  } else if (qobject_cast<QSpinBox*>(object)) {
    if (e->type() == QEvent::KeyPress) {
      QKeyEvent* ke = static_cast<QKeyEvent*>(e);
      if (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return) {
        e->accept();
        qobject_cast<QWidget*>(object)->clearFocus();
        m_widget->setFocus(Qt::OtherFocusReason);
        return true;
      }
    }
  }
  return false;
}

void AlignWidget::onTimeout()
{
  if (m_modes.length() > 0) {
    m_modes[m_currentMode]->timeout();
  }
  m_widget->GetRenderWindow()->Render();
}

void AlignWidget::changeSlice(int delta)
{
  // Changes currentSlice.
  int min = m_minSliceNum;
  int max = m_maxSliceNum;
  int i = m_currentSlice->value() + delta;

  // This makes stack circular.
  if (i > max) {
    i = min;
  } else if (i < min) {
    i = max;
  }
  m_currentSlice->setValue(i);
  m_offsetTable->setCurrentCell(i, 0);
  QTableWidgetSelectionRange range(i, 0, i, 3);
  m_offsetTable->setRangeSelected(range, true);
  setSlice(i, true);
  updateReference();
}

void AlignWidget::currentSliceEdited()
{
  setSlice(m_currentSlice->value());
  updateReference();
}

void AlignWidget::setSlice(int slice, bool resetInc)
{
  // Does not change currentSlice, display only.
  if (resetInc) {
    m_currentSliceOffset->setText(
      QString("Image shift (Shortcut: arrow keys): (%1, %2)")
        .arg(m_offsets[slice][0])
        .arg(m_offsets[slice][1]));
  }
  applySliceOffset(slice);
}

void AlignWidget::updateReference()
{
  int min = m_minSliceNum;
  int max = m_maxSliceNum;

  int refSlice = 0;

  if (m_prevButton->isChecked()) {
    refSlice = m_currentSlice->value() - 1;
  } else if (m_nextButton->isChecked()) {
    refSlice = m_currentSlice->value() + 1;
  } else if (m_statButton->isChecked()) {
    refSlice = m_refNum->value();
  }

  // This makes the stack circular.
  if (refSlice > max) {
    refSlice = min;
  } else if (refSlice < min) {
    refSlice = max;
  }

  m_refNum->setValue(refSlice);

  m_referenceSlice = refSlice;
  for (int i = 0; i < m_modes.length(); ++i) {
    m_modes[i]->referenceSliceUpdated(m_referenceSlice,
                                      m_offsets[m_referenceSlice]);
  }
  if (m_modes.length() > 0) {
    m_modes[m_currentMode]->update();
  }
  m_widget->GetRenderWindow()->Render();
}

void AlignWidget::setFrameRate(int rate)
{
  if (rate <= 0) {
    rate = 0;
  }
  m_frameRate = rate;
  if (m_frameRate > 0) {
    m_timer->setInterval(1000.0 / m_frameRate);
    if (!m_timer->isActive()) {
      m_timer->start();
    }
  } else {
    stopAlign();
  }
}

void AlignWidget::widgetKeyPress(QKeyEvent* key)
{
  vtkVector2i& offset = m_offsets[m_currentSlice->value()];
  bool updateTable = false;
  switch (key->key()) {
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
  if (updateTable) {
    int sliceNumber = m_currentSlice->value();
    QTableWidgetItem* item = m_offsetTable->item(sliceNumber, 1);
    item->setData(Qt::DisplayRole, QString::number(offset[0]));
    item = m_offsetTable->item(sliceNumber, 2);
    item->setData(Qt::DisplayRole, QString::number(offset[1]));
  }
  applySliceOffset();
}

void AlignWidget::changeMode(int mode)
{
  if (m_modes.length() == 0) {
    return;
  }
  // brightness and contrast are not persistent, so grab them from the former
  // mode and reapply them to the new mode
  double bc[2] = { 0.0, 0.0 };
  m_modes[m_currentMode]->brightnessAndContrast(bc[0], bc[1]);
  m_modes[m_currentMode]->removeFromView(m_renderer.Get());
  m_currentMode = mode;
  m_modes[m_currentMode]->addToView(m_renderer.Get());
  m_modes[m_currentMode]->setBrightnessAndContrast(bc[0], bc[1]);
  m_modes[m_currentMode]->update();
  resetCamera();
}

void AlignWidget::applySliceOffset(int sliceNumber)
{
  vtkVector2i offset(0, 0);
  if (sliceNumber == -1) {
    sliceNumber = m_currentSlice->value();
    offset = m_offsets[m_currentSlice->value()];
    m_currentSliceOffset->setText(
      QString("Image shift (Shortcut: arrow keys): (%1, %2)")
        .arg(offset[0])
        .arg(offset[1]));
  } else {
    offset = m_offsets[sliceNumber];
  }
  for (int i = 0; i < m_modes.length(); ++i) {
    m_modes[i]->currentSliceUpdated(sliceNumber, offset);
  }
  if (m_modes.length() > 0) {
    m_modes[m_currentMode]->update();
  }
  m_widget->GetRenderWindow()->Render();
}

void AlignWidget::startAlign()
{
  // frame rate of 0 means nothing ever changes, which is equivalent to stopping
  if (m_frameRate <= 0) {
    stopAlign();
    return;
  }
  if (!m_timer->isActive()) {
    m_timer->start(1000.0 / m_frameRate);
  }
  m_startButton->setEnabled(false);
  m_stopButton->setEnabled(true);
}

void AlignWidget::stopAlign()
{
  m_timer->stop();
  setSlice(m_currentSlice->value());
  m_startButton->setEnabled(true);
  m_stopButton->setEnabled(false);
  m_timer->stop();
  for (int i = 0; i < m_modes.size(); ++i) {
    m_modes[i]->timerStopped();
  }
}

void AlignWidget::zoomToSelectionStart()
{
  m_widget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
    m_zoomToBoxInteractorStyle.Get());
  m_observerId = m_widget->GetRenderWindow()->GetInteractor()->AddObserver(
    vtkCommand::LeftButtonReleaseEvent, this,
    &AlignWidget::zoomToSelectionFinished);
}

void AlignWidget::zoomToSelectionFinished()
{
  m_widget->GetRenderWindow()->GetInteractor()->RemoveObserver(m_observerId);
  m_widget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
    m_defaultInteractorStyle.Get());
}

void AlignWidget::applyChangesToOperator()
{
  if (m_operator) {
    m_operator->setAlignOffsets(m_offsets);
  }
}

void AlignWidget::resetCamera()
{
  if (m_modes.length() == 0) {
    return;
  }
  vtkCamera* camera = m_renderer->GetActiveCamera();
  double* bounds = m_modes[m_currentMode]->bounds();
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
  if (bounds[1] - bounds[0] < bounds[3] - bounds[2]) {
    parallelScale = 0.5 * (bounds[3] - bounds[2] + 1);
  } else {
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
  QTableWidgetItem* item = m_offsetTable->item(slice, offsetComponent);
  QString str = item->data(Qt::DisplayRole).toString();
  bool ok;
  int offset = str.toInt(&ok);
  if (ok) {
    m_offsets[slice][offsetComponent - 1] = offset;
  }
  if (slice == m_currentSlice->value()) {
    applySliceOffset();
  }
  if (slice == m_referenceSlice) {
    applySliceOffset(m_referenceSlice);
  }
}

void AlignWidget::onPresetClicked()
{
  pqPresetDialog dialog(tomviz::mainWidget(),
                        pqPresetDialog::SHOW_NON_INDEXED_COLORS_ONLY);
  dialog.setCustomizableLoadColors(true);
  dialog.setCustomizableLoadOpacities(true);
  dialog.setCustomizableUsePresetRange(true);
  dialog.setCustomizableLoadAnnotations(false);
  connect(&dialog, SIGNAL(applyPreset(const Json::Value&)),
          SLOT(applyCurrentPreset()));
  dialog.exec();
}

void AlignWidget::applyCurrentPreset()
{
  pqPresetDialog* dialog = qobject_cast<pqPresetDialog*>(sender());
  Q_ASSERT(dialog);

  if (m_modes.length() == 0) {
    return;
  }

  vtkSMProxy* lut = m_modes[m_currentMode]->getLUT();
  if (!lut) {
    return;
  }

  if (dialog->loadColors() || dialog->loadOpacities()) {
    vtkSMProxy* sof =
      vtkSMPropertyHelper(lut, "ScalarOpacityFunction", true).GetAsProxy();
    if (dialog->loadColors()) {
      vtkSMTransferFunctionProxy::ApplyPreset(lut, dialog->currentPreset(),
                                              !dialog->usePresetRange());
    }
    if (dialog->loadOpacities()) {
      if (sof) {
        vtkSMTransferFunctionProxy::ApplyPreset(sof, dialog->currentPreset(),
                                                !dialog->usePresetRange());
      } else {
        qWarning("Cannot load opacities since 'ScalarOpacityFunction' is not "
                 "present.");
      }
    }

    // We need to take extra care to avoid the color and opacity function ranges
    // from straying away from each other. This can happen if only one of them
    // is getting a preset and we're using the preset range.
    if (dialog->usePresetRange() &&
        (dialog->loadColors() ^ dialog->loadOpacities()) && sof) {
      double range[2];
      if (dialog->loadColors() &&
          vtkSMTransferFunctionProxy::GetRange(lut, range)) {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(sof, range);
      } else if (dialog->loadOpacities() &&
                 vtkSMTransferFunctionProxy::GetRange(sof, range)) {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(lut, range);
      }
    }
    renderViews();
    m_widget->GetRenderWindow()->Render();
  }
}
}
