/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizEmdFormat_h
#define tomvizEmdFormat_h

#include <string>

#include <QVariantMap>

class vtkImageData;

namespace h5 {
class H5ReadWrite;
}

namespace tomviz {

class EmdFormat
{
public:
  static bool read(const std::string& fileName, vtkImageData* data,
                   const QVariantMap& options = QVariantMap());
  // Whether the HDF5 file has an EMD node to read (other formats, such as
  // Thermo Fisher Velox, also use the extension .emd)
  static bool isEmd(const std::string& fileName);
  static bool write(const std::string& fileName, vtkImageData* image);

  // Read EMD data from a specified node in the HDF5 file
  static bool readNode(const std::string& fileName, const std::string& path,
                       vtkImageData* image,
                       const QVariantMap& options = QVariantMap());
  static bool readNode(h5::H5ReadWrite& reader, const std::string& path,
                       vtkImageData* image,
                       const QVariantMap& options = QVariantMap());
  // Write EMD data to a specified node in the HDF5 file
  static bool writeNode(h5::H5ReadWrite& writer, const std::string& path,
                        vtkImageData* image);
};
} // namespace tomviz

#endif // tomvizEmdFormat_h
