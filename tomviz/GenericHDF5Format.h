/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizGenericHDF5Format_h
#define tomvizGenericHDF5Format_h

#include <string>

#include <QVariantMap>
#include <QVector>

class vtkImageData;

namespace h5 {
class H5ReadWrite;
}

namespace tomviz {

class GenericHDF5Format
{
public:
  // Check to see if the file looks like a data exchange file
  static bool isDataExchange(const std::string& fileName);

  // Check if the file looks like an FXI data set
  static bool isFxi(const std::string& fileName);

  static bool read(const std::string& fileName, vtkImageData* data,
                   const QVariantMap& options = QVariantMap());

  /**
   * Read angles from a path and return the angles. The dataset to be
   * read must have exactly one dimension.
   *
   * @param reader A reader that has already opened the file of interest.
   * @param path The path to the angles dataset in the HDF5 file.
   * @param options The options for reading the angles.
   * @return The angles, or an empty vector if an error occurred.
   */
  static QVector<double> readAngles(h5::H5ReadWrite& reader,
                                    const std::string& path,
                                    const QVariantMap& options = QVariantMap());

  /**
   * Read a volume and write it to a vtkImageData object. This assumes
   * that the volume is stored in the HDF5 file in row-major order, and
   * it will convert it to column major order for VTK.
   *
   * @param reader A reader that has already opened the file of interest.
   * @param path The path to the volume in the HDF5 file.
   * @param data The vtkImageData where the volume will be written.
   * @param options The options for reading the image data.
   * @return True on success, false on failure.
   */
  static bool readVolume(h5::H5ReadWrite& reader, const std::string& path,
                         vtkImageData* data,
                         const QVariantMap& options = QVariantMap());

  /**
   * Add a dataset as a scalar array to pre-existing image data. The
   * dataset must have the same dimensions as the pre-existing image
   * data.
   *
   * If the original image was read using subsampling, the dataset to
   * be added will be read using the same subsampling.
   *
   * @param reader A reader that has already opened the file of interest.
   * @param path The path to the dataset to add as a scalar array.
   * @param image The vtkImageData where the scalar array will be added.
   * @param name The name to give to the scalar array.
   * @return True on success, false on failure.
   */
  static bool addScalarArray(h5::H5ReadWrite& reader, const std::string& path,
                             vtkImageData* image, const std::string& name);

  /**
   * Write a volume from a vtkImageData object to a path. This converts
   * the vtkImageData to row-major order before writing.
   *
   * @param writer The writer that has already opened the file of interest.
   * @param path The path to the group where the data will be written.
   * @param name The name that the dataset will be given.
   * @param image The vtkImageData from which the volume will be written.
   * @return True on success, false on failure.
   */
  static bool writeVolume(h5::H5ReadWrite& writer, const std::string& path,
                          const std::string& name, vtkImageData* image);

  /**
   * Swap the X and Z axes for all scalars in the vtkImageData.
   */
  static void swapXAndZAxes(vtkImageData* image);
};
} // namespace tomviz

#endif // tomvizGenericHDF5Format_h
