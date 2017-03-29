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

#include "RotateAlignWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "TomographyReconstruction.h"
#include "TomographyTiltSeries.h"
#define PI 3.14159265359
#include "AddPythonTransformReaction.h"
#include "Utilities.h"
#include <math.h>

#include "pqCoreUtilities.h"
#include "pqPresetDialog.h"
#include "vtkCamera.h"
#include "vtkDataArray.h"
#include "vtkGenericOpenGLRenderWindow.h"
#include "vtkImageData.h"
#include "vtkImageProperty.h"
#include "vtkImageSlice.h"
#include "vtkImageSliceMapper.h"
#include "vtkInteractorStyleRubberBand2D.h"
#include "vtkLineSource.h"
#include "vtkMath.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTransferFunctionManager.h"
#include "vtkSMTransferFunctionProxy.h"
#include "vtkScalarsToColors.h"
#include "vtkTransform.h"
#include "vtkTrivialProducer.h"
#include "vtkVector.h"

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

#include <array>

namespace tomviz {

class RotateAlignWidget::RAWInternal
{
public:
  Ui::RotateAlignWidget Ui;
  vtkNew<vtkImageSlice> mainSlice;
  vtkNew<vtkImageData> reconImage[3];
  vtkNew<vtkImageSlice> reconSlice[3];
  vtkNew<vtkImageSliceMapper> mainSliceMapper;
  vtkNew<vtkImageSliceMapper> reconSliceMapper[3];
  vtkNew<vtkRenderer> mainRenderer;
  vtkNew<vtkRenderer> reconRenderer[3];
  QPointer<DataSource> Source;
  vtkNew<vtkLineSource> rotationAxis;
  vtkNew<vtkActor> axisActor;
  vtkNew<vtkLineSource> reconSliceLine[3];
  vtkNew<vtkActor> reconSliceLineActor[3];
  vtkSmartPointer<vtkSMProxy> ReconColorMap[3];

