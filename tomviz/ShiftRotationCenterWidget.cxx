/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ShiftRotationCenterWidget.h"
#include "ui_ShiftRotationCenterWidget.h"

#include "ActiveObjects.h"
#include "ColorMap.h"
#include "DataSource.h"
#include "InternalPythonHelper.h"
#include "PresetDialog.h"
#include "Utilities.h"

#include <cmath>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <vtkSMTransferFunctionManager.h>

#include <vtkActor.h>
#include <vtkAxis.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkChartXY.h>
#include <vtkColorTransferFunction.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkCubeAxesActor.h>
#include <vtkFloatArray.h>
#include <vtkPlot.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleImage.h>
#include <vtkLineSource.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkTable.h>

#include <QDebug>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "pqLineEdit.h"

#include <algorithm>

namespace tomviz {

class InternalProgressDialog : public QProgressDialog
{
public:
  InternalProgressDialog(QWidget* parent = nullptr) : QProgressDialog(parent)
  {
    setWindowTitle("Tomviz");
    setLabelText("Generating test images...");
    setMinimum(0);
    setMaximum(0);
    setWindowModality(Qt::WindowModal);

    // No cancel button
    setCancelButton(nullptr);

    // No close button in the corner
    setWindowFlags((windowFlags() | Qt::CustomizeWindowHint) &
                   ~Qt::WindowCloseButtonHint);

    reset();
  }

  void keyPressEvent(QKeyEvent* e) override
  {
    // Do not let the user close the dialog by pressing escape
    if (e->key() == Qt::Key_Escape) {
      return;
    }

    QProgressDialog::keyPressEvent(e);
  }
};

class InteractorStyle : public vtkInteractorStyleImage
{
  // Our customized 2D interactor style class
public:
  static InteractorStyle* New();

  InteractorStyle() { this->SetInteractionModeToImage2D(); }

  void OnLeftButtonDown() override
  {
    // Override this to not do window level events, and instead do panning.
    int x = this->Interactor->GetEventPosition()[0];
    int y = this->Interactor->GetEventPosition()[1];

    this->FindPokedRenderer(x, y);
    if (this->CurrentRenderer == nullptr) {
      return;
    }

    this->GrabFocus(this->EventCallbackCommand);
    if (!this->Interactor->GetShiftKey() &&
        !this->Interactor->GetControlKey()) {
      this->StartPan();
    } else {
      this->Superclass::OnLeftButtonDown();
    }
  }
};

vtkStandardNewMacro(InteractorStyle)

class ShiftRotationCenterWidget::Internal : public QObject
{
  Q_OBJECT

public:
  Ui::ShiftRotationCenterWidget ui;
  QPointer<Operator> op;
  vtkSmartPointer<vtkImageData> image;
  vtkSmartPointer<vtkImageData> rotationImages;
  vtkSmartPointer<vtkSMProxy> colorMap;
  vtkSmartPointer<vtkScalarsToColors> lut;
  QList<double> rotations;
  vtkNew<vtkImageSlice> slice;
  vtkNew<vtkImageSliceMapper> mapper;
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkCubeAxesActor> axesActor;

  // Projection view (top-left) with center line and slice line overlay
  vtkNew<vtkImageSlice> projSlice;
  vtkNew<vtkImageSliceMapper> projMapper;
  vtkNew<vtkRenderer> projRenderer;
  vtkNew<vtkLineSource> centerLine;
  vtkNew<vtkActor> centerLineActor;
  vtkNew<vtkLineSource> sliceLine;
  vtkNew<vtkActor> sliceLineActor;

  // Quality metric line plots (bottom-right, side by side)
  vtkNew<vtkContextView> chartViewQia;
  vtkNew<vtkChartXY> chartQia;
  vtkNew<vtkContextView> chartViewQn;
  vtkNew<vtkChartXY> chartQn;
  vtkNew<vtkTable> indicatorTableQia;
  vtkNew<vtkTable> indicatorTableQn;
  QList<double> qiaValues;
  QList<double> qnValues;

  QString script;
  InternalPythonHelper pythonHelper;
  QPointer<ShiftRotationCenterWidget> parent;
  QPointer<DataSource> dataSource;
  int sliceNumber = 0;
  QScopedPointer<InternalProgressDialog> progressDialog;
  QFutureWatcher<void> futureWatcher;
  bool testRotationsSuccess = false;
  QString testRotationsErrorMessage;

