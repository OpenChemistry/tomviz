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

#include "ReconstructionOperator.h"

#include "DataSource.h"
#include "ReconstructionWidget.h"
#include "TomographyTiltSeries.h"
#include "TomographyReconstruction.h"

#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include <QCoreApplication>

namespace tomviz
{
ReconstructionOperator::ReconstructionOperator(DataSource *source, QObject *p)
  : Superclass(p), dataSource(source), canceled(false)
{
  qRegisterMetaType<std::vector<float> >();
  vtkTrivialProducer *t =
    vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
  vtkImageData *imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  int dataExtent[6];
  imageData->GetExtent(dataExtent);
  for (int i = 0; i < 6; ++i)
  {
    this->extent[i] = dataExtent[i];
  }
  this->setSupportsCancel(true);
}

ReconstructionOperator::~ReconstructionOperator()
{
}

QIcon ReconstructionOperator::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqExtractGrid24.png");
}

Operator* ReconstructionOperator::clone() const
{
  return new ReconstructionOperator(this->dataSource);
}

bool ReconstructionOperator::serialize(pugi::xml_node&) const
{
  // No state to serialize yet
  return true;
}

bool ReconstructionOperator::deserialize(const pugi::xml_node&)
{
  // No state to serialize yet
  return true;
}

EditOperatorWidget *ReconstructionOperator::getEditorContents(QWidget*)
{
  // No options to set
  return nullptr;
}

QWidget *ReconstructionOperator::getCustomProgressWidget(QWidget* p) const
{
  ReconstructionWidget *widget = new ReconstructionWidget(this->dataSource, p);
  QObject::connect(this, &Operator::updateProgress,
                   widget, &ReconstructionWidget::updateProgress);
  QObject::connect(this, &ReconstructionOperator::intermediateResults,
                   widget, &ReconstructionWidget::updateIntermediateResults);
  return widget;
}

int ReconstructionOperator::totalProgressSteps() const
{
  return this->extent[1] - this->extent[0] + 1;
}

void ReconstructionOperator::cancelTransform()
{
  this->canceled = true;
}

bool ReconstructionOperator::applyTransform(vtkDataObject* dataObject)
{
  vtkImageData *imageData = vtkImageData::SafeDownCast(dataObject);
  if (!imageData) {
    return false;
  }
  int dataExtent[6];
  imageData->GetExtent(dataExtent);
  for (int i = 0; i < 6; ++i)
  {
    if (dataExtent[i] != this->extent[i])
    {
      // Extent changing shouldn't matter, but update so that correct
      // number of steps can be reported.
      this->extent[i] = dataExtent[i];
    }
  }
  int numXSlices = dataExtent[1] - dataExtent[0] + 1;
  int numYSlices = dataExtent[3] - dataExtent[2] + 1;
  int numZSlices = dataExtent[5] - dataExtent[4] + 1;
  std::vector<float> sinogramPtr(numYSlices * numZSlices);
  std::vector<float> reconstructionPtr(numYSlices * numYSlices);
  QVector<double> tiltAngles = this->dataSource->getTiltAngles();

  vtkNew<vtkImageData> reconstructionImage;
  int extent2[6] = {dataExtent[0], extent[1], dataExtent[2], dataExtent[3], dataExtent[2], dataExtent[3]};
  reconstructionImage->SetExtent(extent2);
  reconstructionImage->AllocateScalars(VTK_FLOAT, 1);
  vtkDataArray *darray = reconstructionImage->GetPointData()->GetScalars();
  darray->SetName("scalars");

  // TODO: talk to Dave Lonie about how to do this in new data array API
  float *reconstruction = (float*)darray->GetVoidPointer(0);
  for (int i = 0; i < numXSlices && !this->canceled; ++i)
  {
    QCoreApplication::processEvents();
    TomographyTiltSeries::getSinogram(imageData, i, &sinogramPtr[0]);
    TomographyReconstruction::unweightedBackProjection2(
      &sinogramPtr[0], tiltAngles.data(), &reconstructionPtr[0], numZSlices, numYSlices);
    for (int j = 0; j < numYSlices; ++j) {
      for (int k = 0; k < numYSlices; ++k) {
        reconstruction[j * (numYSlices * numXSlices) + k * numXSlices + i] = reconstructionPtr[k * numYSlices + j];
      }
    }
    emit this->intermediateResults(reconstructionPtr);
    emit this->updateProgress(i);
  }
  if (this->canceled)
  {
    return false;
  }
  imageData->ShallowCopy(reconstructionImage.Get());
  return true;
}

}
