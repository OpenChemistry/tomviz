/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConvertToVolumeOperator.h"

#include <vtkDataObject.h>
#include <vtkFieldData.h>
#include <vtkTypeInt8Array.h>

namespace tomviz
{

ConvertToVolumeOperator::ConvertToVolumeOperator(QObject* p,
                                                 DataSource::DataSourceType t,
                                                 QString label)
  : Operator(p), m_type(t), m_label(label)
{
}

ConvertToVolumeOperator::~ConvertToVolumeOperator() = default;

QIcon ConvertToVolumeOperator::icon() const
{
  return QIcon();
}

Operator* ConvertToVolumeOperator::clone() const
{
  return new ConvertToVolumeOperator;
}

bool ConvertToVolumeOperator::applyTransform(vtkDataObject* data)
{
  // The array should already exist.
  auto fd = data->GetFieldData();
  // Make sure the data is marked as a tilt series
  auto dataType =
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

}