  Internal(Operator* o, vtkSmartPointer<vtkImageData> img,
           ShiftRotationCenterWidget* p)
    : op(o), image(img)
  {
    // Must call setupUi() before using p in any way
    ui.setupUi(p);
    setParent(p);
    parent = p;

    // Make the projectionView expand to fill all available vertical space
    // without hiding the "Test Rotations" button.  Every nested layout
    // level must have stretch set on the expanding item, otherwise Qt
    // treats the extra space as dead space.
    ui.verticalLayout->setStretch(0, 1);    // mainHLayout fills widget
    ui.verticalLayout_3->setStretch(0, 1);  // group box fills right column
    ui.gridLayout_3->setRowStretch(6, 1);   // projectionView fills group

    renderer->SetBackground(1, 1, 1);
    mapper->SetOrientation(0);
    slice->SetMapper(mapper);
    renderer->AddViewProp(slice);
    ui.sliceView->renderWindow()->AddRenderer(renderer);

    vtkNew<InteractorStyle> interactorStyle;
    ui.sliceView->interactor()->SetInteractorStyle(interactorStyle);
    setRotationData(vtkImageData::New());

    // Use a child data source if one is available so the color map will match
    if (op->childDataSource()) {
      dataSource = op->childDataSource();
    } else if (op->dataSource()) {
      dataSource = op->dataSource();
    } else {
      dataSource = ActiveObjects::instance().activeDataSource();
    }

    // Set up the projection view showing one projection image (Z-axis slice).
    // This matches the orientation used by the main slice view in
    // RotateAlignWidget: XY plane, camera looking from +Z.
    projMapper->SetInputData(image);
    projMapper->SetSliceNumber(image->GetDimensions()[2] / 2);
    projMapper->Update();
    projSlice->SetMapper(projMapper);

    // Use the data source's color map for the projection view
    auto* dsLut = vtkScalarsToColors::SafeDownCast(
      dataSource->colorMap()->GetClientSideObject());
    if (dsLut) {
      projSlice->GetProperty()->SetLookupTable(dsLut);
    }

    projRenderer->AddViewProp(projSlice);

    // Set up the yellow center line overlay (vertical)
    vtkNew<vtkPolyDataMapper> lineMapper;
    lineMapper->SetInputConnection(centerLine->GetOutputPort());
    centerLineActor->SetMapper(lineMapper);
    centerLineActor->GetProperty()->SetColor(1, 1, 0);
    centerLineActor->GetProperty()->SetLineWidth(2.0);
    projRenderer->AddActor(centerLineActor);

    // Set up the red slice line overlay (horizontal)
    vtkNew<vtkPolyDataMapper> sliceLineMapper;
    sliceLineMapper->SetInputConnection(sliceLine->GetOutputPort());
    sliceLineActor->SetMapper(sliceLineMapper);
    sliceLineActor->GetProperty()->SetColor(1, 0, 0);
    sliceLineActor->GetProperty()->SetLineWidth(2.0);
    projRenderer->AddActor(sliceLineActor);

    ui.projectionView->renderWindow()->AddRenderer(projRenderer);
    vtkNew<InteractorStyle> projInteractorStyle;
    ui.projectionView->interactor()->SetInteractorStyle(projInteractorStyle);

    // Set up the Qia quality metric line plot
    chartViewQia->SetRenderWindow(ui.plotViewQia->renderWindow());
    chartViewQia->SetInteractor(ui.plotViewQia->interactor());
    chartViewQia->GetScene()->AddItem(chartQia);
    chartQia->SetTitle("Qia");
    chartQia->GetAxis(vtkAxis::BOTTOM)->SetTitle("Center (px)");
    chartQia->GetAxis(vtkAxis::LEFT)->SetTitle("");

    // Set up the Qn quality metric line plot
    chartViewQn->SetRenderWindow(ui.plotViewQn->renderWindow());
    chartViewQn->SetInteractor(ui.plotViewQn->interactor());
    chartViewQn->GetScene()->AddItem(chartQn);
    chartQn->SetTitle("Qn");
    chartQn->GetAxis(vtkAxis::BOTTOM)->SetTitle("Center (px)");
    chartQn->GetAxis(vtkAxis::LEFT)->SetTitle("");

    tomviz::setupRenderer(projRenderer, projMapper, nullptr);
    projRenderer->GetActiveCamera()->SetViewUp(1, 0, 0);

    // Mirror the image left-to-right by placing the camera on the -Z side.
    auto* cam = projRenderer->GetActiveCamera();
    double* pos = cam->GetPosition();
    double* fp = cam->GetFocalPoint();
    cam->SetPosition(pos[0], pos[1], fp[2] - (pos[2] - fp[2]));

    projRenderer->ResetCameraClippingRange();
    updateCenterLine();
    updateSliceLine();

    static unsigned int colorMapCounter = 0;
    ++colorMapCounter;

    auto pxm = ActiveObjects::instance().proxyManager();
    vtkNew<vtkSMTransferFunctionManager> tfmgr;
    colorMap =
      tfmgr->GetColorTransferFunction(
        QString("ShiftRotationCenterWidgetColorMap%1")
          .arg(colorMapCounter)
          .toLatin1()
          .data(),
        pxm);

    // Default to the same colormap as the data source (projection view /
    // main render window). Fall back to grayscale if unavailable.
    auto* dsLutVtk = vtkScalarsToColors::SafeDownCast(
      dataSource->colorMap()->GetClientSideObject());
    auto* colorMapVtk =
      vtkScalarsToColors::SafeDownCast(colorMap->GetClientSideObject());
    if (dsLutVtk && colorMapVtk) {
      colorMapVtk->DeepCopy(dsLutVtk);
    } else {
      setColorMapToGrayscale();
    }

    for (auto* w : inputWidgets()) {
      w->installEventFilter(this);
    }

    // This isn't always working in Qt designer, so set it here as well
    ui.colorPresetButton->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites.svg"));

    auto* dims = image->GetDimensions();

    // All center-related values are offsets from the image midpoint in pixels.
    // 0 means the rotation center is exactly at the midpoint.
    setRotationCenter(0);

    // Default start/stop to +/- 50 pixels
    ui.start->setValue(-50);
    ui.stop->setValue(50);

    // Display image dimensions
    ui.imageDimensionsLabel->setText(
      QString("Image: %1 x %2 x %3 (shift axis: Y = %2 px)")
        .arg(dims[0]).arg(dims[1]).arg(dims[2]));

    // Default projection number to the middle projection
    ui.projectionNo->setMaximum(dims[2] - 1);
    ui.projectionNo->setValue(dims[2] / 2);

    // Default slice to the middle slice (bounded by image height)
    ui.slice->setMaximum(dims[0] - 1);
    ui.slice->setValue(dims[0] / 2);

    // Load saved settings for steps, algorithm, numIterations only
    readSettings();

    // Hide iterations by default (only shown for iterative algorithms)
    updateAlgorithmUI();

    progressDialog.reset(new InternalProgressDialog(parent));

    updateControls();
    setupConnections();

    // Replace the IntSliderWidget's built-in line edit with compact
    // up/down arrow buttons that move the slider by one tick.
    auto* sliderLineEdit = ui.imageViewSlider->findChild<pqLineEdit*>();
    if (sliderLineEdit) {
      sliderLineEdit->hide();
    }
    auto* arrowContainer = new QWidget(ui.imageViewSlider);
    auto* arrowLayout = new QVBoxLayout(arrowContainer);
    arrowLayout->setContentsMargins(0, 0, 0, 0);
    arrowLayout->setSpacing(0);
    auto* upButton = new QToolButton(arrowContainer);
    upButton->setArrowType(Qt::UpArrow);
    upButton->setAutoRepeat(true);
    upButton->setFixedSize(20, 14);
    auto* downButton = new QToolButton(arrowContainer);
    downButton->setArrowType(Qt::DownArrow);
    downButton->setAutoRepeat(true);
    downButton->setFixedSize(20, 14);
    arrowLayout->addWidget(upButton);
    arrowLayout->addWidget(downButton);
    ui.imageViewSlider->layout()->addWidget(arrowContainer);
    connect(upButton, &QToolButton::clicked, this, [this]() {
      int current = ui.imageViewSlider->value();
      if (current < ui.imageViewSlider->maximum()) {
        ui.imageViewSlider->setValue(current + 1);
        sliderEdited();
      }
    });
    connect(downButton, &QToolButton::clicked, this, [this]() {
      int current = ui.imageViewSlider->value();
      if (current > ui.imageViewSlider->minimum()) {
        ui.imageViewSlider->setValue(current - 1);
        sliderEdited();
      }
    });

    // Set up transform source UI visibility
    updateTransformSourceUI();

    // Update line positions now that all values are set
    updateCenterLine();
    updateSliceLine();
  }

