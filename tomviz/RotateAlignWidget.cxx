/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RotateAlignWidget.h"

#include "ActiveObjects.h"
#include "AddPythonTransformReaction.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "TomographyReconstruction.h"
#include "TomographyTiltSeries.h"
#include "Utilities.h"

#include <cmath>

#include <pqPresetDialog.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>

#include <vtkCamera.h>
#include <vtkCubeAxesActor.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkLineSource.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkTransform.h>
#include <vtkTrivialProducer.h>
#include <vtkVector.h>

#include "ui_RotateAlignWidget.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

#define PI 3.14159265359

namespace tomviz {

class RotateAlignWidget::RAWInternal
{
public:
  Ui::RotateAlignWidget Ui;
  vtkSmartPointer<vtkImageData> m_image;
  vtkNew<vtkImageSlice> mainSlice;
  vtkNew<vtkImageData> reconImage[3];
  vtkNew<vtkImageSlice> reconSlice[3];
  vtkNew<vtkCubeAxesActor> axesActor;
  vtkNew<vtkImageSliceMapper> mainSliceMapper;
  vtkNew<vtkImageSliceMapper> reconSliceMapper[3];
  vtkNew<vtkRenderer> mainRenderer;
  vtkNew<vtkRenderer> reconRenderer[3];
  vtkNew<vtkLineSource> rotationAxis;
  vtkNew<vtkActor> axisActor;
  vtkNew<vtkLineSource> reconSliceLine[3];
  vtkNew<vtkActor> reconSliceLineActor[3];
  vtkSmartPointer<vtkSMProxy> ReconColorMap[3];
  bool m_reconSliceDirty[3];
  QTimer m_updateSlicesTimer;

  LengthUnit m_lengthUnit = LengthUnit::pixel;
  int m_projectionNum;
  int m_shiftRotation;
  double m_tiltRotation;
  int m_slice0;
  int m_slice1;
  int m_slice2;

  RAWInternal()
  {
    m_reconSliceDirty[0] = m_reconSliceDirty[1] = m_reconSliceDirty[2] = true;
    m_updateSlicesTimer.setInterval(500);
    m_updateSlicesTimer.setSingleShot(true);
    QObject::connect(&m_updateSlicesTimer, &QTimer::timeout,
                     [this]() { this->updateDirtyReconSlices(); });
  }

  void setupCameras()
  {
    tomviz::setupRenderer(this->mainRenderer.Get(), this->mainSliceMapper.Get(),
                          this->axesActor);
    tomviz::setupRenderer(this->reconRenderer[0].Get(),
                          this->reconSliceMapper[0].Get());
    tomviz::setupRenderer(this->reconRenderer[1].Get(),
                          this->reconSliceMapper[1].Get());
    tomviz::setupRenderer(this->reconRenderer[2].Get(),
                          this->reconSliceMapper[2].Get());
  }

  void setupColorMaps()
  {
    vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();

    vtkNew<vtkSMTransferFunctionManager> tfmgr;
    for (int i = 0; i < 3; ++i) {
      this->ReconColorMap[i] = tfmgr->GetColorTransferFunction(
        QString("RotateAlignWidgetColorMap%1").arg(i).toLatin1().data(), pxm);
    }
  }

  void setupRotationAxisLine()
  {
    vtkImageData* imageData = m_image;
    if (imageData) {
      int extent[6];
      imageData->GetExtent(extent);
      double spacing[3];
      imageData->GetSpacing(spacing);

      double bounds[6];
      imageData->GetBounds(bounds);
      double point1[3], point2[3];
      point1[0] = bounds[0] - (bounds[1] - bounds[0]);
      point2[0] = bounds[1] + (bounds[1] - bounds[0]);
      point1[1] = (bounds[2] + bounds[3]) / 2.0;
      point2[1] = (bounds[2] + bounds[3]) / 2.0;
      point1[2] = bounds[5] + 1;
      point2[2] = bounds[5] + 1;
      this->rotationAxis->SetPoint1(point1);
      this->rotationAxis->SetPoint2(point2);
      this->rotationAxis->Update();
      this->axisActor->GetMapper()->Update();
      this->updateSliceLines();
    }
  }

