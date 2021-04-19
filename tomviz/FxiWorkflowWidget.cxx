/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "FxiWorkflowWidget.h"
#include "ui_FxiWorkflowWidget.h"

#include "ActiveObjects.h"
#include "ColorMap.h"
#include "DataSource.h"
#include "InternalPythonHelper.h"
#include "Utilities.h"

#include <cmath>

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <vtkCubeAxesActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkNew.h>
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

class FxiWorkflowWidget::Internal : public QObject
{
  Q_OBJECT

public:
  Ui::FxiWorkflowWidget ui;
  QPointer<Operator> op;
  vtkSmartPointer<vtkImageData> image;
  vtkSmartPointer<vtkImageData> rotationImages;
  QList<double> rotations;
  vtkNew<vtkImageSlice> slice;
  vtkNew<vtkImageSliceMapper> mapper;
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkCubeAxesActor> axesActor;
  QString script;
  InternalPythonHelper pythonHelper;
  QPointer<FxiWorkflowWidget> parent;
  QPointer<DataSource> dataSource;
  int sliceNumber = 0;
  QScopedPointer<InternalProgressDialog> progressDialog;
  QFutureWatcher<void> futureWatcher;

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

    vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle;
    interatorStyle->SetRenderOnMouseMove(true);
    ui.sliceView->interactor()->SetInteractorStyle(interatorStyle);
    setRotationData(vtkImageData::New());

    // Use a child data source if one is available so the color map will match
    if (op->childDataSource()) {
      dataSource = op->childDataSource();
    } else if (op->dataSource()) {
      dataSource = op->dataSource();
    } else {
      dataSource = ActiveObjects::instance().activeDataSource();
    }

    auto lut = vtkScalarsToColors::SafeDownCast(
      dataSource->colorMap()->GetClientSideObject());
    if (lut) {
      // Make a deep copy so we can modify it
      auto* newLut = lut->NewInstance();
      newLut->DeepCopy(lut);
      slice->GetProperty()->SetLookupTable(newLut);
      // Decrement the reference count
      newLut->FastDelete();
    }

    for (auto* w : inputWidgets()) {
      w->installEventFilter(this);
    }

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
    render();
  }

  void generateTestImages()
  {
    rotations.clear();

    {
      Python python;
      auto module = pythonHelper.loadModule(script);
      if (!module.isValid()) {
        QString msg = "Failed to load script";
        qCritical() << msg;
        QMessageBox::critical(parent, "Tomviz", msg);
        return;
      }

      auto func = module.findFunction("test_rotations");
      if (!func.isValid()) {
        QString msg = "Failed to find function \"test_rotations\"";
        qCritical() << msg;
        QMessageBox::critical(parent, "Tomviz", msg);
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
        QString msg = "Failed to execute test_rotations()";
        qCritical() << msg;
        QMessageBox::critical(parent, "Tomviz", msg);
        return;
      }

      auto pyImages = result["images"];
      auto* object = Python::VTK::convertToDataObject(pyImages);
      if (!object) {
        QString msg = "No image data was returned from test_rotations()";
        qCritical() << msg;
        QMessageBox::critical(parent, "Tomviz", msg);
        return;
      }

      auto* imageData = vtkImageData::SafeDownCast(object);
      if (!imageData) {
        QString msg = "No image data was returned from test_rotations()";
        qCritical() << msg;
        QMessageBox::critical(parent, "Tomviz", msg);
        return;
      }

      auto centers = result["centers"];
      auto pyRotations = centers.toList();
      if (!pyRotations.isValid() || pyRotations.length() <= 0) {
        QString msg = "No rotations returned from test_rotations()";
        qCritical() << msg;
        QMessageBox::critical(parent, "Tomviz", msg);
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
  }

  void setRotationData(vtkImageData* data)
  {
    rotationImages = data;
    mapper->SetInputData(rotationImages);
    mapper->SetSliceNumber(0);
    mapper->Update();
    rescaleColors();
    setupRenderer();
  }

  void rescaleColors()
  {
    auto* lut = slice->GetProperty()->GetLookupTable();
    if (!lut) {
      return;
    }

    auto* tf = vtkColorTransferFunction::SafeDownCast(lut);
    if (!tf) {
      return;
    }

    auto* newRange = rotationImages->GetScalarRange();
    rescaleLut(tf, newRange[0], newRange[1]);
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
    ui.imageViewSlider->setVisible(enable);
    ui.currentRotationLabel->setVisible(enable);
    ui.currentRotation->setVisible(enable);
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
}

void FxiWorkflowWidget::setScript(const QString& script)
{
  Superclass::setScript(script);
  m_internal->script = script;
}

} // namespace tomviz
