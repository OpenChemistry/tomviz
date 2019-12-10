/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ReconstructionOperator.h"

#include "DataSource.h"
#include "Pipeline.h"
#include "ReconstructionWidget.h"
#include "TomographyReconstruction.h"
#include "TomographyTiltSeries.h"

#include "pqSMProxy.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include <QCoreApplication>
#include <QDebug>

namespace tomviz {
ReconstructionOperator::ReconstructionOperator(DataSource* source, QObject* p)
  : Operator(p), m_dataSource(source)
{
  qRegisterMetaType<std::vector<float>>();
  auto t = source->producer();
  auto imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int dataExtent[6];
  imageData->GetExtent(dataExtent);
  for (int i = 0; i < 6; ++i) {
    m_extent[i] = dataExtent[i];
  }
  setSupportsCancel(true);
  setTotalProgressSteps(m_extent[1] - m_extent[0] + 1);
  setHasChildDataSource(true);
  connect(
    this,
    static_cast<void (Operator::*)(const QString&,
                                   vtkSmartPointer<vtkDataObject>)>(
      &Operator::newChildDataSource),
    this,
    [this](const QString& label, vtkSmartPointer<vtkDataObject> childData) {
      this->createNewChildDataSource(label, childData, DataSource::Volume,
                                     DataSource::PersistenceState::Transient);
    });
}

QIcon ReconstructionOperator::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqExtractGrid24.png");
}

Operator* ReconstructionOperator::clone() const
{
  return new ReconstructionOperator(m_dataSource);
}

QWidget* ReconstructionOperator::getCustomProgressWidget(QWidget* p) const
{
  DataSource* source = m_dataSource;
  if (source && source->pipeline()) {
    // Use the transformed data source for the reconstruction widget
    source = source->pipeline()->transformedDataSource();
  }

  ReconstructionWidget* widget = new ReconstructionWidget(source, p);
  QObject::connect(this, &Operator::progressStepChanged, widget,
                   &ReconstructionWidget::updateProgress);
  QObject::connect(this, &ReconstructionOperator::intermediateResults, widget,
                   &ReconstructionWidget::updateIntermediateResults);
  return widget;
}

bool ReconstructionOperator::applyTransform(vtkDataObject* dataObject)
{
  vtkSmartPointer<vtkImageData> imageData =
    vtkImageData::SafeDownCast(dataObject);
  if (!imageData) {
    return false;
  }
  int dataExtent[6];
  imageData->GetExtent(dataExtent);
  for (int i = 0; i < 6; ++i) {
    if (dataExtent[i] != m_extent[i]) {
      // Extent changing shouldn't matter, but update so that correct
      // number of steps can be reported.
      m_extent[i] = dataExtent[i];
    }
  }
  setTotalProgressSteps(m_extent[1] - m_extent[0] + 1);

  int numXSlices = dataExtent[1] - dataExtent[0] + 1;
  int numYSlices = dataExtent[3] - dataExtent[2] + 1;
  int numZSlices = dataExtent[5] - dataExtent[4] + 1;
  std::vector<float> sinogramPtr(numYSlices * numZSlices);
  std::vector<float> reconstructionPtr(numYSlices * numYSlices);
  QVector<double> tiltAngles;

  vtkFieldData* fd = dataObject->GetFieldData();
  vtkDataArray* tiltAnglesVTKArray = fd->GetArray("tilt_angles");
  if (tiltAnglesVTKArray) {
    tiltAngles.resize(tiltAnglesVTKArray->GetNumberOfTuples());
    for (int i = 0; i < tiltAngles.size(); ++i) {
      tiltAngles[i] = tiltAnglesVTKArray->GetTuple1(i);
    }
  }

  if (tiltAngles.size() < numZSlices) {
    qDebug() << "Incorrect number of tilt angles. There are"
             << tiltAngles.size() << "and there should be" << numZSlices
             << ".\n";
    return false;
  }

  vtkNew<vtkImageData> reconstructionImage;
  int extent2[6] = { dataExtent[0], m_extent[1],   dataExtent[2],
                     dataExtent[3], dataExtent[2], dataExtent[3] };
  reconstructionImage->SetExtent(extent2);
  reconstructionImage->AllocateScalars(VTK_FLOAT, 1);
  vtkDataArray* darray = reconstructionImage->GetPointData()->GetScalars();
  darray->SetName("scalars");

  // TODO: talk to Dave Lonie about how to do this in new data array API
  float* reconstruction = (float*)darray->GetVoidPointer(0);
  for (int i = 0; i < numXSlices && !isCanceled(); ++i) {
    QCoreApplication::processEvents();
    TomographyTiltSeries::getSinogram(imageData, i, &sinogramPtr[0]);
    TomographyReconstruction::unweightedBackProjection2(
      &sinogramPtr[0], tiltAngles.data(), &reconstructionPtr[0], numZSlices,
      numYSlices);
    for (int j = 0; j < numYSlices; ++j) {
      for (int k = 0; k < numYSlices; ++k) {
        reconstruction[j * (numYSlices * numXSlices) + k * numXSlices + i] =
          reconstructionPtr[k * numYSlices + j];
      }
    }
    emit intermediateResults(reconstructionPtr);
    setProgressStep(i);
  }
  if (isCanceled()) {
    return false;
  }
  emit newChildDataSource("Reconstruction", reconstructionImage);
  return true;
}
} // namespace tomviz