  void moveRotationAxisLine()
  {
    vtkTransform* tform =
      vtkTransform::SafeDownCast(this->axisActor->GetUserTransform());
    if (!tform) {
      vtkNew<vtkTransform> t;
      t->PreMultiply();
      tform = t.Get();
      this->axisActor->SetUserTransform(t.Get());
    }
    double centerOfRotation[3] = { 0.0, 0.0, 0.0 };
    vtkImageData* imageData = m_image;
    double yTranslate = 0;
    if (imageData) {
      double bounds[6];
      imageData->GetBounds(bounds);
      int dims[3];
      imageData->GetDimensions(dims);
      centerOfRotation[0] = (bounds[0] + bounds[1]) / 2.0;
      centerOfRotation[1] = (bounds[2] + bounds[3]) / 2.0;
      centerOfRotation[2] = (bounds[4] + bounds[5]) / 2.0;
      double maxSlice = dims[1];
      yTranslate = (bounds[3] - bounds[2]) * m_shiftRotation / maxSlice;
    }
    tform->Identity();
    tform->Translate(0, yTranslate, 0);
    tform->Translate(centerOfRotation);
    tform->RotateZ(-this->m_tiltRotation);
    tform->Translate(-centerOfRotation[0], -centerOfRotation[1],
                     -centerOfRotation[2]);
    this->Ui.sliceView->GetRenderWindow()->Render();
  }

  void updateDirtyReconSlices()
  {
    for (int i = 0; i < 3; ++i) {
      if (m_reconSliceDirty[i]) {
        this->updateReconSlice(i);
        m_reconSliceDirty[i] = false;
      }
    }
  }

  void updateReconSlice(int i)
  {
    vtkImageData* imageData = m_image;
    if (imageData) {
      int extent[6];
      imageData->GetExtent(extent);
      int dims[3] = { extent[1] - extent[0] + 1, extent[3] - extent[2] + 1,
                      extent[5] - extent[4] + 1 };

      int sliceNumbers[] = { m_slice0, m_slice1, m_slice2 };
      int sliceNum = sliceNumbers[i];

      int Nray = 256; // Size of 2D reconstruction. Fixed for all tilt series
      std::vector<float> sinogram(Nray * dims[2]);
      // Approximate in-plance rotation as a shift in y-direction
      double shift =
        this->m_shiftRotation +
        sin(-this->m_tiltRotation * PI / 180) * (sliceNum - dims[0] / 2);

      TomographyTiltSeries::getSinogram(
        imageData, sliceNum, &sinogram[0], Nray,
        shift); // Get a sinogram from tilt series
      this->reconImage[i]->SetExtent(0, Nray - 1, 0, Nray - 1, 0, 0);
      this->reconImage[i]->AllocateScalars(VTK_FLOAT, 1);
      vtkDataArray* reconArray =
        this->reconImage[i]->GetPointData()->GetScalars();
      float* reconPtr = static_cast<float*>(reconArray->GetVoidPointer(0));

      vtkDataArray* tiltAnglesArray =
        imageData->GetFieldData()->GetArray("tilt_angles");
      double* tiltAngles =
        static_cast<double*>(tiltAnglesArray->GetVoidPointer(0));

      TomographyReconstruction::unweightedBackProjection2(
        &sinogram[0], tiltAngles, reconPtr, dims[2], Nray);
      this->reconSliceMapper[i]->SetInputData(this->reconImage[i].GetPointer());
      this->reconSliceMapper[i]->SetSliceNumber(0);
      this->reconSliceMapper[i]->Update();

      double range[2];
      reconArray->GetRange(range);
      vtkSMTransferFunctionProxy::RescaleTransferFunction(
        this->ReconColorMap[i], range);
      this->reconSlice[i]->GetProperty()->SetLookupTable(
        vtkScalarsToColors::SafeDownCast(
          this->ReconColorMap[i]->GetClientSideObject()));

      tomviz::QVTKGLWidget* sliceView[] = { this->Ui.sliceView_1,
                                            this->Ui.sliceView_2,
                                            this->Ui.sliceView_3 };

      sliceView[i]->GetRenderWindow()->Render();
    }
  }

