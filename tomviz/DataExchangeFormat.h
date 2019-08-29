/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataExchangeFormat_h
#define tomvizDataExchangeFormat_h

#include <string>

#include <QVariantMap>

class vtkImageData;

namespace tomviz {

class DataSource;

class DataExchangeFormat
{
public:
  bool read(const std::string& fileName, vtkImageData* data,
            const QVariantMap& options = QVariantMap());
  bool write(const std::string& fileName, DataSource* source);
  bool write(const std::string& fileName, vtkImageData* image);
};
} // namespace tomviz

#endif // tomvizDataExchangeFormat_h
