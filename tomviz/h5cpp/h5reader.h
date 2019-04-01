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

  // These are for getting the types
  enum class DataType {
    Int,
    UInt8,
    UInt16,
    UInt32,
    Float,
    Double,
    String
    // More to be added later...
  };

  // Get the children of a path
  std::vector<std::string> children(const std::string& path);

  // Get the type of the attribute
  // Returns false if the attribute does not exist
  bool attributeType(const std::string& group, const std::string& name, DataType& type);

  // Read an attribute and interpret it as type T
  // Returns false if type T does not match the type of the attribute
  template <typename T>
  bool attribute(const std::string& group, const std::string& name, T& value);

  // Get the type of the data
  // Returns false if the data does not exist
  bool dataType(const std::string& path, DataType& type);

  // Get the number of dimensions of the data
  // Returns false if the data does not exist
  bool numDimensions(const std::string& path, int& numDims);

  // Read 1-dimensional data and interpret it as type T
  // Returns false if type T does not match the type of the data,
  // or if the data is multi-dimensional
  template <typename T>
  bool readData(const std::string& path, std::vector<T>& data);

  // Read multi-dimensional data and interpret it as type T
  // Returns false if type T does not match the type of the data
  template <typename T>
  bool readData(const std::string& path, std::vector<std::vector<T>>& data);

private:
  class H5ReaderImpl;
  std::unique_ptr<H5ReaderImpl> m_impl;
};

} // namespace tomviz

#endif // tomvizEmdFormat_h