  void updateSliceLines()
  {
    vtkImageData* imageData = m_image;
    if (imageData) {
      double bounds[6];
      imageData->GetBounds(bounds);
      int extent[6];
      imageData->GetExtent(extent);
      double maxSlices = extent[1] - extent[0] + 1;
      int slices[3] = { m_slice0, m_slice1, m_slice2 };
      for (int i = 0; i < 3; ++i) {
        double p1[3], p2[3];
        p1[0] = bounds[0] + (bounds[1] - bounds[0]) * (slices[i] / maxSlices);
        p2[0] = bounds[0] + (bounds[1] - bounds[0]) * (slices[i] / maxSlices);
        p1[1] = bounds[2];
        p2[1] = bounds[3];
        p1[2] = bounds[5];
        p2[2] = bounds[5];
        this->reconSliceLine[i]->SetPoint1(p1);
        this->reconSliceLine[i]->SetPoint2(p2);
        this->reconSliceLine[i]->Update();
        this->reconSliceLineActor[i]->GetMapper()->Update();
      }
    }
  }
};

RotateAlignWidget::RotateAlignWidget(Operator* op,
                                     vtkSmartPointer<vtkImageData> image,
                                     QWidget* p)
  : CustomPythonOperatorWidget(p), Internals(new RAWInternal)
{
  this->Internals->m_image = image;
  this->Internals->Ui.setupUi(this);

  this->Internals->setupColorMaps();
  QIcon setColorMapIcon(":/pqWidgets/Icons/pqFavorites16.png");
  this->Internals->Ui.colorMapButton_1->setIcon(setColorMapIcon);
  this->Internals->Ui.colorMapButton_2->setIcon(setColorMapIcon);
  this->Internals->Ui.colorMapButton_3->setIcon(setColorMapIcon);
  this->connect(this->Internals->Ui.colorMapButton_1, SIGNAL(clicked()), this,
                SLOT(showChangeColorMapDialog0()));
  this->connect(this->Internals->Ui.colorMapButton_2, SIGNAL(clicked()), this,
                SLOT(showChangeColorMapDialog1()));
  this->connect(this->Internals->Ui.colorMapButton_3, SIGNAL(clicked()), this,
                SLOT(showChangeColorMapDialog2()));

  this->Internals->mainSlice->SetMapper(this->Internals->mainSliceMapper.Get());
  this->Internals->reconSlice[0]->SetMapper(
    this->Internals->reconSliceMapper[0].Get());
  this->Internals->reconSlice[1]->SetMapper(
    this->Internals->reconSliceMapper[1].Get());
  this->Internals->reconSlice[2]->SetMapper(
    this->Internals->reconSliceMapper[2].Get());
  this->Internals->mainRenderer->AddViewProp(this->Internals->mainSlice.Get());
  this->Internals->reconRenderer[0]->AddViewProp(
    this->Internals->reconSlice[0].Get());
  this->Internals->reconRenderer[1]->AddViewProp(
    this->Internals->reconSlice[1].Get());
  this->Internals->reconRenderer[2]->AddViewProp(
    this->Internals->reconSlice[2].Get());

  this->Internals->Ui.sliceView->GetRenderWindow()->AddRenderer(
    this->Internals->mainRenderer.Get());
  this->Internals->Ui.sliceView_1->GetRenderWindow()->AddRenderer(
    this->Internals->reconRenderer[0].Get());
  this->Internals->Ui.sliceView_2->GetRenderWindow()->AddRenderer(
    this->Internals->reconRenderer[1].Get());
  this->Internals->Ui.sliceView_3->GetRenderWindow()->AddRenderer(
    this->Internals->reconRenderer[2].Get());

  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyleMain;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle1;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle2;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle3;
  interatorStyleMain->SetRenderOnMouseMove(true);
  interatorStyle1->SetRenderOnMouseMove(true);
  interatorStyle2->SetRenderOnMouseMove(true);
  interatorStyle3->SetRenderOnMouseMove(true);

  this->Internals->Ui.sliceView->GetInteractor()->SetInteractorStyle(
    interatorStyleMain.Get());
  this->Internals->Ui.sliceView_1->GetInteractor()->SetInteractorStyle(
    interatorStyle1.Get());
  this->Internals->Ui.sliceView_2->GetInteractor()->SetInteractorStyle(
    interatorStyle2.Get());
  this->Internals->Ui.sliceView_3->GetInteractor()->SetInteractorStyle(
    interatorStyle3.Get());
  this->Internals->setupCameras();

  this->Internals->rotationAxis->SetPoint1(0, 0, 0);
  this->Internals->rotationAxis->SetPoint1(1, 1, 1);
  this->Internals->rotationAxis->Update();

  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(this->Internals->rotationAxis->GetOutputPort());

  this->Internals->axisActor->SetMapper(mapper.Get());
  this->Internals->axisActor->GetProperty()->SetColor(1, 1, 0); // yellow
  this->Internals->axisActor->GetProperty()->SetLineWidth(2.5);
  this->Internals->mainRenderer->AddActor(this->Internals->axisActor.Get());

  for (int i = 0; i < 3; ++i) {
    this->Internals->reconSliceLine[i]->Update();
    vtkNew<vtkPolyDataMapper> sMapper;
    sMapper->SetInputConnection(
      this->Internals->reconSliceLine[i]->GetOutputPort());
    this->Internals->reconSliceLineActor[i]->SetMapper(sMapper.Get());
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetColor(1, 0, 0);
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetLineWidth(2.0);
    this->Internals->reconSliceLineActor[i]
      ->GetProperty()
      ->SetLineStipplePattern(0xFF00);
    this->Internals->mainRenderer->AddActor(
      this->Internals->reconSliceLineActor[i].Get());
  }

  this->Internals->Ui.lengthUnit->addItem("Pixel");
  this->Internals->Ui.lengthUnit->addItem("Physical");
  this->Internals->Ui.lengthUnit->setCurrentIndex(0);

  QObject::connect(this->Internals->Ui.lengthUnit,
                   QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   &RotateAlignWidget::onLengthUnitChanged);
  QObject::connect(this->Internals->Ui.projection,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &RotateAlignWidget::onProjectionNumberChanged);
  this->Internals->Ui.projection->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_1,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this](double val) { this->onReconSliceChanged(0, val); });
  this->Internals->Ui.spinBox_1->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_2,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this](double val) { this->onReconSliceChanged(1, val); });
  this->Internals->Ui.spinBox_2->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_3,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this](double val) { this->onReconSliceChanged(2, val); });
  this->Internals->Ui.spinBox_3->installEventFilter(this);

  QObject::connect(this->Internals->Ui.rotationAxis,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &RotateAlignWidget::onRotationShiftChanged);
  this->Internals->Ui.rotationAxis->installEventFilter(this);

  QObject::connect(this->Internals->Ui.rotationAngle,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &RotateAlignWidget::onRotationAngleChanged);
  this->Internals->Ui.rotationAngle->installEventFilter(this);

  //  this->connect(this->Internals->Ui.pushButton, SIGNAL(pressed()),
  //                SLOT(onFinalReconButtonPressed()));

  this->Internals->mainSliceMapper->SetInputData(this->Internals->m_image);
  this->Internals->mainSliceMapper->Update();

  DataSource* ds = op->dataSource();
  if (!ds) {
    ds = ActiveObjects::instance().activeDataSource();
  }

  vtkScalarsToColors* lut =
    vtkScalarsToColors::SafeDownCast(ds->colorMap()->GetClientSideObject());
  if (lut) {
    this->Internals->mainSlice->GetProperty()->SetLookupTable(lut);
    for (int i = 0; i < 3; ++i) {
      this->Internals->ReconColorMap[i]->Copy(ds->colorMap());
      this->Internals->ReconColorMap[i]->UpdateVTKObjects();
    }
  }
  vtkImageData* imageData = this->Internals->m_image;
  int dims[3];
  imageData->GetDimensions(dims);

  this->Internals->m_slice0 = vtkMath::Round(0.25 * dims[0]);
  this->Internals->m_slice1 = vtkMath::Round(0.50 * dims[0]);
  this->Internals->m_slice2 = vtkMath::Round(0.75 * dims[0]);

  this->Internals->m_projectionNum = dims[2] / 2;
  this->Internals->m_shiftRotation = 0;
  this->Internals->m_tiltRotation = 0;

  updateControls();

  // We have to do this here since we need the output to exist so the camera
  // can be initialized below
  this->Internals->updateReconSlice(0);
  this->Internals->updateReconSlice(1);
  this->Internals->updateReconSlice(2);

  this->Internals->setupCameras();
  this->Internals->setupRotationAxisLine();

  updateWidgets();
}

