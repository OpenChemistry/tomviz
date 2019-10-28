/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizH5ReadWrite_h
#define tomvizH5ReadWrite_h

#include <memory>
#include <string>
#include <vector>

namespace h5 {

class H5ReadWrite
{
public:
  /**
   * Enumeration of the open modes.
   */
  enum class OpenMode
  {
    ReadOnly,
    WriteOnly
  };

  /**
   * Open an HDF5 file for reading.
   * @param fileName the file to open for reading.
   */
  explicit H5ReadWrite(const std::string& fileName,
                       OpenMode mode = OpenMode::ReadOnly);

  /** Closes the file and destroys the H5ReadWrite */
  ~H5ReadWrite();

  /** Explicitly close the file if one is open */
  void close();

  /** Copy constructor is disabled */
  H5ReadWrite(const H5ReadWrite&) = delete;

  /** Assignment operator is disabled */
  H5ReadWrite& operator=(const H5ReadWrite&) = delete;

  /** Enumeration of the data types */
  enum class DataType
  {
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    String,
    None = -1
  };

  /** Get a string representation of the enum DataType */
  static std::string dataTypeToString(const DataType& type);

  /**
   * Get the children of a path.
   * @param ok If used, set to true on success and false on failure.
   * @return A vector of the names of the children.
   */
  std::vector<std::string> children(const std::string& path,
                                    bool* ok = nullptr);

  /**
   * Check if a given path has at least one attribute.
   * @param path The path to the attribute.
   * @return True if the path has an attribute, false if it does not,
   *         or if an error occurred.
   */
  bool hasAttribute(const std::string& path);

  /**
   * Check if a given path has an attribute with a given name.
   * @param path The path to the attribute.
   * @param name The name of the attribute.
   * @return True if the path has the attribute, false if it does not,
   *         or if an error occurred.
   */
  bool hasAttribute(const std::string& path, const std::string& name);

  /**
   * Get an attribute's type.
   * @param path The path to the attribute.
   * @param name The name of the attribute.
   * @return The datatype of the attribute, or DataType::None on failure.
   */
  DataType attributeType(const std::string& path, const std::string& name);

  /**
   * Read an attribute and interpret it as type T. If T is not
   * the correct type of the attribute, an error will occur.
   * @param path The path to the attribute.
   * @param name The name of the attribute.
   * @ok If used, set to true on success and false on failure.
   * @return The attribute.
   */
  template <typename T>
  T attribute(const std::string& path, const std::string& name,
              bool* ok = nullptr);

  /**
   * Check if a given path is a data set.
   * @return True if the path is a data set, false if it is not, or if
   *         an error occurred.
   */
  bool isDataSet(const std::string& path);

  /**
   * Check if the given path is a group.
   * @return True if the path is a group, false if it is not, or if
   *              an error occurred.
   */
  bool isGroup(const std::string& path);

  /**
   * Get the paths to all of the data sets in the file, or under a
   * specified path.
   * @param path The path for which to get the data sets. Defaults
   *             to getting the paths for the whole file.
   * @return A vector of strings of the paths to the data sets.
   */
  std::vector<std::string> allDataSets(const std::string& path = "");

  /**
   * Get a data set's type. An error will occur if @p path is not a dataset.
   * @param path The path to the data set.
   * @return The datatype of the attribute, or DataType::None on failure.
   */
  DataType dataType(const std::string& path);

  /**
   * Get the number of dimensions of a data set.
   * @param path The path to the data set.
   * @return The number of dimensions, or a negative number on failure.
   */
  int dimensionCount(const std::string& path);

  /**
   * Get the dimensions of a data set.
   * @param path The path to the data set.
   * @return A vector of the dimensions, or an empty vector on failure.
   */
  std::vector<int> getDimensions(const std::string& path);

  /**
   * Read a 1-dimensional data set and interpret it as type T. If @p path
   * is not a data set, @p path is not a 1-dimensional data set, or T is
   * not the correct type of the data set, an error will occur.
   * @param path The path to the data set.
   * @return A vector of the data, or an empty vector on failure.
   */
  template <typename T>
  std::vector<T> readData(const std::string& path);