  void setupConnections()
  {
    connect(ui.testRotations, &QPushButton::pressed, this,
            &Internal::startGeneratingTestImages);
    connect(ui.imageViewSlider, &IntSliderWidget::valueEdited, this,
            &Internal::sliderEdited);
    connect(&futureWatcher, &QFutureWatcher<void>::finished, this,
            &Internal::testImagesGenerated);
    connect(&futureWatcher, &QFutureWatcher<void>::finished,
            progressDialog.data(), &QProgressDialog::accept);
    connect(ui.colorPresetButton, &QToolButton::clicked, this,
            &Internal::onColorPresetClicked);
    connect(ui.previewMin, &DoubleSliderWidget::valueEdited, this,
            &Internal::onPreviewRangeEdited);
    connect(ui.previewMax, &DoubleSliderWidget::valueEdited, this,
            &Internal::onPreviewRangeEdited);
    connect(ui.algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Internal::updateAlgorithmUI);
    connect(ui.algorithm, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Internal::clearTestResults);
    connect(ui.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &Internal::clearTestResults);
    connect(ui.stop, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &Internal::clearTestResults);
    connect(ui.steps, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::clearTestResults);
    connect(ui.numIterations, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::clearTestResults);
    connect(ui.circMaskRatio,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Internal::clearTestResults);
    connect(ui.projectionNo, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::onProjectionChanged);
    connect(ui.slice, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::onSliceChanged);
    connect(ui.rotationCenter,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Internal::updateCenterLine);
    connect(ui.rotationCenter,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Internal::updateChartIndicator);

    // Transform source UI
    connect(ui.transformSource,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::updateTransformSourceUI);
    connect(ui.transformFileBrowse, &QPushButton::clicked, this, [this]() {
      auto path = QFileDialog::getOpenFileName(
        parent, "Open Transform File", QString(), "NPZ files (*.npz)");
      if (!path.isEmpty()) {
        ui.transformFile->setText(path);
      }
    });
    connect(ui.saveFileBrowse, &QPushButton::clicked, this, [this]() {
      auto path = QFileDialog::getSaveFileName(
        parent, "Save Transform File", QString(), "NPZ files (*.npz)");
      if (!path.isEmpty()) {
        ui.saveFile->setText(path);
      }
    });
  }

