/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RotateAlignWidget.h"

#include "ActiveObjects.h"
#include "ColorMap.h"
#include "PresetDialog.h"
#include "TomographyReconstruction.h"
#include "TomographyTiltSeries.h"
#include "Utilities.h"
#include "pipeline/data/VolumeData.h"

#include <cmath>

#include <pqApplicationCore.h>
#include <pqSettings.h>

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
  vtkNew<vtkImageData> m_summedImage;
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

  bool m_sumProjections = false;
  int m_projectionNum;
  int m_shiftRotation;
  double m_tiltRotation;
  int m_slice0;
  int m_slice1;
  int m_slice2;
  int m_orientation;

  bool m_summedImageInitialized = false;

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
                          nullptr);
    this->mainRenderer->ResetCameraClippingRange();
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
    if (!pxm) {
      return;
    }

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
      } else {
        double maxSlice = dims[0];
        xTranslate = (bounds[1] - bounds[0]) * m_shiftRotation / maxSlice;
      }
    }
    tform->Identity();
    tform->Translate(xTranslate, yTranslate, 0);
    tform->Translate(centerOfRotation);

    double rotate = -this->m_tiltRotation;
    if (m_orientation == 1) {
      // Rotate in the opposite direction for vertical tilt axes
      // This is a quick fix so that the drawn axis matches the drawn
      // images correctly. See #2241.
      rotate *= -1;
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
        imageData, sliceNum, &sinogram[0], Nray, shift,
        m_orientation); // Get a sinogram from tilt series

      this->reconImage[i]->SetExtent(0, Nray - 1, 0, Nray - 1, 0, 0);
      this->reconImage[i]->AllocateScalars(VTK_FLOAT, 1);
      vtkDataArray* reconArray =
        this->reconImage[i]->GetPointData()->GetScalars();
      float* reconPtr = static_cast<float*>(reconArray->GetVoidPointer(0));

      vtkDataArray* tiltAnglesArray =
        imageData->GetFieldData()->GetArray("tilt_angles");
      if (!tiltAnglesArray) {
        return;
      }
      double* tiltAngles =
        static_cast<double*>(tiltAnglesArray->GetVoidPointer(0));

      TomographyReconstruction::unweightedBackProjection2(
        &sinogram[0], tiltAngles, reconPtr, dims[2], Nray);
      this->reconSliceMapper[i]->SetInputData(this->reconImage[i].GetPointer());
      this->reconSliceMapper[i]->SetSliceNumber(0);
      this->reconSliceMapper[i]->Update();

      double range[2] = {DBL_MAX, -DBL_MAX};

      // Get the range of the inscribed circle only
      auto radius = static_cast<double>(Nray) / 2;

      // The max distance for what we will keep is the radius multiplied by
      // some reduction factor in order to exclude edge pixels. The images come
      // out better if we exclude edge pixels since the pixel values tend to
      // drop fast near the edges. So this factor is a magic number.
      auto maxDistance = radius * 0.97;
      for (int j = 0; j < Nray; ++j) {
        for (int k = 0; k < Nray; ++k) {
          auto distance = std::sqrt(std::pow(radius - j, 2) + std::pow(radius - k, 2));
          if (distance > maxDistance) {
            // Not in the inscribed circle (or is an edge pixel). Continue.
            continue;
          }

          auto idx = j * Nray + k;
          auto val = reconArray->GetTuple1(idx);
          if (val < range[0]) {
            range[0] = val;
          }
          if (val > range[1]) {
            range[1] = val;
          }
        }
      }

      if (this->ReconColorMap[i]) {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(
          this->ReconColorMap[i], range);
        this->reconSlice[i]->GetProperty()->SetLookupTable(
          vtkScalarsToColors::SafeDownCast(
            this->ReconColorMap[i]->GetClientSideObject()));
      }

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
        } else {
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

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    m_orientation = settings->value("RotateAlignWidget.orientation",
                                    0).toInt();
    // Only 0 (horizontal) and 1 (vertical) are valid; anything else in the
    // settings file would index dims[] out of range.
    if (m_orientation != 0 && m_orientation != 1) {
      m_orientation = 0;
    }
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->setValue("RotateAlignWidget.orientation", m_orientation);
  }

  void initializeSummedImage()
  {
    if (m_summedImageInitialized) {
      // Already initialized. This data is static and does not change.
      return;
    }

    auto image = m_image;
    auto summedImage = m_summedImage.Get();

    const auto* dims = image->GetDimensions();

    // The summed image will have only one dimension in Z
    summedImage->SetDimensions(dims[0], dims[1], 1);
    summedImage->SetOrigin(image->GetOrigin());
    summedImage->SetSpacing(image->GetSpacing());
    summedImage->AllocateScalars(VTK_DOUBLE, 1);

    auto imageArray = image->GetPointData()->GetScalars();
    auto summedImageArray = summedImage->GetPointData()->GetScalars();

    // Initialize values to zero
    for (int i = 0; i < summedImageArray->GetNumberOfTuples(); ++i) {
      summedImageArray->SetTuple1(i, 0);
    }

    // Now sum the values in the image data
    // Fortran ordering
    for (int k = 0; k < dims[2]; ++k) {
      for (int j = 0; j < dims[1]; ++j) {
        for (int i = 0; i < dims[0]; ++i) {
          auto summedIdx = static_cast<size_t>(j) * dims[0] + i;
          auto imgIdx = (static_cast<size_t>(k) * dims[1] + j) * dims[0] + i;
          auto prevValue = summedImageArray->GetTuple1(summedIdx);
          auto newValue = prevValue + imageArray->GetTuple1(imgIdx);
          summedImageArray->SetTuple1(summedIdx, newValue);
        }
      }
    }

    summedImageArray->Modified();

    // Rescale the data range to the original range
    // We are doing this because I was having issues rescaling the
    // color range to be in the new range. Cosmetically, for the image,
    // rescaling this data to be in the original range should result in
    // exactly the same image as rescaling the color range to be in the
    // new range.
    const auto* newRange = imageArray->GetRange();
    const auto* oldRange = summedImageArray->GetRange();

    const double newSpread = newRange[1] - newRange[0];
    const double oldSpread = oldRange[1] - oldRange[0];
    const double multiplier = newSpread / oldSpread;

    for (int i = 0; i < summedImageArray->GetNumberOfTuples(); ++i) {
      double oldVal = summedImageArray->GetTuple1(i);
      double newVal = (oldVal - oldRange[0]) * multiplier + newRange[0];
      summedImageArray->SetTuple1(i, newVal);
    }

    summedImageArray->Modified();

    m_summedImageInitialized = true;
  }
};

