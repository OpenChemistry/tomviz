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

#include "QVTKWidget.h"
#include "vtkCamera.h"
#include "vtkDataArray.h"
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
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkScalarsToColors.h"
#include "vtkSMSourceProxy.h"
#include "vtkTransform.h"
#include "vtkTrivialProducer.h"
#include "vtkVector.h"

#include "ui_RotateAlignWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QPointer>
#include <QDoubleSpinBox>

namespace
{
void setupRenderer(vtkRenderer *renderer, vtkImageSliceMapper *mapper)
{
  int axis = mapper->GetOrientation();
  int horizontal;
  int vertical;
  if (axis == 2)
  {
    horizontal = 0;
    vertical = 1;
  }
  else if (axis == 0)
  {
    horizontal = 1;
    vertical = 2;
  }
  else
  {
    horizontal = 0;
    vertical = 2;
  }
  renderer->SetBackground(1.0, 1.0, 1.0);
  vtkCamera *camera = renderer->GetActiveCamera();
  renderer->SetViewport(0.0, 0.0,
                        1.0, 1.0);

  double *bounds = mapper->GetBounds();
  vtkVector3d point;
  point[0] = 0.5 * (bounds[0] + bounds[1]);
  point[1] = 0.5 * (bounds[2] + bounds[3]);
  point[2] = 0.5 * (bounds[4] + bounds[5]);
  camera->SetFocalPoint(point.GetData());
  point[axis] += 50 + 0.5 * (bounds[axis * 2 + 1] - bounds[axis * 2]);
  camera->SetPosition(point.GetData());
  double viewUp[3] = { 0.0, 0.0, 0.0 };
  viewUp[vertical] = 1.0;
  camera->SetViewUp(viewUp);
  camera->ParallelProjectionOn();
  double parallelScale;
  if (bounds[horizontal * 2 + 1] - bounds[horizontal * 2] <
        bounds[vertical * 2 + 1] - bounds[vertical * 2])
  {
    parallelScale = 0.5 * (bounds[vertical * 2 + 1] - bounds[vertical * 2] + 1);
  }
  else
  {
    parallelScale = 0.5 * (bounds[horizontal * 2 + 1] - bounds[horizontal * 2] + 1);
  }
  camera->SetParallelScale(parallelScale);
  double clippingRange[2];
  camera->GetClippingRange(clippingRange);
  clippingRange[1] = clippingRange[0] + (bounds[axis * 2 + 1] - bounds[axis * 2] + 50);
  camera->SetClippingRange(clippingRange);
}
}

namespace tomviz
{

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

  void setupCameras()
  {
    setupRenderer(this->mainRenderer.Get(), this->mainSliceMapper.Get());
    setupRenderer(this->reconRenderer[0].Get(), this->reconSliceMapper[0].Get());
    setupRenderer(this->reconRenderer[1].Get(), this->reconSliceMapper[1].Get());
    setupRenderer(this->reconRenderer[2].Get(), this->reconSliceMapper[2].Get());
  }

  void setupRotationAxisLine()
  {
    vtkTrivialProducer *t =
        vtkTrivialProducer::SafeDownCast(this->Source->producer()->GetClientSideObject());
    if (!t)
    {
      return;
    }
    vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData)
    {
      int extent[6];
      imageData->GetExtent(extent);
      double spacing[3];
      imageData->GetSpacing(spacing);
      this->Ui.projection->setMaximum(extent[5] - extent[4]);
      int yslices = extent[3] - extent[2] + 1;
      this->Ui.rotationAxis->setRange(-yslices * spacing[1] / 2.0, yslices * spacing[1] / 2.0);
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
    vtkTransform *tform = vtkTransform::SafeDownCast(this->axisActor->GetUserTransform());
    if (!tform)
    {
      vtkNew<vtkTransform> t;
      t->PreMultiply();
      tform = t.Get();
      this->axisActor->SetUserTransform(t.Get());
    }
    vtkTrivialProducer *t =
        vtkTrivialProducer::SafeDownCast(this->Source->producer()->GetClientSideObject());
    if (!t)
    {
      return;
    }
    double centerOfRotation[3] = { 0.0, 0.0, 0.0 };
    vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData)
    {
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
    tform->Translate(-centerOfRotation[0], -centerOfRotation[1], -centerOfRotation[2]);
    this->Ui.sliceView->update();
  }

