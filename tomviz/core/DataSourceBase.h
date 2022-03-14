/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataSourceBase_h
#define tomvizDataSourceBase_h

#include "DataSource.h"

namespace tomviz {

/** Pure virtual base class providing a proxy to the DataSource class. */
class DataSourceBase
{
public:
  DataSourceBase(DataSource* ds) : m_dataSource(ds) {}

  vtkImageData* darkData() const { return m_dataSource->darkData(); }
  vtkImageData* whiteData() const { return m_dataSource->whiteData(); }
  QString fileName() const { return m_dataSource->fileName(); }
  MetadataType metadata() const { return m_dataSource->metadata(); }

private:
  DataSource* m_dataSource = nullptr;
};

} // namespace tomviz

#endif