  /**
   * Read a multi-dimensional data set and interpret it as type T. If
   * @p path is not a data set, or T is not the correct type of the
   * data set, an error will occur.
   * @param path The path to the data set.
   * @param dimensions Will be set to the dimensions of the data set.
   * @return A vector of the data, or an empty vector on failure.
   */
  template <typename T>
  std::vector<T> readData(const std::string& path,
                          std::vector<int>& dimensions);

  /**
   * Read a multi-dimensional data set and interpret it as type T. If
   * @p path is not a data set, or T is not the correct type of the
   * data set, an error will occur.
   * @param path The path to the data set.
   * @param data A pointer to a block of memory with a size large enough
   *             to hold the data (size >= dim1 * dim2 * dim3...). This
   *             will be set to the data read from the data set.
   * @return True on success, false on failure.
   */
  template <typename T>
  bool readData(const std::string& path, T* data);

  /**
   * Read a multi-dimensional data set and itnerpret it as type @p type.
   * If @p path is not a data set, or @p type is not the correct type
   * of the data set, an error will occur.
   * @param path The path to the data set.
   * @param type The type of the data set.
   * @param data A pointer to a block of memory with a size large enough
   *             to hold the data (size >= dim1 * dim2 * dim3...). This
   *             will be set to the data read from the data set.
   * @param strides The strides that will be applied when reading the
   *                data. If used, ensure that the dimensions of @p data
   *                are all divided by the strides with integer division
   *                (i. e., dims[i] /= strides[i]).
   * @param start The start of the block of data to be read. If used,
   *              must have a length of ndims. If unspecified, the
   *              origin will be used.
   * @param counts The number of data elements to be read. If used,
   *               must have a length of ndims. If unspecified,
   *               as much data will be read as possible.
   * @return True on success, false on failure.
   */
  bool readData(const std::string& path, const DataType& type, void* data,
                int* strides = nullptr, size_t* start = nullptr,
                size_t* counts = nullptr);

  /**
   * Write data to a specified path.
   * @param path The path where the data will be written.
   * @param name The name of the data.
   * @param dimensions The dimensions of the data.
   * @param data The data to write.
   * @return True on success, false on failure.
   */
  template <typename T>
  bool writeData(const std::string& path, const std::string& name,
                 const std::vector<int>& dimensions,
                 const std::vector<T>& data);

  /**
   * Write data to a specified path.
   * @param path The path where the data will be written.
   * @param name The name of the data.
   * @param dimensions The dimensions of the data.
   * @param data The data to write.
   * @return True on success, false on failure.
   */
  template <typename T>
  bool writeData(const std::string& path, const std::string& name,
                 const std::vector<int>& dimensions, const T* data);

  /**
   * Write data to a specified path.
   * @param path The path where the data will be written.
   * @param name The name of the data.
   * @param dimensions The dimensions of the data.
   * @param type The type of data to write.
   * @param data The data to write.
   * @return True on success, false on failure.
   */
  bool writeData(const std::string& path, const std::string& name,
                 const std::vector<int>& dimensions, const DataType& type,
                 const void* data);

  /**
   * Set an attribute on a specified path.
   * @param path The path where the attribute will be written.
   * @param name The name of the attribute.
   * @param value The value of the attribute.
   * @return True on success, false on failure.
   */
  template <typename T>
  bool setAttribute(const std::string& path, const std::string& name, T value);

  /**
   * Create a group.
   * @param path The path to the group that will be created.
   * @return True on success, false on failure
   */
  bool createGroup(const std::string& path);

private:
  class H5ReadWriteImpl;
  std::unique_ptr<H5ReadWriteImpl> m_impl;
};

template <typename T>
bool H5ReadWrite::writeData(const std::string& path, const std::string& name,
                            const std::vector<int>& dims,
                            const std::vector<T>& data)
{
  return writeData(path, name, dims, data.data());
}

} // namespace h5

#endif // tomvizH5ReadWrite_h