  void updateReconSlice(int i)
  {
    vtkTrivialProducer *t =
        vtkTrivialProducer::SafeDownCast(this->Source->producer()->GetClientSideObject());
    if (!t)
    {
      return;
    }
    vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData)
    {
      int extent[6];
      imageData->GetExtent(extent);
      int dims[3] = { extent[1] - extent[0] + 1,
                      extent[3] - extent[2] + 1,
                      extent[5] - extent[4] + 1 };
      QSpinBox *spinBoxes[3] = { this->Ui.spinBox_1, this->Ui.spinBox_2, this->Ui.spinBox_3 };
      int sliceNum = spinBoxes[i]->value();
      std::vector<float> sinogram(dims[1] * dims[2]);
      TomographyReconstruction::tiltSeriesToSinogram(imageData, sliceNum, &sinogram[0]);
      this->reconImage[i]->SetExtent(0, dims[1] - 1, 0, dims[1] - 1, 0, 0);
      this->reconImage[i]->AllocateScalars(VTK_FLOAT, 1);
      vtkDataArray *reconArray = this->reconImage[i]->GetPointData()->GetScalars();
      float *reconPtr = static_cast<float*>(reconArray->GetVoidPointer(0));

      vtkDataArray *tiltAnglesArray = imageData->GetFieldData()->GetArray("tilt_angles");
      double *tiltAngles = static_cast<double*>(tiltAnglesArray->GetVoidPointer(0));

      TomographyReconstruction::unweightedBackProjection2(&sinogram[0], tiltAngles, reconPtr, dims[2], dims[1]);
      this->reconSliceMapper[i]->SetInputData(this->reconImage[i].GetPointer());
      this->reconSliceMapper[i]->SetSliceNumber(0);
      this->reconSliceMapper[i]->Update();
    }
  }

  void updateSliceLines()
  {
    vtkTrivialProducer *t =
        vtkTrivialProducer::SafeDownCast(this->Source->producer()->GetClientSideObject());
    if (!t)
    {
      return;
    }
    vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData)
    {
      double bounds[6];
      imageData->GetBounds(bounds);
      int extent[6];
      imageData->GetExtent(extent);
      double maxSlices = extent[1] - extent[0] + 1;
      QSpinBox *spinBoxes[3] = { this->Ui.spinBox_1, this->Ui.spinBox_2, this->Ui.spinBox_3 };
      for (int i = 0; i < 3; ++i)
      {
        double p1[3], p2[3];
        p1[0] = bounds[0] + (bounds[1] - bounds[0]) * (spinBoxes[i]->value() / maxSlices);
        p2[0] = bounds[0] + (bounds[1] - bounds[0]) * (spinBoxes[i]->value() / maxSlices);
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

RotateAlignWidget::RotateAlignWidget(DataSource *source, QWidget *p)
  : Superclass(p), Internals(new RAWInternal)
{
  this->Internals->Ui.setupUi(this);
  this->Internals->mainSlice->SetMapper(this->Internals->mainSliceMapper.Get());
  this->Internals->reconSlice[0]->SetMapper(this->Internals->reconSliceMapper[0].Get());
  this->Internals->reconSlice[1]->SetMapper(this->Internals->reconSliceMapper[1].Get());
  this->Internals->reconSlice[2]->SetMapper(this->Internals->reconSliceMapper[2].Get());
  this->Internals->mainRenderer->AddViewProp(this->Internals->mainSlice.Get());
  this->Internals->reconRenderer[0]->AddViewProp(this->Internals->reconSlice[0].Get());
  this->Internals->reconRenderer[1]->AddViewProp(this->Internals->reconSlice[1].Get());
  this->Internals->reconRenderer[2]->AddViewProp(this->Internals->reconSlice[2].Get());
  this->Internals->Ui.sliceView->GetRenderWindow()->AddRenderer(
      this->Internals->mainRenderer.Get());
  this->Internals->Ui.sliceView_1->GetRenderWindow()->AddRenderer(
      this->Internals->reconRenderer[0].Get());
  this->Internals->Ui.sliceView_2->GetRenderWindow()->AddRenderer(
      this->Internals->reconRenderer[1].Get());
  this->Internals->Ui.sliceView_3->GetRenderWindow()->AddRenderer(
      this->Internals->reconRenderer[2].Get());

  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle;
  interatorStyle->SetRenderOnMouseMove(true);

  this->Internals->Ui.sliceView->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      interatorStyle.Get());
  this->Internals->Ui.sliceView_1->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      interatorStyle.Get());
  this->Internals->Ui.sliceView_2->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      interatorStyle.Get());
  this->Internals->Ui.sliceView_3->GetRenderWindow()->GetInteractor()->SetInteractorStyle(
      interatorStyle.Get());
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

  for (int i = 0; i < 3; ++i)
  {
    this->Internals->reconSliceLine[i]->Update();
    vtkNew<vtkPolyDataMapper> sMapper;
    sMapper->SetInputConnection(this->Internals->reconSliceLine[i]->GetOutputPort());
    this->Internals->reconSliceLineActor[i]->SetMapper(sMapper.Get());
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetColor(1, 0, 0);
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetLineWidth(2.0);
    this->Internals->reconSliceLineActor[i]->GetProperty()->SetLineStipplePattern(0xFF00);
    this->Internals->mainRenderer->AddActor(this->Internals->reconSliceLineActor[i].Get());
  }
  this->Internals->Ui.spinBox_1->setValue(25);
  this->Internals->Ui.spinBox_2->setValue(50);
  this->Internals->Ui.spinBox_3->setValue(75);

  this->connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource *)),
                SLOT(setDataSource(DataSource *)));
  this->connect(this->Internals->Ui.projection, SIGNAL(valueChanged(int)),
                SLOT(onProjectionNumberChanged(int)));
  this->connect(this->Internals->Ui.spinBox_1, SIGNAL(valueChanged(int)),
                SLOT(onReconSliceChanged(int)));
  this->connect(this->Internals->Ui.spinBox_2, SIGNAL(valueChanged(int)),
                SLOT(onReconSliceChanged(int)));
  this->connect(this->Internals->Ui.spinBox_3, SIGNAL(valueChanged(int)),
                SLOT(onReconSliceChanged(int)));
  this->connect(this->Internals->Ui.rotationAxis, SIGNAL(valueChanged(double)),
                SLOT(onRotationAxisChanged()));
  this->connect(this->Internals->Ui.rotationAngle, SIGNAL(valueChanged(double)),
                SLOT(onRotationAxisChanged()));
  this->connect(this->Internals->Ui.pushButton, SIGNAL(pressed()),
                SLOT(onFinalReconButtonPressed()));

  this->setDataSource(source);
  this->onProjectionNumberChanged(0);
}

