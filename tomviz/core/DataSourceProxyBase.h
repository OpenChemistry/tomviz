/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataSourceProxyBase_h
#define tomvizDataSourceProxyBase_h

#include "tomvizcore_export.h"

class vtkImageData;

namespace tomviz {

/** Pure virtual base class providing a proxy to the DataSource class. */
class TOMVIZCORE_EXPORT DataSourceProxyBase
{
public:
  virtual ~DataSourceProxyBase() = default;

  virtual vtkImageData* darkData() = 0;

  virtual vtkImageData* whiteData() = 0;
};

} // namespace tomviz

#endif