  void onProjectionChanged(int val)
  {
    // Update the projection view to show the selected projection
    projMapper->SetSliceNumber(val);
    projMapper->Update();
    updateCenterLine();
    updateSliceLine();

    ui.projectionView->renderWindow()->Render();
  }

  void onSliceChanged(int)
  {
    updateSliceLine();
    clearTestResults();
  }

  void clearTestResults()
  {
    setRotationData(vtkImageData::New());
    rotations.clear();
    qiaValues.clear();
    qnValues.clear();
    updateImageViewSlider();
    updateChart();
    render();
  }

  void updateCenterLine()
  {
    if (!image) {
      return;
    }

    double bounds[6];
    image->GetBounds(bounds);
    double centerY = (bounds[2] + bounds[3]) / 2.0;
    double lineY = centerY + rotationCenter() * image->GetSpacing()[1];

    // Vertical line in the view (constant Y, spanning X), placed just in
    // front of the current Z slice (toward the camera, which looks from -Z).
    double z = bounds[4] - 1;
    double p1[3] = { bounds[0], lineY, z };
    double p2[3] = { bounds[1], lineY, z };
    centerLine->SetPoint1(p1);
    centerLine->SetPoint2(p2);
    centerLine->Update();
    centerLineActor->GetMapper()->Update();

    projRenderer->ResetCameraClippingRange();
    ui.projectionView->renderWindow()->Render();
  }

  void updateSliceLine()
  {
    if (!image) {
      return;
    }

    double bounds[6];
    image->GetBounds(bounds);
    double lineX = bounds[0] + ui.slice->value() * image->GetSpacing()[0];

    // Horizontal red line in the view (constant X, spanning Y), placed just in
    // front of the current Z slice (toward the camera, which looks from -Z).
    double z = bounds[4] - 1;
    double p1[3] = { lineX, bounds[2], z };
    double p2[3] = { lineX, bounds[3], z };
    sliceLine->SetPoint1(p1);
    sliceLine->SetPoint2(p2);
    sliceLine->Update();
    sliceLineActor->GetMapper()->Update();

    projRenderer->ResetCameraClippingRange();
    ui.projectionView->renderWindow()->Render();
  }