RotateAlignWidget::~RotateAlignWidget()
{
}

void RotateAlignWidget::setDataSource(DataSource *source)
{
  this->Internals->Source = source;
  if (source)
  {
    vtkTrivialProducer *t =
        vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
    if (t)
    {
      this->Internals->mainSliceMapper->SetInputConnection(t->GetOutputPort());
      this->Internals->reconSliceMapper[0]->SetInputConnection(t->GetOutputPort());
      this->Internals->reconSliceMapper[1]->SetInputConnection(t->GetOutputPort());
      this->Internals->reconSliceMapper[2]->SetInputConnection(t->GetOutputPort());
    }
    this->Internals->mainSliceMapper->Update();
    vtkScalarsToColors *lut =
        vtkScalarsToColors::SafeDownCast(source->colorMap()->GetClientSideObject());
    if (lut)
    {
      this->Internals->mainSlice->GetProperty()->SetLookupTable(lut);
      this->Internals->reconSlice[0]->GetProperty()->SetLookupTable(lut);
      this->Internals->reconSlice[1]->GetProperty()->SetLookupTable(lut);
      this->Internals->reconSlice[2]->GetProperty()->SetLookupTable(lut);
    }
    vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    int extent[6];
    imageData->GetExtent(extent);
    this->Internals->Ui.xSizeLabel->setText(QString::number(extent[1] - extent[0]));
    this->Internals->Ui.ySizeLabel->setText(QString::number(extent[3] - extent[2]));

    this->Internals->Ui.spinBox_1->setRange(0, extent[1] - extent[0]);
    this->Internals->Ui.spinBox_2->setRange(0, extent[1] - extent[0]);
    this->Internals->Ui.spinBox_3->setRange(0, extent[1] - extent[0]);
    this->Internals->Ui.spinBox_1->setValue(vtkMath::Round(0.25 * (extent[1] - extent[0])));
    this->Internals->Ui.spinBox_2->setValue(vtkMath::Round(0.5  * (extent[1] - extent[0])));
    this->Internals->Ui.spinBox_3->setValue(vtkMath::Round(0.75 * (extent[1] - extent[0])));

    this->Internals->setupCameras();
    this->Internals->setupRotationAxisLine();
  }
  else
  {
    this->Internals->mainSliceMapper->SetInputConnection(NULL);
    this->Internals->mainSliceMapper->Update();
  }
  updateWidgets();
}

