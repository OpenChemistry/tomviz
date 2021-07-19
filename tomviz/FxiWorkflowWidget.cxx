/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "FxiWorkflowWidget.h"
#include "ui_FxiWorkflowWidget.h"

#include "ActiveObjects.h"
#include "ColorMap.h"
#include "DataSource.h"
#include "InterfaceBuilder.h"
#include "InternalPythonHelper.h"
#include "OperatorPython.h"
#include "PresetDialog.h"
#include "Utilities.h"

#include <cmath>

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <vtkCallbackCommand.h>
#include <vtkColorTransferFunction.h>
#include <vtkCubeAxesActor.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleImage.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
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

  class FxiWorkflowWidget::Internal : public QObject
{
  Q_OBJECT

public:
  Ui::FxiWorkflowWidget ui;
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
  QString script;
  InternalPythonHelper pythonHelper;
  QPointer<FxiWorkflowWidget> parent;
  QPointer<DataSource> dataSource;
  QPointer<InterfaceBuilder> interfaceBuilder;
  int sliceNumber = 0;
  QScopedPointer<InternalProgressDialog> progressDialog;
  QFutureWatcher<void> futureWatcher;
  bool testRotationsSuccess = false;
  QString testRotationsErrorMessage;
  QStringList additionalParameterNames;

  Internal(Operator* o, vtkSmartPointer<vtkImageData> img, FxiWorkflowWidget* p)
    : op(o), image(img)
  {
    // Must call setupUi() before using p in any way
    ui.setupUi(p);
    setParent(p);
    parent = p;

    readSettings();

    // Keep the axes invisible until the data is displayed
    axesActor->SetVisibility(false);

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

    colorMap = dataSource->colorMap();

    for (auto* w : inputWidgets()) {
      w->installEventFilter(this);
    }

    // This isn't always working in Qt designer, so set it here as well
    ui.colorPresetButton->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites.svg"));

    auto* dims = image->GetDimensions();
    ui.slice->setMaximum(dims[1] - 1);
    ui.sliceStart->setMaximum(dims[1] - 1);
    ui.sliceStop->setMaximum(dims[1]);

    // Get the slice start to default to 0, and the slice stop
    // to default to dims[1], despite whatever settings they read in.
    ui.sliceStart->setValue(0);
    ui.sliceStop->setValue(dims[1]);

    // Indicate what the max is via a tooltip.
    auto toolTip = "Max: " + QString::number(dims[1]);
    ui.sliceStop->setToolTip(toolTip);

    // Hide the additional parameters label unless the user adds some
    ui.additionalParametersLayoutLabel->hide();

    progressDialog.reset(new InternalProgressDialog(parent));

    updateControls();
    setupConnections();
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
  }

  void setupUI(OperatorPython* pythonOp)
  {
    if (!pythonOp) {
      return;
    }

    // If the user added extra parameters, add them here
    auto json = QJsonDocument::fromJson(pythonOp->JSONDescription().toLatin1());
    if (json.isNull() || !json.isObject()) {
      return;
    }

    DataSource* ds = nullptr;
    if (pythonOp->hasChildDataSource()) {
      ds = pythonOp->childDataSource();
    } else {
      ds = qobject_cast<DataSource*>(pythonOp->parent());
    }

    if (!ds) {
      ds = ActiveObjects::instance().activeDataSource();
    }

    QJsonObject root = json.object();

    // Get the parameters for the operator
    QJsonValueRef parametersNode = root["parameters"];
    if (parametersNode.isUndefined() || !parametersNode.isArray()) {
      return;
    }
    auto parameters = parametersNode.toArray();

    // Here is the list of parameters for which we already have widgets
    QStringList knownParameters = {
      "rotation_center", "slice_start", "slice_stop",
    };

    additionalParameterNames.clear();
    int i = 0;
    while (i < parameters.size()) {
      QJsonValueRef parameterNode = parameters[i];
      QJsonObject parameterObject = parameterNode.toObject();
      QJsonValueRef nameValue = parameterObject["name"];
      if (knownParameters.contains(nameValue.toString())) {
        // This parameter is already known. Remove it.
        parameters.removeAt(i);
      } else {
        i += 1;
        additionalParameterNames.append(nameValue.toString());
      }
    }

    if (parameters.isEmpty()) {
      return;
    }

    // If we get to this point, we have some extra parameters.
    // Show the additional parameters label, and add the parameters.
    ui.additionalParametersLayoutLabel->show();
    auto layout = ui.additionalParametersLayout;

    if (interfaceBuilder) {
      interfaceBuilder->deleteLater();
      interfaceBuilder = nullptr;
    }

    interfaceBuilder = new InterfaceBuilder(this, ds);
    interfaceBuilder->setParameterValues(pythonOp->arguments());
    interfaceBuilder->buildParameterInterface(layout, parameters);
  }

  void setAdditionalParameterValues(QMap<QString, QVariant> values)
  {
    if (!interfaceBuilder) {
      return;
    }

    auto parentWidget = ui.additionalParametersLayout->parentWidget();
    interfaceBuilder->setParameterValues(values);
    interfaceBuilder->updateWidgetValues(parentWidget);
  }

  QVariantMap additionalParametersValues()
  {
    if (!interfaceBuilder) {
      return QVariantMap();
    }

    auto parentWidget = ui.additionalParametersLayout->parentWidget();
    return interfaceBuilder->parameterValues(parentWidget);
  }

