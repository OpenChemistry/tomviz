/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorFactory.h"

#include "ArrayWranglerOperator.h"
#include "ConvertToFloatOperator.h"
#include "CropOperator.h"
#include "OperatorPython.h"
#include "ReconstructionOperator.h"
#include "SetTiltAnglesOperator.h"
#include "SnapshotOperator.h"
#include "TranslateAlignOperator.h"
#include "TransposeDataOperator.h"

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
  ConvertToVolumeOperator(
    QObject* p = nullptr,
    tomviz::DataSource::DataSourceType t = tomviz::DataSource::Volume,
    QString label = "Mark as Volume")
    : Operator(p), m_type(t), m_label(label)
  {
  }
  ~ConvertToVolumeOperator() {}

  QString label() const override { return m_label; }
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
      fd->AddArray(array);
      dataType = array;
    }
    // It should already be this value...
    dataType->SetTuple1(0, m_type);
    return true;
  }

private:
  Q_DISABLE_COPY(ConvertToVolumeOperator)
  tomviz::DataSource::DataSourceType m_type;
  QString m_label;
};

#include "OperatorFactory.moc"
} // namespace

namespace tomviz {

OperatorFactory::OperatorFactory() {}

OperatorFactory::~OperatorFactory() {}

QList<QString> OperatorFactory::operatorTypes()
{
  QList<QString> reply;
  reply << "Python"
        << "ArrayWrangler"
        << "ConvertToFloat"
        << "ConvertToVolume"
        << "Crop"
        << "CxxReconstruction"
        << "SetTiltAngles"
        << "TranslateAlign"
        << "TransposeData"
        << "Snapshot";
  qSort(reply);
  return reply;
}

Operator* OperatorFactory::createConvertToVolumeOperator(
  DataSource::DataSourceType t)
{
  if (t == DataSource::Volume) {
    return new ConvertToVolumeOperator(nullptr, t, "Mark as Volume");
  } else if (t == DataSource::FIB) {
    return new ConvertToVolumeOperator(nullptr, t, "Mark as Focused Ion Beam");
  }
  return nullptr;
}

Operator* OperatorFactory::createOperator(const QString& type, DataSource* ds)
{

  Operator* op = nullptr;
  if (type == "Python") {
    op = new OperatorPython();
  } else if (type == "ArrayWrangler") {
    op = new ArrayWranglerOperator();
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
  } else if (type == "TransposeData") {
    op = new TransposeDataOperator();
  } else if (type == "Snapshot") {
    op = new SnapshotOperator(ds);
  }
  return op;
}

const char* OperatorFactory::operatorType(const Operator* op)
{
  if (qobject_cast<const OperatorPython*>(op)) {
    return "Python";
  }
  if (qobject_cast<const ConvertToVolumeOperator*>(op)) {
    return "ConvertToVolume";
  }
  if (qobject_cast<ArrayWranglerOperator*>(op)) {
    return "ArrayWrangler";
  }
  if (qobject_cast<ConvertToFloatOperator*>(op)) {
    return "ConvertToFloat";
  }
  if (qobject_cast<const CropOperator*>(op)) {
    return "Crop";
  }
  if (qobject_cast<const ReconstructionOperator*>(op)) {
    return "CxxReconstruction";
  }
  if (qobject_cast<const SetTiltAnglesOperator*>(op)) {
    return "SetTiltAngles";
  }
  if (qobject_cast<const TranslateAlignOperator*>(op)) {
    return "TranslateAlign";
  }
  if (qobject_cast<TransposeDataOperator*>(op)) {
    return "TransposeData";
  }
  if (qobject_cast<SnapshotOperator*>(op)) {
    return "Snapshot";
  }
  return nullptr;
}
} // namespace tomviz