void RotateAlignWidget::onProjectionNumberChanged(int newVal)
{
  vtkTrivialProducer *t =
      vtkTrivialProducer::SafeDownCast(this->Internals->Source->producer()->GetClientSideObject());
  if (!t)
  {
    std::cout << "Failed to get trivail producer" << std::endl;
    return;
  }
  vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int extent[6];
  imageData->GetExtent(extent);
  this->Internals->mainSliceMapper->SetSliceNumber(newVal + extent[4]);
  this->Internals->mainSliceMapper->Update();
  this->Internals->Ui.sliceView->update();
}

void RotateAlignWidget::onRotationAxisChanged()
{
  this->Internals->moveRotationAxisLine();
}

void RotateAlignWidget::onRotationAngleChanged()
{
  this->Internals->moveRotationAxisLine();
}

void RotateAlignWidget::onReconSliceChanged(int)
{
  QSpinBox *sb = qobject_cast<QSpinBox*>(this->sender());
  this->Internals->updateSliceLines();
  this->Internals->Ui.sliceView->update();
  if (sb == this->Internals->Ui.spinBox_1)
  {
    this->Internals->updateReconSlice(0);
//    this->Internals->reconSliceMapper[0]->SetSliceNumber(newSlice);
    this->Internals->reconSliceMapper[0]->Update();
    this->Internals->Ui.sliceView_1->update();
  }
  else if (sb == this->Internals->Ui.spinBox_2)
  {
    this->Internals->updateReconSlice(1);
//    this->Internals->reconSliceMapper[1]->SetSliceNumber(newSlice);
    this->Internals->reconSliceMapper[1]->Update();
    this->Internals->Ui.sliceView_2->update();
  }
  else // if (sb == this->Internals->Ui.spinBox_3)
  {
    this->Internals->updateReconSlice(2);
//    this->Internals->reconSliceMapper[2]->SetSliceNumber(newSlice);
    this->Internals->reconSliceMapper[2]->Update();
    this->Internals->Ui.sliceView_3->update();
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
  DataSource *source = this->Internals->Source;
  if (!source)
  {
    return;
  }
  vtkTrivialProducer *t =
      vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
  vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  DataSource* output = source->clone(true,true);
  QString name = output->producer()->GetAnnotation("tomviz.Label");
  name = "Rotation_Aligned_" + name;
  output->producer()->SetAnnotation("tomviz.Label", name.toAscii().data());
  t = vtkTrivialProducer::SafeDownCast(output->producer()->GetClientSideObject());
  vtkImageData *recon = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  TomographyReconstruction::weightedBackProjection3(imageData, recon);
  output->dataModified();

  LoadDataReaction::dataSourceAdded(output);
  emit creatingAlignedData();
}

}