CustomPythonOperatorWidget* RotateAlignWidget::New(
  QWidget* p, Operator* op, vtkSmartPointer<vtkImageData> data)
{
  return new RotateAlignWidget(op, data, p);
}

RotateAlignWidget::~RotateAlignWidget() {}

void RotateAlignWidget::getValues(QMap<QString, QVariant>& map)
{
  QList<QVariant> value;
  value << 0 << -this->Internals->m_shiftRotation << 0;
  map.insert("SHIFT", value);
  map.insert("rotation_angle", this->Internals->m_tiltRotation);
}

void RotateAlignWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("SHIFT")) {
    auto shift = map["SHIFT"];
    onRotationShiftChanged(-shift.toList()[1].toInt());
  }
  if (map.contains("rotation_angle")) {
    auto rotation = map["rotation_angle"];
    onRotationAngleChanged(rotation.toDouble());
  }
  updateControls();
}

bool RotateAlignWidget::eventFilter(QObject* o, QEvent* e)
{
  if (o == this->Internals->Ui.rotationAngle ||
      o == this->Internals->Ui.rotationAxis ||
      o == this->Internals->Ui.projection ||
      o == this->Internals->Ui.spinBox_1 ||
      o == this->Internals->Ui.spinBox_2 ||
      o == this->Internals->Ui.spinBox_3) {
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

void RotateAlignWidget::onLengthUnitChanged(int val)
{
  LengthUnit lengthUnit;
  switch (val) {
    case 0: {
      lengthUnit = LengthUnit::pixel;
      break;
    }
    case 1: {
      lengthUnit = LengthUnit::physical;
      break;
    }
    default: {
      lengthUnit = LengthUnit::pixel;
      break;
    }
  }

  this->Internals->m_lengthUnit = lengthUnit;
  updateControls();
  updateWidgets();
}

void RotateAlignWidget::onProjectionNumberChanged(double val)
{
  vtkImageData* imageData = this->Internals->m_image;

  int extent[6];
  imageData->GetExtent(extent);
  double spacing[3];
  imageData->GetSpacing(spacing);
  int dims[3];
  imageData->GetDimensions(dims);

  int newVal;
  if (this->Internals->m_lengthUnit == LengthUnit::pixel) {
    newVal = val;
  } else {
    newVal = val / spacing[2] - extent[4];
  }

  if (newVal == this->Internals->m_projectionNum) {
    return;
  }

  this->Internals->m_projectionNum = newVal;
  this->Internals->mainSliceMapper->SetSliceNumber(newVal);
  this->Internals->mainSliceMapper->Update();
  this->Internals->Ui.sliceView->GetRenderWindow()->Render();
}

void RotateAlignWidget::onRotationShiftChanged(double val)
{
  vtkImageData* imageData = this->Internals->m_image;

  int extent[6];
  imageData->GetExtent(extent);
  double spacing[3];
  imageData->GetSpacing(spacing);
  int dims[3];
  imageData->GetDimensions(dims);

  int newVal;
  if (this->Internals->m_lengthUnit == LengthUnit::pixel) {
    newVal = val;
  } else {
    newVal = val / spacing[1];
  }

  if (newVal == this->Internals->m_shiftRotation) {
    return;
  }

  this->Internals->m_shiftRotation = newVal;
  onRotationAxisChanged();
}

void RotateAlignWidget::onRotationAngleChanged(double val)
{
  if (val == this->Internals->m_tiltRotation) {
    return;
  }

  this->Internals->m_tiltRotation = val;
  onRotationAxisChanged();
}

void RotateAlignWidget::onRotationAxisChanged()
{
  this->Internals->moveRotationAxisLine();
  // Update recon windows
  this->Internals->m_reconSliceDirty[0] = true;
  this->Internals->m_reconSliceDirty[1] = true;
  this->Internals->m_reconSliceDirty[2] = true;
  this->Internals->m_updateSlicesTimer.start();
}

void RotateAlignWidget::onReconSliceChanged(int idx, double val)
{
  vtkImageData* imageData = this->Internals->m_image;

  int extent[6];
  imageData->GetExtent(extent);
  double spacing[3];
  imageData->GetSpacing(spacing);
  int dims[3];
  imageData->GetDimensions(dims);

  int* slice;
  switch (idx) {
    case 0: {
      slice = &this->Internals->m_slice0;
      break;
    }
    case 1: {
      slice = &this->Internals->m_slice1;
      break;
    }
    case 2: {
      slice = &this->Internals->m_slice2;
      break;
    }
    default: {
      return;
    }
  }

  int newVal;
  if (this->Internals->m_lengthUnit == LengthUnit::pixel) {
    newVal = val;
  } else {
    newVal = val / spacing[0] - extent[0];
  }

  if (newVal == *slice) {
    return;
  }

  *slice = newVal;

  this->Internals->updateSliceLines();
  this->Internals->Ui.sliceView->GetRenderWindow()->Render();
  this->Internals->m_reconSliceDirty[idx] = true;
  this->Internals->m_updateSlicesTimer.start();
}

namespace {
template <typename T>
std::array<T, 3> make_array(std::initializer_list<T> list)
{
  auto array = std::array<T, 3>();
  int i = 0;
  foreach (T t, list) {
    array[i++] = t;
  }
  return array;
}
} // namespace

void RotateAlignWidget::showChangeColorMapDialog(int reconSlice)
{
  auto slotsArray = make_array({ &RotateAlignWidget::changeColorMap0,
                                 &RotateAlignWidget::changeColorMap1,
                                 &RotateAlignWidget::changeColorMap2 });
  pqPresetDialog dialog(tomviz::mainWidget(),
                        pqPresetDialog::SHOW_NON_INDEXED_COLORS_ONLY);
  dialog.setCustomizableLoadColors(true);
  dialog.setCustomizableLoadOpacities(true);
  dialog.setCustomizableUsePresetRange(true);
  dialog.setCustomizableLoadAnnotations(false);
  this->connect(&dialog, &pqPresetDialog::applyPreset, this,
                slotsArray[reconSlice]);
  dialog.exec();
}

void RotateAlignWidget::changeColorMap(int reconSlice)
{
  pqPresetDialog* dialog = qobject_cast<pqPresetDialog*>(this->sender());
  Q_ASSERT(dialog);

  vtkSMProxy* lut = this->Internals->ReconColorMap[reconSlice];
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
    this->updateWidgets();
  }
}

void RotateAlignWidget::updateWidgets()
{
  this->Internals->Ui.sliceView->GetRenderWindow()->Render();
  this->Internals->Ui.sliceView_1->GetRenderWindow()->Render();
  this->Internals->Ui.sliceView_2->GetRenderWindow()->Render();
  this->Internals->Ui.sliceView_3->GetRenderWindow()->Render();
}

void RotateAlignWidget::updateControls()
{
  QSignalBlocker blockers[] = {
    QSignalBlocker(this->Internals->Ui.projection),
    QSignalBlocker(this->Internals->Ui.spinBox_1),
    QSignalBlocker(this->Internals->Ui.spinBox_2),
    QSignalBlocker(this->Internals->Ui.spinBox_3),
    QSignalBlocker(this->Internals->Ui.rotationAxis),
    QSignalBlocker(this->Internals->Ui.rotationAngle)
  };

  vtkImageData* imageData = this->Internals->m_image;

  int extent[6];
  imageData->GetExtent(extent);
  double spacing[3];
  imageData->GetSpacing(spacing);
  int dims[3];
  imageData->GetDimensions(dims);

  QString xLabel;
  QString yLabel;
  QString xZeroLabel;
  QString yZeroLabel;

  double projectionValue;
  double projectionRange[2];
  double projectionStep;
  int projectionDecimals;

  double sliceValues[3];
  double sliceRange[2];
  double sliceStep;
  int sliceDecimals;

  double rotationShiftValue;
  double rotationShiftRange[2];
  double rotationShiftStep;
  int rotationShiftDecimals;

  double rotationAngleValue = this->Internals->m_tiltRotation;
  double rotationAngleRange[2] = { -180, 180 };
  double rotationAngleStep = 0.5;
  int rotationAngleDecimals = 2;

  double xAxisRange[2];
  double yAxisRange[2];

  if (this->Internals->m_lengthUnit == LengthUnit::physical) {
    projectionValue =
      (extent[4] + this->Internals->m_projectionNum) * spacing[2];
    projectionRange[0] = extent[4] * spacing[2];
    projectionRange[1] = extent[5] * spacing[2];
    projectionStep = spacing[2];
    projectionDecimals = std::max(0, int(std::ceil(-log10(spacing[2]))));

    rotationShiftValue = this->Internals->m_shiftRotation * spacing[1];
    rotationShiftRange[0] = -(dims[1] / 2) * spacing[1];
    rotationShiftRange[1] = (dims[1] / 2) * spacing[1];
    rotationShiftStep = spacing[1];
    rotationShiftDecimals = std::max(0, int(std::ceil(-log10(spacing[1]))));

    sliceValues[0] = (extent[0] + this->Internals->m_slice0) * spacing[0];
    sliceValues[1] = (extent[0] + this->Internals->m_slice1) * spacing[0];
    sliceValues[2] = (extent[0] + this->Internals->m_slice2) * spacing[0];
    sliceRange[0] = extent[0] * spacing[0];
    sliceRange[1] = extent[1] * spacing[0];
    sliceStep = spacing[0];
    sliceDecimals = std::max(0, int(std::ceil(-log10(spacing[0]))));

    xAxisRange[0] = extent[0] * spacing[0];
    xAxisRange[1] = extent[1] * spacing[0];
    yAxisRange[0] = extent[2] * spacing[1];
    yAxisRange[1] = extent[3] * spacing[1];
  } else {
    projectionValue = this->Internals->m_projectionNum;
    projectionRange[0] = 0;
    projectionRange[1] = dims[2] - 1;
    projectionStep = 1;
    projectionDecimals = 0;

    rotationShiftValue = this->Internals->m_shiftRotation;
    rotationShiftRange[0] = -dims[1] / 2;
    rotationShiftRange[1] = dims[1] / 2;
    rotationShiftStep = 1;
    rotationShiftDecimals = 0;

    sliceValues[0] = this->Internals->m_slice0;
    sliceValues[1] = this->Internals->m_slice1;
    sliceValues[2] = this->Internals->m_slice2;
    sliceRange[0] = 0;
    sliceRange[1] = dims[0] - 1;
    sliceStep = 1;
    sliceDecimals = 0;

    xAxisRange[0] = 0;
    xAxisRange[1] = dims[0];
    yAxisRange[0] = 0;
    yAxisRange[1] = dims[1];
  }

  this->Internals->Ui.projection->setRange(projectionRange[0],
                                           projectionRange[1]);
  this->Internals->Ui.projection->setSingleStep(projectionStep);
  this->Internals->Ui.projection->setDecimals(projectionDecimals);
  this->Internals->Ui.projection->setValue(projectionValue);

  this->Internals->Ui.spinBox_1->setRange(sliceRange[0], sliceRange[1]);
  this->Internals->Ui.spinBox_2->setRange(sliceRange[0], sliceRange[1]);
  this->Internals->Ui.spinBox_3->setRange(sliceRange[0], sliceRange[1]);
  this->Internals->Ui.spinBox_1->setSingleStep(sliceStep);
  this->Internals->Ui.spinBox_2->setSingleStep(sliceStep);
  this->Internals->Ui.spinBox_3->setSingleStep(sliceStep);
  this->Internals->Ui.spinBox_1->setDecimals(sliceDecimals);
  this->Internals->Ui.spinBox_2->setDecimals(sliceDecimals);
  this->Internals->Ui.spinBox_3->setDecimals(sliceDecimals);
  this->Internals->Ui.spinBox_1->setValue(sliceValues[0]);
  this->Internals->Ui.spinBox_2->setValue(sliceValues[1]);
  this->Internals->Ui.spinBox_3->setValue(sliceValues[2]);

  this->Internals->Ui.rotationAxis->setRange(rotationShiftRange[0],
                                             rotationShiftRange[1]);
  this->Internals->Ui.rotationAxis->setSingleStep(rotationShiftStep);
  this->Internals->Ui.rotationAxis->setDecimals(rotationShiftDecimals);
  this->Internals->Ui.rotationAxis->setValue(rotationShiftValue);

  this->Internals->Ui.rotationAngle->setRange(rotationAngleRange[0],
                                              rotationAngleRange[1]);
  this->Internals->Ui.rotationAngle->setSingleStep(rotationAngleStep);
  this->Internals->Ui.rotationAngle->setDecimals(rotationAngleDecimals);
  this->Internals->Ui.rotationAngle->setValue(rotationAngleValue);

  this->Internals->axesActor->SetXAxisRange(xAxisRange);
  this->Internals->axesActor->SetYAxisRange(yAxisRange);
}

void RotateAlignWidget::onFinalReconButtonPressed() {}
} // namespace tomviz
