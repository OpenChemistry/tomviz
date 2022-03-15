/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataSourceBase_h
#define tomvizDataSourceBase_h

#include "Variant.h"

class vtkImageData;

namespace tomviz {

using MetadataType = std::map<std::string, Variant>;

/** Pure virtual base class providing a proxy to the DataSource class. */
class DataSourceBase
{
public:
  void setDarkData(vtkImageData* data) { m_dark = data; }
  vtkImageData* darkData() const { return m_dark; }

  void setWhiteData(vtkImageData* data) { m_white = data; }
  vtkImageData* whiteData() const { return m_white; }

  void setFileName(const std::string& name) { m_fileName = name; }
  std::string fileName() const { return m_fileName; }

  void setMetadata(const MetadataType& meta) { m_metadata = meta; }
  MetadataType metadata() const { return m_metadata; }

private:
  vtkImageData* m_dark = nullptr;
  vtkImageData* m_white = nullptr;
  std::string m_fileName;
  MetadataType m_metadata;
};

} // namespace tomviz

#endif