RotateAlignWidget::RotateAlignWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* p)
  : pipeline::CustomPythonNodeWidget(p), Internals(new RAWInternal)
{
  vtkSmartPointer<vtkImageData> image;
  vtkSMProxy* sourceColorMap = nullptr;
  if (auto it = inputs.constFind(QStringLiteral("volume"));
      it != inputs.constEnd()) {
    if (auto vol = it.value().value<pipeline::VolumeDataPtr>();
        vol && vol->isValid()) {
      image = vol->imageData();
      vol->initColorMap();
      sourceColorMap = vol->colorMap();
    }
  }
  this->Internals->m_image = image;
  initUI(sourceColorMap);
}

void RotateAlignWidget::initUI(vtkSMProxy* sourceColorMap)
{
  auto* d = this->Internals.data();
  d->Ui.setupUi(this);
  d->readSettings();
  d->setupColorMaps();

  QIcon setColorMapIcon(":/pqWidgets/Icons/pqFavorites.svg");
  d->Ui.colorMapButton_1->setIcon(setColorMapIcon);
  d->Ui.colorMapButton_2->setIcon(setColorMapIcon);
  d->Ui.colorMapButton_3->setIcon(setColorMapIcon);
  connect(d->Ui.colorMapButton_1, &QToolButton::clicked, this,
          &RotateAlignWidget::showChangeColorMapDialog0);
  connect(d->Ui.colorMapButton_2, &QToolButton::clicked, this,
          &RotateAlignWidget::showChangeColorMapDialog1);
  connect(d->Ui.colorMapButton_3, &QToolButton::clicked, this,
          &RotateAlignWidget::showChangeColorMapDialog2);

  d->mainSlice->SetMapper(d->mainSliceMapper);
  d->reconSlice[0]->SetMapper(d->reconSliceMapper[0]);
  d->reconSlice[1]->SetMapper(d->reconSliceMapper[1]);
  d->reconSlice[2]->SetMapper(d->reconSliceMapper[2]);
  d->mainRenderer->AddViewProp(d->mainSlice);
  d->reconRenderer[0]->AddViewProp(d->reconSlice[0]);
  d->reconRenderer[1]->AddViewProp(d->reconSlice[1]);
  d->reconRenderer[2]->AddViewProp(d->reconSlice[2]);

  d->Ui.sliceView->renderWindow()->AddRenderer(d->mainRenderer);
  d->Ui.sliceView_1->renderWindow()->AddRenderer(d->reconRenderer[0]);
  d->Ui.sliceView_2->renderWindow()->AddRenderer(d->reconRenderer[1]);
  d->Ui.sliceView_3->renderWindow()->AddRenderer(d->reconRenderer[2]);

  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyleMain;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle1;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle2;
  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle3;
  interatorStyleMain->SetRenderOnMouseMove(true);
  interatorStyle1->SetRenderOnMouseMove(true);
  interatorStyle2->SetRenderOnMouseMove(true);
  interatorStyle3->SetRenderOnMouseMove(true);

  d->Ui.sliceView->interactor()->SetInteractorStyle(interatorStyleMain);
  d->Ui.sliceView_1->interactor()->SetInteractorStyle(interatorStyle1);
  d->Ui.sliceView_2->interactor()->SetInteractorStyle(interatorStyle2);
  d->Ui.sliceView_3->interactor()->SetInteractorStyle(interatorStyle3);

  d->rotationAxis->SetPoint1(0, 0, 0);
  d->rotationAxis->SetPoint1(1, 1, 1);
  d->rotationAxis->Update();

  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(d->rotationAxis->GetOutputPort());
  d->axisActor->SetMapper(mapper);
  d->axisActor->GetProperty()->SetColor(1, 1, 0); // yellow
  d->axisActor->GetProperty()->SetLineWidth(2.5);
  d->mainRenderer->AddActor(d->axisActor);

  for (int i = 0; i < 3; ++i) {
    d->reconSliceLine[i]->Update();
    vtkNew<vtkPolyDataMapper> sMapper;
    sMapper->SetInputConnection(d->reconSliceLine[i]->GetOutputPort());
    d->reconSliceLineActor[i]->SetMapper(sMapper);
    d->reconSliceLineActor[i]->GetProperty()->SetColor(1, 0, 0);
    d->reconSliceLineActor[i]->GetProperty()->SetLineWidth(2.0);
    d->reconSliceLineActor[i]->GetProperty()->SetLineStipplePattern(0xFF00);
    d->mainRenderer->AddActor(d->reconSliceLineActor[i]);
  }

  connect(d->Ui.sumProjections, &QCheckBox::toggled, this,
          &RotateAlignWidget::onSumProjectionsToggled);
  connect(d->Ui.projection,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &RotateAlignWidget::onProjectionNumberChanged);
  d->Ui.projection->installEventFilter(this);

  connect(d->Ui.spinBox_1,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](int val) { this->onReconSliceChanged(0, val); });
  d->Ui.spinBox_1->installEventFilter(this);

  connect(d->Ui.spinBox_2,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](int val) { this->onReconSliceChanged(1, val); });
  d->Ui.spinBox_2->installEventFilter(this);

  connect(d->Ui.spinBox_3,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](int val) { this->onReconSliceChanged(2, val); });
  d->Ui.spinBox_3->installEventFilter(this);

  connect(d->Ui.rotationAxis,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &RotateAlignWidget::onRotationShiftChanged);
  d->Ui.rotationAxis->installEventFilter(this);

  connect(d->Ui.rotationAngle,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &RotateAlignWidget::onRotationAngleChanged);
  d->Ui.rotationAngle->installEventFilter(this);

  connect(d->Ui.orientation,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int val) { this->onOrientationChanged(val); });
  d->Ui.orientation->installEventFilter(this);

  if (!d->m_image) {
    return;
  }

  d->mainSliceMapper->SetInputData(d->m_image);
  d->mainSliceMapper->Update();

  // Apply source color map if provided
  if (sourceColorMap) {
    vtkScalarsToColors* lut = vtkScalarsToColors::SafeDownCast(
      sourceColorMap->GetClientSideObject());
    if (lut) {
      d->mainSlice->GetProperty()->SetLookupTable(lut);
      for (int i = 0; i < 3; ++i) {
        d->ReconColorMap[i]->Copy(sourceColorMap);
        d->ReconColorMap[i]->UpdateVTKObjects();
      }
    }
  }

  vtkImageData* imageData = d->m_image;
  int dims[3];
  imageData->GetDimensions(dims);

  // The preview slices index the axis selected by the (persisted)
  // orientation, exactly as onOrientationChanged() does. Seeding them
  // from dims[0] regardless read past the end of the tilt series when a
  // vertical tilt axis was restored for a wider-than-tall stack (#2308).
  const int sliceAxis = d->m_orientation;
  d->m_slice0 = vtkMath::Round(0.25 * dims[sliceAxis]);
  d->m_slice1 = vtkMath::Round(0.50 * dims[sliceAxis]);
  d->m_slice2 = vtkMath::Round(0.75 * dims[sliceAxis]);

  int projectionNum = dims[2] / 2;
  d->m_projectionNum = projectionNum;
  d->mainSliceMapper->SetSliceNumber(projectionNum);
  d->mainSliceMapper->Update();

  d->m_shiftRotation = 0;
  d->m_tiltRotation = 0;

  d->moveRotationAxisLine();

  updateControls();

  d->updateReconSlice(0);
  d->updateReconSlice(1);
  d->updateReconSlice(2);

  d->setupCameras();
  d->setupRotationAxisLine();

  updateWidgets();
}


