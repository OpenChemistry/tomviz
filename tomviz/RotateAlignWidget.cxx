/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RotateAlignWidget.h"

#include "ActiveObjects.h"
#include "AddPythonTransformReaction.h"
#include "ColorMap.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "PresetDialog.h"
#include "TomographyReconstruction.h"
#include "TomographyTiltSeries.h"
#include "Utilities.h"

#include <cmath>

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

  int m_projectionNum;
  int m_shiftRotation;
  double m_tiltRotation;
  int m_slice0;
  int m_slice1;
  int m_slice2;
  int m_orientation;

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
    tomviz::setupRenderer(this->mainRenderer, this->mainSliceMapper,
                          this->axesActor);
    tomviz::setupRenderer(this->reconRenderer[0],
                          this->reconSliceMapper[0]);
    tomviz::setupRenderer(this->reconRenderer[1],
                          this->reconSliceMapper[1]);
    tomviz::setupRenderer(this->reconRenderer[2],
                          this->reconSliceMapper[2]);
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
      tform = t;
      this->axisActor->SetUserTransform(t);
    }
    double centerOfRotation[3] = { 0.0, 0.0, 0.0 };
    vtkImageData* imageData = m_image;
    double xTranslate = 0, yTranslate = 0;
    if (imageData) {
      double bounds[6];
      imageData->GetBounds(bounds);
      int dims[3];
      imageData->GetDimensions(dims);
      centerOfRotation[0] = (bounds[0] + bounds[1]) / 2.0;
      centerOfRotation[1] = (bounds[2] + bounds[3]) / 2.0;
      centerOfRotation[2] = (bounds[4] + bounds[5]) / 2.0;
      if (m_orientation == 0) {
        double maxSlice = dims[1];
        yTranslate = (bounds[3] - bounds[2]) * m_shiftRotation / maxSlice;
      }
      else {
        double maxSlice = dims[0];
        xTranslate = (bounds[1] - bounds[0]) * m_shiftRotation / maxSlice;
      }
    }
    tform->Identity();
    tform->Translate(xTranslate, yTranslate, 0);
    tform->Translate(centerOfRotation);

    double rotate = -this->m_tiltRotation;
    if (m_orientation == 1) {
      // Rotate an extra 90 degrees for orientation == 1
      rotate -= 90.0;
    }

    tform->RotateZ(rotate);
    tform->Translate(-centerOfRotation[0], -centerOfRotation[1],
                     -centerOfRotation[2]);
    this->Ui.sliceView->renderWindow()->Render();
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
        shift, m_orientation); // Get a sinogram from tilt series

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

      sliceView[i]->renderWindow()->Render();
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
      double maxSlices;
      if (m_orientation == 0)
        maxSlices = extent[1] - extent[0] + 1;
      else
        maxSlices = extent[3] - extent[2] + 1;

      int slices[3] = { m_slice0, m_slice1, m_slice2 };
      for (int i = 0; i < 3; ++i) {
        double p1[3], p2[3];
        if (m_orientation == 0) {
          p1[0] = bounds[0] + (bounds[1] - bounds[0]) * (slices[i] / maxSlices);
          p2[0] = bounds[0] + (bounds[1] - bounds[0]) * (slices[i] / maxSlices);
          p1[1] = bounds[2];
          p2[1] = bounds[3];
        }
        else {
          p1[0] = bounds[0];
          p2[0] = bounds[1];
          p1[1] = bounds[2] + (bounds[3] - bounds[2]) * (slices[i] / maxSlices);
          p2[1] = bounds[2] + (bounds[3] - bounds[2]) * (slices[i] / maxSlices);
        }
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

  this->Internals->mainSlice->SetMapper(this->Internals->mainSliceMapper);
  this->Internals->reconSlice[0]->SetMapper(
    this->Internals->reconSliceMapper[0]);
  this->Internals->reconSlice[1]->SetMapper(
    this->Internals->reconSliceMapper[1]);
  this->Internals->reconSlice[2]->SetMapper(
    this->Internals->reconSliceMapper[2]);
  this->Internals->mainRenderer->AddViewProp(this->Internals->mainSlice);
  this->Internals->reconRenderer[0]->AddViewProp(
    this->Internals->reconSlice[0]);
  this->Internals->reconRenderer[1]->AddViewProp(
    this->Internals->reconSlice[1]);
  this->Internals->reconRenderer[2]->AddViewProp(
    this->Internals->reconSlice[2]);

  this->Internals->Ui.sliceView->renderWindow()->AddRenderer(
    this->Internals->mainRenderer);
  this->Internals->Ui.sliceView_1->renderWindow()->AddRenderer(
    this->Internals->reconRenderer[0]);
  this->Internals->Ui.sliceView_2->renderWindow()->AddRenderer(
    this->Internals->reconRenderer[1]);
  this->Internals->Ui.sliceView_3->renderWindow()->AddRenderer(
    this->Internals->reconRenderer[2]);

  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyleMain;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle1;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle2;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle3;
  interatorStyleMain->SetRenderOnMouseMove(true);
  interatorStyle1->SetRenderOnMouseMove(true);
  interatorStyle2->SetRenderOnMouseMove(true);
  interatorStyle3->SetRenderOnMouseMove(true);

  this->Internals->Ui.sliceView->interactor()->SetInteractorStyle(
    interatorStyleMain);
  this->Internals->Ui.sliceView_1->interactor()->SetInteractorStyle(
    interatorStyle1);
  this->Internals->Ui.sliceView_2->interactor()->SetInteractorStyle(
    interatorStyle2);
  this->Internals->Ui.sliceView_3->interactor()->SetInteractorStyle(
    interatorStyle3);
  this->Internals->setupCameras();

  this->Internals->rotationAxis->SetPoint1(0, 0, 0);
  this->Internals->rotationAxis->SetPoint1(1, 1, 1);
  this->Internals->rotationAxis->Update();

  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(this->Internals->rotationAxis->GetOutputPort());

  this->Internals->axisActor->SetMapper(mapper);
  this->Internals->axisActor->GetProperty()->SetColor(1, 1, 0); // yellow
  this->Internals->axisActor->GetProperty()->SetLineWidth(2.5);
  this->Internals->mainRenderer->AddActor(this->Internals->axisActor);

  for (int i = 0; i < 3; ++i) {
    this->Internals->reconSliceLine[i]->Update();
    vtkNew<vtkPolyDataMapper> sMapper;
    sMapper->SetInputConnection(
      this->Internals->reconSliceLine[i]->GetOutputPort());
    this->Internals->reconSliceLineActor[i]->SetMapper(sMapper);
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetColor(1, 0, 0);
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetLineWidth(2.0);
    this->Internals->reconSliceLineActor[i]
      ->GetProperty()
      ->SetLineStipplePattern(0xFF00);
    this->Internals->mainRenderer->AddActor(
      this->Internals->reconSliceLineActor[i]);
  }

  QObject::connect(this->Internals->Ui.projection,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &RotateAlignWidget::onProjectionNumberChanged);
  this->Internals->Ui.projection->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_1,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this](int val) { this->onReconSliceChanged(0, val); });
  this->Internals->Ui.spinBox_1->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_2,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this](int val) { this->onReconSliceChanged(1, val); });
  this->Internals->Ui.spinBox_2->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_3,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   [this](int val) { this->onReconSliceChanged(2, val); });
  this->Internals->Ui.spinBox_3->installEventFilter(this);

  QObject::connect(this->Internals->Ui.rotationAxis,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &RotateAlignWidget::onRotationShiftChanged);
  this->Internals->Ui.rotationAxis->installEventFilter(this);

  QObject::connect(this->Internals->Ui.rotationAngle,
                   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                   &RotateAlignWidget::onRotationAngleChanged);
  this->Internals->Ui.rotationAngle->installEventFilter(this);

  QObject::connect(this->Internals->Ui.orientation,
                   QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [this](int val) { this->onOrientationChanged(val); });
  this->Internals->Ui.orientation->installEventFilter(this);

  //  this->connect(this->Internals->Ui.pushButton, SIGNAL(pressed()),
  //                SLOT(onFinalReconButtonPressed()));

  this->Internals->mainSliceMapper->SetInputData(this->Internals->m_image);
  this->Internals->mainSliceMapper->Update();

  // Use a child data source if one is available so the color map will match
  DataSource* ds;
  if (op->childDataSource())
    ds = op->childDataSource();
  else if (op->dataSource())
    ds = op->dataSource();
  else
    ds = ActiveObjects::instance().activeDataSource();


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
  this->Internals->m_orientation = 0;

  int projectionNum = dims[2] / 2;
  this->Internals->m_projectionNum = projectionNum;
  this->Internals->mainSliceMapper->SetSliceNumber(projectionNum);
  this->Internals->mainSliceMapper->Update();

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