  void setupRenderer()
  {
    // Pass nullptr for the axes actor to avoid vtkVectorText
    // "Text is not set" errors caused by degenerate bounds in the
    // slice axis dimension.
    tomviz::setupRenderer(renderer, mapper, nullptr);
  }

  void render() { ui.sliceView->renderWindow()->Render(); }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ShiftRotationCenterWidget");
    ui.steps->setValue(settings->value("steps", 200).toInt());
    setAlgorithm(settings->value("algorithm", "mlem").toString());
    ui.numIterations->setValue(settings->value("numIterations", 15).toInt());
    ui.circMaskRatio->setValue(settings->value("circMaskRatio", 0.8).toDouble());

    // Restore start/stop only if the saved values fit within the current image
    auto halfDim = image->GetDimensions()[1] / 2.0;
    if (settings->contains("start")) {
      auto savedStart = settings->value("start").toDouble();
      if (std::abs(savedStart) <= halfDim) {
        ui.start->setValue(savedStart);
      }
    }
    if (settings->contains("stop")) {
      auto savedStop = settings->value("stop").toDouble();
      if (std::abs(savedStop) <= halfDim) {
        ui.stop->setValue(savedStop);
      }
    }

    settings->endGroup();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ShiftRotationCenterWidget");
    settings->setValue("steps", ui.steps->value());
    settings->setValue("algorithm", algorithm());
    settings->setValue("numIterations", ui.numIterations->value());
    settings->setValue("circMaskRatio", ui.circMaskRatio->value());
    settings->setValue("start", ui.start->value());
    settings->setValue("stop", ui.stop->value());
    settings->endGroup();
  }

  QList<QWidget*> inputWidgets()
  {
    return { ui.start, ui.stop, ui.steps, ui.projectionNo, ui.slice,
             ui.rotationCenter };
  }

  void startGeneratingTestImages()
  {
    progressDialog->show();
    auto future =
      QtConcurrent::run(std::bind(&Internal::generateTestImages, this));
    futureWatcher.setFuture(future);
  }

  void testImagesGenerated()
  {
    if (!testRotationsSuccess) {
      auto msg = testRotationsErrorMessage;
      qCritical() << msg;
      QMessageBox::critical(parent, "Tomviz", msg);
      return;
    }

    // Re-update the mapper on the main thread so bounds are current,
    // then configure the camera. This must happen here (not in
    // setRotationData) because that runs on a background thread.
    mapper->Update();
    setupRenderer();
    renderer->ResetCameraClippingRange();
    updateImageViewSlider();

    if (rotationDataValid()) {
      resetColorRange();
      render();
    }

    updateChart();
  }

  void generateTestImages()
  {
    testRotationsSuccess = false;
    rotations.clear();

    {
      Python python;
      auto module = pythonHelper.loadModule(script);
      if (!module.isValid()) {
        testRotationsErrorMessage = "Failed to load script";
        return;
      }

      auto func = module.findFunction("test_rotations");
      if (!func.isValid()) {
        testRotationsErrorMessage =
          "Failed to find function \"test_rotations\"";
        return;
      }

      Python::Object data = Python::createDataset(image, *dataSource);

      Python::Dict kwargs;
      kwargs.set("dataset", data);
      kwargs.set("start", ui.start->value());
      kwargs.set("stop", ui.stop->value());
      kwargs.set("steps", ui.steps->value());
      kwargs.set("sli", ui.slice->value());
      kwargs.set("algorithm", algorithm());
      kwargs.set("num_iter", ui.numIterations->value());
      kwargs.set("circ_mask_ratio", ui.circMaskRatio->value());

      auto ret = func.call(kwargs);
      auto result = ret.toDict();
      if (!result.isValid()) {
        testRotationsErrorMessage = "Failed to execute test_rotations()";
        return;
      }

      auto pyImages = result["images"];
      auto* object = Python::VTK::convertToDataObject(pyImages);
      if (!object) {
        testRotationsErrorMessage =
          "No image data was returned from test_rotations()";
        return;
      }

      auto* imageData = vtkImageData::SafeDownCast(object);
      if (!imageData) {
        testRotationsErrorMessage =
          "No image data was returned from test_rotations()";
        return;
      }

      auto centers = result["centers"];
      auto pyRotations = centers.toList();
      if (!pyRotations.isValid() || pyRotations.length() <= 0) {
        testRotationsErrorMessage =
          "No rotations returned from test_rotations()";
        return;
      }

      for (int i = 0; i < pyRotations.length(); ++i) {
        rotations.append(pyRotations[i].toDouble());
      }

      qiaValues.clear();
      qnValues.clear();

      auto pyQia = result["qia"];
      auto qiaList = pyQia.toList();
      if (qiaList.isValid()) {
        for (int i = 0; i < qiaList.length(); ++i) {
          qiaValues.append(qiaList[i].toDouble());
        }
      }

      auto pyQn = result["qn"];
      auto qnList = pyQn.toList();
      if (qnList.isValid()) {
        for (int i = 0; i < qnList.length(); ++i) {
          qnValues.append(qnList[i].toDouble());
        }
      }

      setRotationData(imageData);
    }

    // If we made it this far, it was a success
    // Save these settings in case the user wants to use them again...
    writeSettings();
    testRotationsSuccess = true;
  }

