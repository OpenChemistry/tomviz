/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizGenericHDF5Format_h
#define tomvizGenericHDF5Format_h

#include <string>

class vtkImageData;

namespace h5 {
class H5ReadWrite;
}

namespace tomviz {

class GenericHDF5Format
{
public:
  // Some options to set before reading

  // Should we check the size and ask the user to stride?
  void setCheckSize(bool b) { m_checkSize = b; }

  // What should the stride be?
  void setStride(int i) { m_stride = i; }

  bool read(const std::string& fileName, vtkImageData* data);

  /**
   * Read a volume and write it to a vtkImageData object. This assumes
   * that the volume is stored in the HDF5 file in row-major order, and
   * it will convert it to column major order for VTK.
   *
   * @param reader A reader that has already opened the file of interest.
   * @param path The path to the volume in the HDF5 file.
   * @param data The vtkImageData where the volume will be written.
   * @return True on success, false on failure.
   */
  bool readVolume(h5::H5ReadWrite& reader, const std::string& path,
                  vtkImageData* data);
private:
  // Check to see if the size is very big, and if it is, allow the user
  // to choose a stride.
  bool m_checkSize = true;

  // Stride to use, unless the user chooses one.
  int m_stride = 1;
};
} // namespace tomviz

#endif // tomvizGenericHDF5Format_h
