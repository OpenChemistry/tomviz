/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorFactory.h"

#include "ArrayWranglerOperator.h"
#include "ConvertToFloatOperator.h"
#include "ConvertToVolumeOperator.h"
#include "CropOperator.h"
#include "OperatorPython.h"
#include "ReconstructionOperator.h"
#include "SetTiltAnglesOperator.h"
#include "SnapshotOperator.h"
#include "TranslateAlignOperator.h"
#include "TransposeDataOperator.h"

namespace tomviz {

OperatorFactory::OperatorFactory() = default;

OperatorFactory::~OperatorFactory() = default;

QList<QString> OperatorFactory::operatorTypes()
{
  QList<QString> reply;
  reply << "ArrayWrangler"
        << "ConvertToFloat"
        << "ConvertToVolume"
        << "Crop"
        << "CxxReconstruction"
        << "Python"
        << "SetTiltAngles"
        << "Snapshot"
        << "TranslateAlign"
        << "TransposeData";
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
    op = new OperatorPython(ds);
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
  if (qobject_cast<const ArrayWranglerOperator*>(op)) {
    return "ArrayWrangler";
  }
  if (qobject_cast<const ConvertToFloatOperator*>(op)) {
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
  if (qobject_cast<const TransposeDataOperator*>(op)) {
    return "TransposeData";
  }
  if (qobject_cast<const SnapshotOperator*>(op)) {
    return "Snapshot";
  }
  return nullptr;
}
} // namespace tomviz