RotateAlignWidget::~RotateAlignWidget() {}

void RotateAlignWidget::getValues(QMap<QString, QVariant>& map)
{
  QList<QVariant> shift;
  shift << 0 << -this->Internals->m_shiftRotation << 0;

  // Swap x and y for the shift if we are doing vertical alignment
  auto orientation = this->Internals->m_orientation;
  if (orientation != 0)
    shift.swapItemsAt(0, 1);

  map.insert("SHIFT", shift);
  map.insert("rotation_angle", this->Internals->m_tiltRotation);
  map.insert("tilt_axis", orientation);
}

void RotateAlignWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("tilt_axis")) {
    // Tilt axis should be updated first so the shift knows which index
    // to use.
    auto tilt = map["tilt_axis"];
    onOrientationChanged(tilt.toInt());
  }
  if (map.contains("SHIFT")) {
    auto shift = map["SHIFT"];
    auto ind = (this->Internals->m_orientation == 0 ? 1 : 0);
    onRotationShiftChanged(-shift.toList()[ind].toInt());
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

void RotateAlignWidget::onSumProjectionsToggled(bool sumProjections)
{
  if (sumProjections == this->Internals->m_sumProjections) {
    return;
  }

  this->Internals->m_sumProjections = sumProjections;

  if (sumProjections) {
    this->Internals->initializeSummedImage();
    this->Internals->mainSliceMapper->SetInputData(
      this->Internals->m_summedImage);
    this->Internals->mainSliceMapper->SetSliceNumber(0);
  } else {
    this->Internals->mainSliceMapper->SetInputData(this->Internals->m_image);
    this->Internals->mainSliceMapper->SetSliceNumber(
      this->Internals->m_projectionNum);
  }

  this->Internals->mainSliceMapper->Update();
  this->Internals->Ui.sliceView->renderWindow()->Render();
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
  for (const T& t : list) {
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
    QSignalBlocker(this->Internals->Ui.rotationAngle),
    QSignalBlocker(this->Internals->Ui.orientation)
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

  this->Internals->Ui.orientation->setCurrentIndex(tiltAxis);

  this->Internals->axesActor->SetXAxisRange(xAxisRange);
  this->Internals->axesActor->SetYAxisRange(yAxisRange);

  // It would be nice if we could only write the settings when the
  // widget is accepted, but I don't immediately see an easy way
  // to do that.
  this->Internals->writeSettings();
}

void RotateAlignWidget::onFinalReconButtonPressed() {}
} // namespace tomviz
