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
#include "OperatorFactory.h"

#include "ConvertToFloatOperator.h"
#include "CropOperator.h"
#include "DataSource.h"
#include "OperatorPython.h"
#include "ReconstructionOperator.h"
#include "SetTiltAnglesOperator.h"
#include "SnapshotOperator.h"
#include "TranslateAlignOperator.h"

#include "vtkFieldData.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"
#include "vtkTypeInt8Array.h"

namespace {
class ConvertToVolumeOperator : public tomviz::Operator
{
  Q_OBJECT
public:
  ConvertToVolumeOperator(QObject* p = nullptr) : Operator(p) {}
  ~ConvertToVolumeOperator() {}

  QString label() const override { return "Mark as Volume"; }
  QIcon icon() const override { return QIcon(); }
  Operator* clone() const override { return new ConvertToVolumeOperator; }

protected:
  bool applyTransform(vtkDataObject* data) override
  {
    // The array should already exist... but just in case
    vtkFieldData* fd = data->GetFieldData();
    // Make sure the data is marked as a tilt series
    vtkTypeInt8Array* dataType =
      vtkTypeInt8Array::SafeDownCast(fd->GetArray("tomviz_data_source_type"));
    if (!dataType) {
      vtkNew<vtkTypeInt8Array> array;
      array->SetNumberOfTuples(1);
      array->SetName("tomviz_data_source_type");
      fd->AddArray(array.Get());
      dataType = array.Get();
    }
    // It should already be this value...
    dataType->SetTuple1(0, tomviz::DataSource::Volume);
    return true;
  }

private:
  Q_DISABLE_COPY(ConvertToVolumeOperator)
};

#include "OperatorFactory.moc"
}

namespace tomviz {

OperatorFactory::OperatorFactory()
{
}

OperatorFactory::~OperatorFactory()
{
}

QList<QString> OperatorFactory::operatorTypes()
{
  QList<QString> reply;
  reply << "Python"
        << "ConvertToFloat"
        << "ConvertToVolume"
        << "Crop"
        << "CxxReconstruction"
        << "SetTiltAngles"
        << "TranslateAlign"
        << "Snapshot";
  qSort(reply);
  return reply;
}

Operator* OperatorFactory::createConvertToVolumeOperator()
{
  return new ConvertToVolumeOperator;
}

Operator* OperatorFactory::createOperator(const QString& type, DataSource* ds)
{

  Operator* op = nullptr;
  if (type == "Python") {
    op = new OperatorPython();
  } else if (type == "ConvertToFloat") {
    op = new ConvertToFloatOperator();
  } else if (type == "ConvertToVolume") {
    op = new ConvertToVolumeOperator();
  } else if (type == "Crop") {
    op = new CropOperator();
  } else if (type == "CxxReconstruction") {
    op = new ReconstructionOperator(ds);
  } else if (type == "SetTiltAngles") {
    op = new SetTiltAnglesOperator();
  } else if (type == "TranslateAlign") {
    op = new TranslateAlignOperator(ds);
  } else if (type == "Snapshot") {
    op = new SnapshotOperator(ds);
  }
  return op;
}

const char* OperatorFactory::operatorType(Operator* op)
{
  if (qobject_cast<OperatorPython*>(op)) {
    return "Python";
  }
  if (qobject_cast<ConvertToVolumeOperator*>(op)) {
    return "ConvertToVolume";
  }
  if (qobject_cast<ConvertToFloatOperator*>(op)) {
    return "ConvertToFloat";
  }
  if (qobject_cast<CropOperator*>(op)) {
    return "Crop";
  }
  if (qobject_cast<ReconstructionOperator*>(op)) {
    return "CxxReconstruction";
  }
  if (qobject_cast<SetTiltAnglesOperator*>(op)) {
    return "SetTiltAngles";
  }
  if (qobject_cast<TranslateAlignOperator*>(op)) {
    return "TranslateAlign";
  }
  if (qobject_cast<SnapshotOperator*>(op)) {
    return "Snapshot";
  }
  return nullptr;
}
}
