/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizEmdFormat_h
#define tomvizEmdFormat_h

#include <string>

#include <QVariantMap>

class QJsonObject;
class vtkImageData;

namespace h5 {
class H5ReadWrite;
}

namespace tomviz {

class DataSource;
class Operator;

class EmdFormat
{
public:
  bool read(const std::string& fileName, vtkImageData* data,
            const QVariantMap& options = QVariantMap());
  bool write(const std::string& fileName, DataSource* source);
  bool write(const std::string& fileName, vtkImageData* image);

  // Write the full state of tomviz into our custom EMD format,
  // .tvh5 files.
  bool writeFullState(const std::string& fileName);
  bool readFullState(const std::string& fileName);
private:
  // Read the EMD data from a specified node in the HDF5 file
  bool readNode(h5::H5ReadWrite& reader, const std::string& path,
                vtkImageData* image,
                const QVariantMap& options = QVariantMap());
  // Write the EMD data to a specified node in the HDF5 file
  bool writeNode(h5::H5ReadWrite& writer, const std::string& path,
                 vtkImageData* image);

  // Load a data source from data in an EMD file
  // If the active data source is found, it is set to @param active
  bool loadDataSource(h5::H5ReadWrite& reader, const QJsonObject& dsObject,
                      DataSource** active, Operator* parent = nullptr);
};
} // namespace tomviz

#endif // tomvizEmdFormat_h