  void setupCameras()
  {
    tomviz::setupRenderer(this->mainRenderer.Get(),
                          this->mainSliceMapper.Get());
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
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      this->Source->producer()->GetClientSideObject());
    if (!t) {
      return;
    }
    vtkImageData* imageData =
      vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData) {
      int extent[6];
      imageData->GetExtent(extent);
      double spacing[3];
      imageData->GetSpacing(spacing);
      this->Ui.projection->setMaximum(extent[5] - extent[4]);
      int yslices = extent[3] - extent[2] + 1;
      this->Ui.rotationAxis->setRange(-yslices * spacing[1] / 2.0,
                                      yslices * spacing[1] / 2.0);
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
      this->Ui.rotationAngle->setSingleStep(0.5);
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
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      this->Source->producer()->GetClientSideObject());
    if (!t) {
      return;
    }
    double centerOfRotation[3] = { 0.0, 0.0, 0.0 };
    vtkImageData* imageData =
      vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData) {
      double bounds[6];
      imageData->GetBounds(bounds);
      centerOfRotation[0] = (bounds[0] + bounds[1]) / 2.0;
      centerOfRotation[1] = (bounds[2] + bounds[3]) / 2.0;
      centerOfRotation[2] = (bounds[4] + bounds[5]) / 2.0;
    }
    tform->Identity();
    tform->Translate(0, this->Ui.rotationAxis->value(), 0);
    tform->Translate(centerOfRotation);
    tform->RotateZ(this->Ui.rotationAngle->value());
    tform->Translate(-centerOfRotation[0], -centerOfRotation[1],
                     -centerOfRotation[2]);
    this->Ui.sliceView->update();
  }

  void updateReconSlice(int i)
  {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      this->Source->producer()->GetClientSideObject());
    if (!t) {
      return;
    }
    vtkImageData* imageData =
      vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData) {
      int extent[6];
      imageData->GetExtent(extent);
      int dims[3] = { extent[1] - extent[0] + 1, extent[3] - extent[2] + 1,
                      extent[5] - extent[4] + 1 };
      QSpinBox* spinBoxes[3] = { this->Ui.spinBox_1, this->Ui.spinBox_2,
                                 this->Ui.spinBox_3 };
      int sliceNum = spinBoxes[i]->value();

      int Nray = 256; // Size of 2D reconstruction. Fixed for all tilt series
      std::vector<float> sinogram(Nray * dims[2]);
      // Approximate in-plance rotation as a shift in y-direction
      double shift = this->Ui.rotationAxis->value() +
                     sin(this->Ui.rotationAngle->value() * PI / 180) *
                       (sliceNum - dims[0] / 2);

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
    }
  }

  void updateSliceLines()
  {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      this->Source->producer()->GetClientSideObject());
    if (!t) {
      return;
    }
    vtkImageData* imageData =
      vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData) {
      double bounds[6];
      imageData->GetBounds(bounds);
      int extent[6];
      imageData->GetExtent(extent);
      double maxSlices = extent[1] - extent[0] + 1;
      QSpinBox* spinBoxes[3] = { this->Ui.spinBox_1, this->Ui.spinBox_2,
                                 this->Ui.spinBox_3 };
      for (int i = 0; i < 3; ++i) {
        double p1[3], p2[3];
        p1[0] = bounds[0] +
                (bounds[1] - bounds[0]) * (spinBoxes[i]->value() / maxSlices);
        p2[0] = bounds[0] +
                (bounds[1] - bounds[0]) * (spinBoxes[i]->value() / maxSlices);
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

RotateAlignWidget::RotateAlignWidget(DataSource* source, QWidget* p)
  : Superclass(p), Internals(new RAWInternal)
{
  this->Internals->Ui.setupUi(this);

  vtkNew<vtkGenericOpenGLRenderWindow> sWindow;
  this->Internals->Ui.sliceView->SetRenderWindow(sWindow.Get());
  vtkNew<vtkGenericOpenGLRenderWindow> sWindow1;
  this->Internals->Ui.sliceView_1->SetRenderWindow(sWindow1.Get());
  vtkNew<vtkGenericOpenGLRenderWindow> sWindow2;
  this->Internals->Ui.sliceView_2->SetRenderWindow(sWindow2.Get());
  vtkNew<vtkGenericOpenGLRenderWindow> sWindow3;
  this->Internals->Ui.sliceView_3->SetRenderWindow(sWindow3.Get());

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
  this->Internals->Ui.spinBox_1->setValue(25);
  this->Internals->Ui.spinBox_2->setValue(50);
  this->Internals->Ui.spinBox_3->setValue(75);

  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(setDataSource(DataSource*)));

  QObject::connect(this->Internals->Ui.projection, SIGNAL(editingFinished()),
                   this, SLOT(onProjectionNumberChanged()));
  this->Internals->Ui.projection->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_1, SIGNAL(editingFinished()),
                   this, SLOT(onReconSlice0Changed()));
  this->Internals->Ui.spinBox_1->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_2, SIGNAL(editingFinished()),
                   this, SLOT(onReconSlice1Changed()));
  this->Internals->Ui.spinBox_2->installEventFilter(this);

  QObject::connect(this->Internals->Ui.spinBox_3, SIGNAL(editingFinished()),
                   this, SLOT(onReconSlice2Changed()));
  this->Internals->Ui.spinBox_3->installEventFilter(this);

  QObject::connect(this->Internals->Ui.rotationAxis, SIGNAL(editingFinished()),
                   this, SLOT(onRotationAxisChanged()));
  this->Internals->Ui.rotationAxis->installEventFilter(this);

  QObject::connect(this->Internals->Ui.rotationAngle, SIGNAL(editingFinished()),
                   this, SLOT(onRotationAxisChanged()));
  this->Internals->Ui.rotationAngle->installEventFilter(this);

  this->connect(this->Internals->Ui.pushButton, SIGNAL(pressed()),
                SLOT(onFinalReconButtonPressed()));

  this->setDataSource(source);
}

RotateAlignWidget::~RotateAlignWidget()
{
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

void RotateAlignWidget::setDataSource(DataSource* source)
{
  this->Internals->Source = source;
  if (source) {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    if (t) {
      this->Internals->mainSliceMapper->SetInputConnection(t->GetOutputPort());
    }
    this->Internals->mainSliceMapper->Update();
    vtkScalarsToColors* lut = vtkScalarsToColors::SafeDownCast(
      source->colorMap()->GetClientSideObject());
    if (lut) {
      this->Internals->mainSlice->GetProperty()->SetLookupTable(lut);
      for (int i = 0; i < 3; ++i) {
        this->Internals->ReconColorMap[i]->Copy(source->colorMap());
        this->Internals->ReconColorMap[i]->UpdateVTKObjects();
      }
    }
    vtkImageData* imageData =
      vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int extent[6];
    imageData->GetExtent(extent);
    this->Internals->Ui.xSizeLabel->setText(
      QString::number(extent[1] - extent[0]));
    this->Internals->Ui.ySizeLabel->setText(
      QString::number(extent[3] - extent[2]));

    this->Internals->Ui.projection->setValue((extent[5] - extent[4]) / 2);
    this->onProjectionNumberChanged();
    this->Internals->Ui.spinBox_1->setRange(0, extent[1] - extent[0]);
    this->Internals->Ui.spinBox_2->setRange(0, extent[1] - extent[0]);
    this->Internals->Ui.spinBox_3->setRange(0, extent[1] - extent[0]);
    this->Internals->Ui.spinBox_1->setValue(
      vtkMath::Round(0.25 * (extent[1] - extent[0])));
    this->Internals->Ui.spinBox_2->setValue(
      vtkMath::Round(0.5 * (extent[1] - extent[0])));
    this->Internals->Ui.spinBox_3->setValue(
      vtkMath::Round(0.75 * (extent[1] - extent[0])));

    this->Internals->updateReconSlice(0);
    this->Internals->updateReconSlice(1);
    this->Internals->updateReconSlice(2);

    this->Internals->setupCameras();
    this->Internals->setupRotationAxisLine();
  } else {
    this->Internals->mainSliceMapper->SetInputConnection(NULL);
    this->Internals->mainSliceMapper->Update();
  }
  updateWidgets();
}

void RotateAlignWidget::onProjectionNumberChanged()
{
  int newVal = this->Internals->Ui.projection->value();
  vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
    this->Internals->Source->producer()->GetClientSideObject());
  if (!t) {
    std::cout << "Failed to get trivail producer" << std::endl;
    return;
  }
  vtkImageData* imageData =
    vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int extent[6];
  imageData->GetExtent(extent);
  this->Internals->mainSliceMapper->SetSliceNumber(newVal + extent[4]);
  this->Internals->mainSliceMapper->Update();
  this->Internals->Ui.sliceView->update();
}