  void setRotationData(vtkImageData* data)
  {
    rotationImages = data;
    mapper->SetInputData(rotationImages);
    mapper->SetSliceNumber(0);
    mapper->Update();
  }

  void resetColorRange()
  {
    if (!rotationDataValid()) {
      return;
    }

    auto* range = rotationImages->GetScalarRange();

    auto blocked1 = QSignalBlocker(ui.previewMin);
    auto blocked2 = QSignalBlocker(ui.previewMax);
    ui.previewMin->setMinimum(range[0]);
    ui.previewMin->setMaximum(range[1]);
    ui.previewMin->setValue(range[0]);
    ui.previewMax->setMinimum(range[0]);
    ui.previewMax->setMaximum(range[1]);
    ui.previewMax->setValue(range[1]);

    rescaleColors(range);
  }

  void rescaleColors(double* range)
  {
    // Always perform a deep copy of the original color map
    // If we always modify the control points of the same LUT,
    // the control points will often change and we will end up
    // with a very different LUT than we had originally.
    resetLut();
    if (!lut) {
      return;
    }

    auto* tf = vtkColorTransferFunction::SafeDownCast(lut);
    if (!tf) {
      return;
    }

    rescaleLut(tf, range[0], range[1]);
  }

  void onPreviewRangeEdited()
  {
    if (!rotationDataValid() || !lut) {
      return;
    }

    auto* maxRange = rotationImages->GetScalarRange();

    double range[2];
    range[0] = ui.previewMin->value();
    range[1] = ui.previewMax->value();

    auto minDiff = (maxRange[1] - maxRange[0]) / 1000;
    if (range[1] - range[0] < minDiff) {
      if (sender() == ui.previewMin) {
        // Move the max
        range[1] = range[0] + minDiff;
        auto blocked = QSignalBlocker(ui.previewMax);
        ui.previewMax->setValue(range[1]);
      } else {
        // Move the min
        range[0] = range[1] - minDiff;
        auto blocked = QSignalBlocker(ui.previewMin);
        ui.previewMin->setValue(range[0]);
      }
    }

    rescaleColors(range);
    render();
  }

  void updateControls()
  {
    std::vector<QSignalBlocker> blockers;
    for (auto w : inputWidgets()) {
      blockers.emplace_back(w);
    }

    updateImageViewSlider();
  }

  bool rotationDataValid()
  {
    if (!rotationImages.GetPointer()) {
      return false;
    }

    if (rotations.isEmpty()) {
      return false;
    }

    return true;
  }

  void updateImageViewSlider()
  {
    auto blocked = QSignalBlocker(ui.imageViewSlider);

    bool enable = rotationDataValid();
    ui.testRotationsSettingsGroup->setVisible(enable);
    ui.plotViewQia->setVisible(enable);
    bool iterative = (ui.algorithm->currentText() == "mlem" ||
                      ui.algorithm->currentText() == "ospml_hybrid");
    ui.plotViewQn->setVisible(enable && !iterative);
    if (!enable) {
      return;
    }

    auto* dims = rotationImages->GetDimensions();
    ui.imageViewSlider->setMaximum(dims[0] - 1);

    sliceNumber = dims[0] / 2;
    ui.imageViewSlider->setValue(sliceNumber);

    sliderEdited();
  }

  void sliderEdited()
  {
    sliceNumber = ui.imageViewSlider->value();
    if (sliceNumber < rotations.size()) {
      ui.currentRotation->setValue(rotations[sliceNumber]);

      // For convenience, also set the rotation center
      ui.rotationCenter->setValue(rotations[sliceNumber]);
    } else {
      qCritical() << sliceNumber
                  << "is greater than the rotations size:" << rotations.size();
    }

    mapper->SetSliceNumber(sliceNumber);
    mapper->Update();
    render();
  }

