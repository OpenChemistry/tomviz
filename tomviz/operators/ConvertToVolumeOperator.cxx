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
  DataSource::setType(data, m_type);
  return true;
}

}