void RotateAlignWidget::onRotationAxisChanged()
{
  this->Internals->moveRotationAxisLine();
  // Update recon windows
  this->Internals->updateReconSlice(0);
  this->Internals->reconSliceMapper[0]->Update();
  this->Internals->Ui.sliceView_1->update();

  this->Internals->updateReconSlice(1);
  this->Internals->reconSliceMapper[1]->Update();
  this->Internals->Ui.sliceView_2->update();

  this->Internals->updateReconSlice(2);
  this->Internals->reconSliceMapper[2]->Update();
  this->Internals->Ui.sliceView_3->update();
}

void RotateAlignWidget::onReconSliceChanged(int idx)
{
  this->Internals->updateSliceLines();
  this->Internals->Ui.sliceView->update();
  this->Internals->updateReconSlice(idx);
  this->Internals->reconSliceMapper[idx]->Update();
  if (idx == 0) {
    this->Internals->Ui.sliceView_1->update();
  } else if (idx == 1) {
    this->Internals->Ui.sliceView_2->update();
  } else // if (idx == 2)
  {
    this->Internals->Ui.sliceView_3->update();
  }
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
}

void RotateAlignWidget::showChangeColorMapDialog(int reconSlice)
{
  auto slotsArray = make_array({ &RotateAlignWidget::changeColorMap0,
                                 &RotateAlignWidget::changeColorMap1,
                                 &RotateAlignWidget::changeColorMap2 });
  pqPresetDialog dialog(pqCoreUtilities::mainWidget(),
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
  this->Internals->Ui.sliceView->update();
  this->Internals->Ui.sliceView_1->update();
  this->Internals->Ui.sliceView_2->update();
  this->Internals->Ui.sliceView_3->update();
}

void RotateAlignWidget::onFinalReconButtonPressed()
{
  DataSource* source = this->Internals->Source;
  if (!source) {
    return;
  }
  /*
    DataSource* output = source->clone(true,true);
    QString name = output->producer()->GetAnnotation(Attributes::LABEL);
    name = "Rotation_Aligned_" + name;
    output->producer()->SetAnnotation(Attributes::LABEL, name.toAscii().data());
    t =
    vtkTrivialProducer::SafeDownCast(output->producer()->GetClientSideObject());
    vtkImageData *recon = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

    LoadDataReaction::dataSourceAdded(output);

    */
  // Apply python transform
  // Apply shift (in y-direction)
  QMap<QString, QVariant> arguments;
  QList<QVariant> value;
  value << 0 << -this->Internals->Ui.rotationAxis->value() << 0;
  arguments.insert("SHIFT", value);

  QString scriptLabel = "Shift";
  QString scriptSource = readInPythonScript("Shift3D");
  AddPythonTransformReaction::addPythonOperator(
    this->Internals->Source, scriptLabel, scriptSource, arguments,
    readInJSONDescription("Shift3D"));
  arguments.clear();

  // Apply in-plane rotation
  scriptLabel = "Rotate";
  scriptSource = readInPythonScript("Rotate3D");
  arguments.insert("rotation_axis", 2);
  arguments.insert("rotation_angle",
                   -this->Internals->Ui.rotationAngle->value());

  AddPythonTransformReaction::addPythonOperator(
    this->Internals->Source, scriptLabel, scriptSource, arguments,
    readInJSONDescription("Rotate3D"));
  emit creatingAlignedData();
}
}