  bool eventFilter(QObject* o, QEvent* e) override
  {
    if (inputWidgets().contains(qobject_cast<QWidget*>(o))) {
      if (e->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter) {
          e->accept();
          qobject_cast<QWidget*>(o)->clearFocus();
          return true;
        }
      }
    }
    return QObject::eventFilter(o, e);
  }

  void resetLut()
  {
    auto dsLut =
      vtkScalarsToColors::SafeDownCast(colorMap->GetClientSideObject());
    if (!dsLut) {
      return;
    }

    // Make a deep copy to modify
    lut = dsLut->NewInstance();
    lut->DeepCopy(dsLut);
    slice->GetProperty()->SetLookupTable(lut);
  }

  void setColorMapToGrayscale()
  {
    ColorMap::instance().applyPreset("Grayscale", colorMap);
  }

  void onColorPresetClicked()
  {
    if (!colorMap) {
      qCritical() << "No color map found!";
      return;
    }

    PresetDialog dialog(tomviz::mainWidget());
    connect(&dialog, &PresetDialog::applyPreset, this, [this, &dialog]() {
      ColorMap::instance().applyPreset(dialog.presetName(), colorMap);
      // Keep the range the same
      double range[2];
      range[0] = ui.previewMin->value();
      range[1] = ui.previewMax->value();
      rescaleColors(range);
      render();
    });
    dialog.exec();
  }

  void setRotationCenter(double center) { ui.rotationCenter->setValue(center); }
  double rotationCenter() const { return ui.rotationCenter->value(); }

  QString algorithm() const { return ui.algorithm->currentText(); }
  void setAlgorithm(const QString& alg)
  {
    int index = ui.algorithm->findText(alg);
    if (index >= 0) {
      ui.algorithm->setCurrentIndex(index);
    }
  }

  void updateAlgorithmUI()
  {
    auto alg = ui.algorithm->currentText();
    bool iterative = (alg == "mlem" || alg == "ospml_hybrid");
    ui.numIterationsLabel->setVisible(iterative);
    ui.numIterations->setVisible(iterative);

    // Qn is only meaningful for non-iterative algorithms (gridrec, fbp)
    // that can produce negative values in the reconstruction.
    ui.plotViewQn->setVisible(!iterative && rotationDataValid());
  }

  void updateTransformSourceUI()
  {
    bool manual = (ui.transformSource->currentIndex() == 0);

    // Manual mode: show rotation center, save file, test rotation controls
    ui.rotationCenterLabel->setVisible(manual);
    ui.rotationCenter->setVisible(manual);
    ui.saveFileLabel->setVisible(manual);
    ui.saveFile->setVisible(manual);
    ui.saveFileBrowse->setVisible(manual);
    ui.testRotationCentersGroup->setVisible(manual);
    ui.testRotationsSettingsGroup->setVisible(manual && rotationDataValid());
    ui.plotViewQia->setVisible(manual && rotationDataValid());
    bool iterAlg = (ui.algorithm->currentText() == "mlem" ||
                    ui.algorithm->currentText() == "ospml_hybrid");
    ui.plotViewQn->setVisible(manual && rotationDataValid() && !iterAlg);
    ui.sliceView->setVisible(manual);

    // Load-from-file mode: show transform file controls
    ui.transformFileLabel->setVisible(!manual);
    ui.transformFile->setVisible(!manual);
    ui.transformFileBrowse->setVisible(!manual);
  }

  void populateChart(vtkChartXY* targetChart,
                     QVTKGLWidget* view, const QList<double>& values,
                     unsigned char r, unsigned char g, unsigned char b)
  {
    targetChart->ClearPlots();

    if (rotations.isEmpty() || values.isEmpty()) {
      view->renderWindow()->Render();
      return;
    }

    int n = std::min(rotations.size(), values.size());

    vtkNew<vtkFloatArray> xArr;
    xArr->SetName("Center");
    xArr->SetNumberOfValues(n);

    vtkNew<vtkFloatArray> yArr;
    yArr->SetName("Value");
    yArr->SetNumberOfValues(n);

    for (int i = 0; i < n; ++i) {
      xArr->SetValue(i, rotations[i]);
      yArr->SetValue(i, values[i]);
    }

    vtkNew<vtkTable> table;
    table->AddColumn(xArr);
    table->AddColumn(yArr);
    table->SetNumberOfRows(n);

    auto* line = targetChart->AddPlot(vtkChart::LINE);
    line->SetInputData(table, 0, 1);
    line->SetColor(r, g, b, 255);
    line->SetWidth(2.0);
  }

  void addIndicator(vtkChartXY* targetChart, vtkTable* indTable,
                    QVTKGLWidget* view, const QList<double>& values)
  {
    if (rotations.isEmpty() || values.isEmpty()) {
      return;
    }

    // Remove old indicator (keep only the data plot at index 0)
    while (targetChart->GetNumberOfPlots() > 1) {
      targetChart->RemovePlot(targetChart->GetNumberOfPlots() - 1);
    }

    double center = rotationCenter();

    double yMin = values[0];
    double yMax = values[0];
    for (auto v : values) {
      if (v < yMin)
        yMin = v;
      if (v > yMax)
        yMax = v;
    }
    double yPadding = (yMax - yMin) * 0.05;

    vtkNew<vtkFloatArray> indX;
    indX->SetName("X");
    indX->SetNumberOfValues(2);
    indX->SetValue(0, center);
    indX->SetValue(1, center);

    vtkNew<vtkFloatArray> indY;
    indY->SetName("Y");
    indY->SetNumberOfValues(2);
    indY->SetValue(0, yMin - yPadding);
    indY->SetValue(1, yMax + yPadding);

    indTable->Initialize();
    indTable->AddColumn(indX);
    indTable->AddColumn(indY);
    indTable->SetNumberOfRows(2);

    auto* indLine = targetChart->AddPlot(vtkChart::LINE);
    indLine->SetInputData(indTable, 0, 1);
    indLine->SetColor(255, 255, 0, 255);
    indLine->SetWidth(2.0);

    view->renderWindow()->Render();
  }

  void updateChart()
  {
    populateChart(chartQia, ui.plotViewQia, qiaValues, 0, 114, 189);
    populateChart(chartQn, ui.plotViewQn, qnValues, 217, 83, 25);
    updateChartIndicator();
  }

  void updateChartIndicator()
  {
    addIndicator(chartQia, indicatorTableQia, ui.plotViewQia, qiaValues);
    addIndicator(chartQn, indicatorTableQn, ui.plotViewQn, qnValues);
  }
};

