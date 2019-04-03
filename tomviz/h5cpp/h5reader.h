/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizH5Reader_h
#define tomvizH5Reader_h

#include <memory>
#include <string>
#include <vector>

namespace tomviz {

class H5Reader {
public:
  // Opens the file
  explicit H5Reader(const std::string& filename);
  // If a file is open, automatically closes the file
  ~H5Reader();

  H5Reader(const H5Reader&) = delete;
  H5Reader& operator=(const H5Reader&) = delete;

  // These are for getting the types
  enum class DataType {
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
    String
  };

  // Get a string representation of the DataType enum
  static std::string dataTypeToString(const DataType& type);

  // Get the children of a path
  bool children(const std::string& path, std::vector<std::string>& result);

  // Get the type of the attribute
  // Returns false if the attribute does not exist
  bool attributeType(const std::string& group, const std::string& name,
                     DataType& type);

  // Read an attribute and interpret it as type T
  // Returns false if type T does not match the type of the attribute
  template <typename T>
  bool attribute(const std::string& group, const std::string& name, T& value);

  // Get the type of the data
  // Returns false if the data does not exist
  bool dataType(const std::string& path, DataType& type);

  // Get the number of dimensions of the data
  // Returns false if the data does not exist
  bool numDims(const std::string& path, int& nDims);

  // Get the dimensions of the data
  // Returns false if an error occurs
  bool getDims(const std::string& path, std::vector<int>& dims);

  // Read 1-dimensional data and interpret it as type T
  // Returns false if type T does not match the type of the data,
  // or if the data is multi-dimensional
  template <typename T>
  bool readData(const std::string& path, std::vector<T>& data);

  // Read 2-dimensional data and interpret it as type T
  // Returns false if type T does not match the type of the data
  template <typename T>
  bool readData(const std::string& path, std::vector<std::vector<T>>& data);

  // Read multi-dimensional data and interpret it as type T.
  // A vector containing the multi-dimensional data is returned, along
  // with a vector containing the dimensions.
  // The caller can use the dimensions to put the results in an object
  // of a different shape, if desired.
  // Returns false if an error occurs.
  template <typename T>
  bool readData(const std::string& path, std::vector<T>& data,
                std::vector<int>& dimensions);

private:
  class H5ReaderImpl;
  std::unique_ptr<H5ReaderImpl> m_impl;
};

} // namespace tomviz

#endif // tomvizH5Reader_h
