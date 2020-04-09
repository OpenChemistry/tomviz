/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataSourceBase_h
#define tomvizDataSourceBase_h

#include "tomvizcore_export.h"

class vtkImageData;

namespace tomviz {

/** Pure virtual base class providing a proxy to the DataSource class. */
class TOMVIZCORE_EXPORT DataSourceBase
{
public:
  void setDarkData(vtkImageData* data) { m_dark = data; }

  vtkImageData* darkData() const { return m_dark; }

  void setWhiteData(vtkImageData* data) { m_white = data; }

  vtkImageData* whiteData() const { return m_white; }

private:
  vtkImageData* m_dark = nullptr;
  vtkImageData* m_white = nullptr;
};

} // namespace tomviz

#endif