#include "ShiftRotationCenterWidget.moc"

ShiftRotationCenterWidget::ShiftRotationCenterWidget(
  Operator* op, vtkSmartPointer<vtkImageData> image, QWidget* p)
  : CustomPythonOperatorWidget(p)
{
  m_internal.reset(new Internal(op, image, this));
}

ShiftRotationCenterWidget::~ShiftRotationCenterWidget() = default;

void ShiftRotationCenterWidget::getValues(QVariantMap& map)
{
  map.insert("rotation_center", m_internal->rotationCenter());

  auto sourceIndex = m_internal->ui.transformSource->currentIndex();
  map.insert("transform_source", sourceIndex == 0 ? "manual" : "from_file");
  map.insert("transform_file", m_internal->ui.transformFile->text());
  map.insert("transforms_save_file", m_internal->ui.saveFile->text());
}

void ShiftRotationCenterWidget::setValues(const QVariantMap& map)
{
  if (map.contains("rotation_center")) {
    m_internal->setRotationCenter(map["rotation_center"].toDouble());
  }
  if (map.contains("algorithm")) {
    m_internal->setAlgorithm(map["algorithm"].toString());
  }
  if (map.contains("num_iter")) {
    m_internal->ui.numIterations->setValue(map["num_iter"].toInt());
  }
  if (map.contains("transform_source")) {
    auto source = map["transform_source"].toString();
    m_internal->ui.transformSource->setCurrentIndex(
      source == "from_file" ? 1 : 0);
  }
  if (map.contains("transform_file")) {
    m_internal->ui.transformFile->setText(map["transform_file"].toString());
  }
  if (map.contains("transforms_save_file")) {
    m_internal->ui.saveFile->setText(map["transforms_save_file"].toString());
  }
}

void ShiftRotationCenterWidget::setScript(const QString& script)
{
  Superclass::setScript(script);
  m_internal->script = script;
}

void ShiftRotationCenterWidget::writeSettings()
{
  Superclass::writeSettings();
  m_internal->writeSettings();
}

} // namespace tomviz
