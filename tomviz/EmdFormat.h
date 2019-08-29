/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizEmdFormat_h
#define tomvizEmdFormat_h

#include <string>

#include <QVariantMap>

class vtkImageData;

namespace tomviz {

class DataSource;

class EmdFormat
{
public:
  bool read(const std::string& fileName, vtkImageData* data,
            const QVariantMap& options = QVariantMap());
  bool write(const std::string& fileName, DataSource* source);
  bool write(const std::string& fileName, vtkImageData* image);
};
} // namespace tomviz

#endif // tomvizEmdFormat_h
