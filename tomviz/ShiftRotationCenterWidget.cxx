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
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkCubeAxesActor.h>
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

#include <QDebug>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtConcurrent>

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

    // All center-related values are offsets from the image midpoint.
    // 0 means the rotation center is exactly at the midpoint.
    setRotationCenter(0);

    // Default start/stop to +/- 10% of the detector width
    auto delta = dims[0] * 0.1;
    ui.start->setValue(-delta);
    ui.stop->setValue(delta);

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
    connect(ui.projectionNo, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::onProjectionChanged);
    connect(ui.slice, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::onSliceChanged);
    connect(ui.rotationCenter,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Internal::updateCenterLine);
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

    // The existing test rotation results are no longer valid for this slice
    setRotationData(vtkImageData::New());
    rotations.clear();
    updateImageViewSlider();
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
    double lineY = centerY + rotationCenter() * image->GetSpacing()[0];

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
    ui.steps->setValue(settings->value("steps", 26).toInt());
    setAlgorithm(settings->value("algorithm", "mlem").toString());
    ui.numIterations->setValue(settings->value("numIterations", 15).toInt());
    settings->endGroup();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ShiftRotationCenterWidget");
    settings->setValue("steps", ui.steps->value());
    settings->setValue("algorithm", algorithm());
    settings->setValue("numIterations", ui.numIterations->value());
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
    bool iterative = (ui.algorithm->currentText() != "gridrec");
    ui.numIterationsLabel->setVisible(iterative);
    ui.numIterations->setVisible(iterative);
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
