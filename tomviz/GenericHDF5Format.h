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
  // Should we ask the user to pick a subsample? False by default.
  void setAskForSubsample(bool b) { m_askForSubsample = b; }

  // If any dimensions are greater than this number, the user will
  // be asked to pick a subsample, even if m_askForSubsample is false.
  // 1200 by default.
  void setSubsampleDimOverride(int i) { m_subsampleDimOverride = i; }

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
  // Should we ask the user to pick a subsample?
  bool m_askForSubsample = false;

  // If any dimensions are greater than this number, ask the user to
  // pick a subsample, even if m_askForSubsample is false.
  int m_subsampleDimOverride = 1200;
};
} // namespace tomviz

#endif // tomvizGenericHDF5Format_h