void RotateAlignWidget::onProjectionNumberChanged(int val)
{
  if (val == this->Internals->m_projectionNum)
    return;

  this->Internals->m_projectionNum = val;
  this->Internals->mainSliceMapper->SetSliceNumber(val);
  this->Internals->mainSliceMapper->Update();
  this->Internals->Ui.sliceView->renderWindow()->Render();
}

void RotateAlignWidget::onRotationShiftChanged(int val)
{
  if (val == this->Internals->m_shiftRotation)
    return;

  this->Internals->m_shiftRotation = val;
  onRotationAxisChanged();
}

void RotateAlignWidget::onRotationAngleChanged(double val)
{
  if (val == this->Internals->m_tiltRotation)
    return;

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

void RotateAlignWidget::onOrientationChanged(int val)
{
  this->Internals->m_orientation = val;

  int dims[3];
  this->Internals->m_image->GetDimensions(dims);

  this->Internals->m_slice0 = vtkMath::Round(0.25 * dims[val]);
  this->Internals->m_slice1 = vtkMath::Round(0.50 * dims[val]);
  this->Internals->m_slice2 = vtkMath::Round(0.75 * dims[val]);

  this->updateControls();
  this->Internals->updateSliceLines();
  this->Internals->moveRotationAxisLine();
  for (int i = 0; i < 3; ++i)
    this->Internals->updateReconSlice(i);
}

void RotateAlignWidget::onReconSliceChanged(int idx, int val)
{
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

  if (val == *slice)
    return;

  *slice = val;

  this->Internals->updateSliceLines();
  this->Internals->Ui.sliceView->renderWindow()->Render();
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

  PresetDialog dialog(tomviz::mainWidget());
  QObject::connect(&dialog, &PresetDialog::applyPreset, this,
                   slotsArray[reconSlice]);
  dialog.exec();
}

void RotateAlignWidget::changeColorMap(int reconSlice)
{
  auto dialog = qobject_cast<PresetDialog*>(sender());
  Q_ASSERT(dialog);

  auto lut = this->Internals->ReconColorMap[reconSlice];
  if (!lut) {
    return;
  }

  auto current = dialog->presetName();
  ColorMap::instance().applyPreset(current, lut);
  updateWidgets();
}

void RotateAlignWidget::updateWidgets()
{
  this->Internals->Ui.sliceView->renderWindow()->Render();
  this->Internals->Ui.sliceView_1->renderWindow()->Render();
  this->Internals->Ui.sliceView_2->renderWindow()->Render();
  this->Internals->Ui.sliceView_3->renderWindow()->Render();
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

  int dims[3];
  imageData->GetDimensions(dims);

  QString xLabel;
  QString yLabel;
  QString xZeroLabel;
  QString yZeroLabel;

  double projectionValue;
  double projectionRange[2];

  double sliceValues[3];
  double sliceRange[2];

  double rotationShiftValue;
  double rotationShiftRange[2];

  double rotationAngleValue = this->Internals->m_tiltRotation;
  double rotationAngleRange[2] = { -180, 180 };

  double xAxisRange[2];
  double yAxisRange[2];

  int tiltAxis = this->Internals->m_orientation;
  int otherAxis = (tiltAxis == 0 ? 1 : 0);

  rotationShiftValue = this->Internals->m_shiftRotation;
  rotationShiftRange[0] = -dims[otherAxis] / 2;
  rotationShiftRange[1] = dims[otherAxis] / 2;

  sliceValues[0] = this->Internals->m_slice0;
  sliceValues[1] = this->Internals->m_slice1;
  sliceValues[2] = this->Internals->m_slice2;
  sliceRange[0] = 0;
  sliceRange[1] = dims[tiltAxis] - 1;

  xAxisRange[0] = 0;
  xAxisRange[1] = dims[0];
  yAxisRange[0] = 0;
  yAxisRange[1] = dims[1];

  projectionValue = this->Internals->m_projectionNum;
  projectionRange[0] = 0;
  projectionRange[1] = dims[2] - 1;

  this->Internals->Ui.projection->setRange(projectionRange[0],
                                           projectionRange[1]);
  this->Internals->Ui.projection->setValue(projectionValue);

  this->Internals->Ui.spinBox_1->setRange(sliceRange[0], sliceRange[1]);
  this->Internals->Ui.spinBox_2->setRange(sliceRange[0], sliceRange[1]);
  this->Internals->Ui.spinBox_3->setRange(sliceRange[0], sliceRange[1]);
  this->Internals->Ui.spinBox_1->setValue(sliceValues[0]);
  this->Internals->Ui.spinBox_2->setValue(sliceValues[1]);
  this->Internals->Ui.spinBox_3->setValue(sliceValues[2]);

  this->Internals->Ui.rotationAxis->setRange(rotationShiftRange[0],
                                             rotationShiftRange[1]);
  this->Internals->Ui.rotationAxis->setValue(rotationShiftValue);

  this->Internals->Ui.rotationAngle->setRange(rotationAngleRange[0],
                                              rotationAngleRange[1]);
  this->Internals->Ui.rotationAngle->setValue(rotationAngleValue);

  this->Internals->axesActor->SetXAxisRange(xAxisRange);
  this->Internals->axesActor->SetYAxisRange(yAxisRange);
}

void RotateAlignWidget::onFinalReconButtonPressed() {}
} // namespace tomviz