  void setupRenderer() { tomviz::setupRenderer(renderer, mapper, axesActor); }

  void render() { ui.sliceView->renderWindow()->Render(); }

  void readSettings()
  {
    readReconSettings();
    readTestSettings();
  }

  void readReconSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("FxiWorkflowWidget");
    settings->beginGroup("Recon");
    setRotationCenter(settings->value("rotationCenter", 600).toDouble());
    setSliceStart(settings->value("sliceStart", 0).toInt());
    setSliceStop(settings->value("sliceStop", 1).toInt());
    settings->endGroup();
    settings->endGroup();
  }

  void readTestSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("FxiWorkflowWidget");
    settings->beginGroup("TestSettings");
    ui.start->setValue(settings->value("start", 550).toDouble());
    ui.stop->setValue(settings->value("stop", 650).toDouble());
    ui.steps->setValue(settings->value("steps", 26).toInt());
    ui.slice->setValue(settings->value("sli", 0).toInt());
    settings->endGroup();
    settings->endGroup();
  }

  void writeSettings()
  {
    writeReconSettings();
    writeTestSettings();
  }

  void writeReconSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("FxiWorkflowWidget");
    settings->beginGroup("Recon");
    settings->setValue("rotationCenter", rotationCenter());
    settings->setValue("sliceStart", sliceStart());
    settings->setValue("sliceStop", sliceStop());
    settings->endGroup();
    settings->endGroup();
  }

  void writeTestSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("FxiWorkflowWidget");
    settings->beginGroup("TestSettings");
    settings->setValue("start", ui.start->value());
    settings->setValue("stop", ui.stop->value());
    settings->setValue("steps", ui.steps->value());
    settings->setValue("sli", ui.slice->value());
    settings->endGroup();
    settings->endGroup();
  }

  QList<QWidget*> inputWidgets()
  {
    return { ui.start,          ui.stop,       ui.steps,    ui.slice,
             ui.rotationCenter, ui.sliceStart, ui.sliceStop };
  }

  void startGeneratingTestImages()
  {
    progressDialog->show();
    auto future = QtConcurrent::run(this, &Internal::generateTestImages);
    futureWatcher.setFuture(future);
  }

  void testImagesGenerated()
  {
    updateImageViewSlider();
    if (!testRotationsSuccess) {
      auto msg = testRotationsErrorMessage;
      qCritical() << msg;
      QMessageBox::critical(parent, "Tomviz", msg);
      return;
    }

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
    // Make the axes visible.
    axesActor->SetVisibility(true);

    // Save these settings in case the user wants to use them again...
    writeTestSettings();
    testRotationsSuccess = true;
  }

  void setRotationData(vtkImageData* data)
  {
    rotationImages = data;
    mapper->SetInputData(rotationImages);
    mapper->SetSliceNumber(0);
    mapper->Update();
    setupRenderer();
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

    // It would be nice if we could only write the settings when the
    // widget is accepted, but I don't immediately see an easy way
    // to do that.
    writeSettings();
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

    sliceNumber = 0;
    ui.imageViewSlider->setValue(sliceNumber);

    sliderEdited();
  }

  void sliderEdited()
  {
    sliceNumber = ui.imageViewSlider->value();
    if (sliceNumber < rotations.size()) {
      ui.currentRotation->setValue(rotations[sliceNumber]);

      // For convenience, also set the rotation center for reconstruction
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

  void setSliceStart(int i) { ui.sliceStart->setValue(i); }
  int sliceStart() const { return ui.sliceStart->value(); }

  void setSliceStop(int i) { ui.sliceStop->setValue(i); }
  int sliceStop() const { return ui.sliceStop->value(); }
};

#include "FxiWorkflowWidget.moc"

FxiWorkflowWidget::FxiWorkflowWidget(Operator* op,
                                     vtkSmartPointer<vtkImageData> image,
                                     QWidget* p)
  : CustomPythonOperatorWidget(p)
{
  m_internal.reset(new Internal(op, image, this));
}

FxiWorkflowWidget::~FxiWorkflowWidget() = default;

void FxiWorkflowWidget::getValues(QMap<QString, QVariant>& map)
{
  map.insert("rotation_center", m_internal->rotationCenter());
  map.insert("slice_start", m_internal->sliceStart());
  map.insert("slice_stop", m_internal->sliceStop());

  auto extraParamsMap = m_internal->additionalParametersValues();
  for (auto it = extraParamsMap.cbegin(); it != extraParamsMap.cend(); ++it) {
    map.insert(it.key(), it.value());
  }
}

void FxiWorkflowWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("rotation_center")) {
    m_internal->setRotationCenter(map["rotation_center"].toDouble());
  }
  if (map.contains("slice_start")) {
    m_internal->setSliceStart(map["slice_start"].toInt());
  }
  if (map.contains("slice_stop")) {
    m_internal->setSliceStop(map["slice_stop"].toInt());
  }

  m_internal->setAdditionalParameterValues(map);
}

void FxiWorkflowWidget::setScript(const QString& script)
{
  Superclass::setScript(script);
  m_internal->script = script;
}

void FxiWorkflowWidget::setupUI(OperatorPython* op)
{
  Superclass::setupUI(op);
  m_internal->setupUI(op);
}

} // namespace tomviz
