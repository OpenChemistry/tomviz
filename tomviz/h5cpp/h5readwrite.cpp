/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "h5readwrite.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <numeric>

#include "h5capi.h"
#include "h5typemaps.h"
#include "hidcloser.h"

using std::cerr;
using std::cout;
using std::endl;

using std::map;
using std::string;
using std::vector;

namespace {

// Just a convenience function. Only sets "ok" if it is not nullptr.
void setOk(bool* ok, bool status)
{
  if (ok)
    *ok = status;
}

} // end namespace

namespace h5 {

using DataType = H5ReadWrite::DataType;

class ListAllDataSetsVisitor
{
public:
  vector<string> dataSets;
  static herr_t operation(hid_t /*o_id*/, const char* name,
                          const H5O_info_t* object_info, void* op_data)
  {
    // If this object isn't a dataset, continue
    if (object_info->type != H5O_TYPE_DATASET)
      return 0;

    auto* self = reinterpret_cast<ListAllDataSetsVisitor*>(op_data);
    self->dataSets.push_back(name);
    return 0;
  }
};

class H5ReadWrite::H5ReadWriteImpl
{
public:
  H5ReadWriteImpl() {}

  H5ReadWriteImpl(const string& file, OpenMode mode)
  {
    if (mode == OpenMode::ReadOnly) {
      if (!openFile(file))
        cerr << "Warning: failed to open file " << file << "\n";
    } else if (mode == OpenMode::WriteOnly) {
      if (!createFile(file))
        cerr << "Warning: failed to create file " << file << "\n";
    } else {
      cerr << "Warning: open mode currently not implemented.\n";
    }
  }

  ~H5ReadWriteImpl() { clear(); }

  bool openFile(const string& file)
  {
    m_fileId = H5Fopen(file.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    return fileIsValid();
  }

  bool createFile(const string& file)
  {
    m_fileId = H5Fcreate(file.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    return fileIsValid();
  }

  bool attributeExists(const string& path, const string& name)
  {
    if (!fileIsValid())
      return false;

    return H5Aexists_by_name(m_fileId, path.c_str(), name.c_str(),
                             H5P_DEFAULT) > 0;
  }

  bool hasAttribute(const string& path)
  {
    H5O_info_t info;
    if (!getInfoByName(path, info)) {
      cerr << "Failed to get info by name\n";
      return false;
    }

    return info.num_attrs > 0;
  }

  bool attribute(const string& path, const string& name, void* value,
                 hid_t dataTypeId, hid_t memTypeId)
  {
    if (!attributeExists(path, name)) {
      cerr << "Attribute " << path << name << " not found!" << endl;
      return false;
    }

    hid_t attr = H5Aopen_by_name(m_fileId, path.c_str(), name.c_str(),
                                 H5P_DEFAULT, H5P_DEFAULT);
    hid_t type = H5Aget_type(attr);

    // For automatic closing upon leaving scope
    HIDCloser attrCloser(attr, H5Aclose);
    HIDCloser typeCloser(type, H5Tclose);

    if (H5Tequal(type, dataTypeId) == 0) {
      // The type of the attribute does not match the requested type.
      cerr << "Type determined does not match that requested." << endl;
      cerr << type << " -> " << dataTypeId << endl;
      return false;
    } else if (H5Tequal(type, dataTypeId) < 0) {
      cerr << "Something went really wrong....\n\n";
      return false;
    }

    return H5Aread(attr, memTypeId, value) >= 0;
  }

  bool setAttribute(const string& path, const string& name, void* value,
                    hid_t fileTypeId, hid_t typeId, hsize_t dims)
  {
    if (!fileIsValid()) {
      cerr << "File is not valid\n";
      return false;
    }

    bool onData = isDataSet(path);

    hid_t parentId;
    herr_t (*closer)(hid_t) = nullptr;
    if (onData) {
      parentId = H5Dopen(m_fileId, path.c_str(), H5P_DEFAULT);
      closer = H5Dclose;
    } else {
      parentId = H5Gopen(m_fileId, path.c_str(), H5P_DEFAULT);
      closer = H5Gclose;
    }

    HIDCloser parentCloser(parentId, closer);

    hid_t dataspaceId = H5Screate_simple(1, &dims, nullptr);
    hid_t attributeId = H5Acreate2(parentId, name.c_str(), fileTypeId,
                                   dataspaceId, H5P_DEFAULT, H5P_DEFAULT);

    HIDCloser dataspaceCloser(dataspaceId, H5Sclose);
    HIDCloser attributeCloser(attributeId, H5Aclose);

    return H5Awrite(attributeId, typeId, value) >= 0;
  }

  bool writeData(const string& path, const string& name,
                 const std::vector<int>& dims, const void* data,
                 hid_t dataTypeId, hid_t memTypeId)
  {
    if (!fileIsValid()) {
      cerr << "File is invalid\n";
      return false;
    }

    std::vector<hsize_t> h5dim;
    for (size_t i = 0; i < dims.size(); ++i) {
      h5dim.push_back(static_cast<hsize_t>(dims[i]));
    }
    hid_t groupId = H5Gopen(m_fileId, path.c_str(), H5P_DEFAULT);
    hid_t dataSpaceId =
      H5Screate_simple(static_cast<int>(dims.size()), &h5dim[0], nullptr);
    hid_t dataId = H5Dcreate(groupId, name.c_str(), dataTypeId, dataSpaceId,
                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    HIDCloser groupCloser(groupId, H5Gclose);
    HIDCloser spaceCloser(dataSpaceId, H5Sclose);
    HIDCloser dataCloser(dataId, H5Dclose);

    hid_t status =
      H5Dwrite(dataId, memTypeId, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    return status >= 0;
  }

  vector<int> getDimensions(const string& path)
  {
    vector<int> result;
    if (!isDataSet(path)) {
      cerr << path << " is not a data set.\n";
      return result;
    }

    hid_t dataSetId = H5Dopen(fileId(), path.c_str(), H5P_DEFAULT);
    if (dataSetId < 0) {
      cerr << "Failed to get dataSetId\n";
      return result;
    }

    // Automatically close upon leaving scope
    HIDCloser dataSetCloser(dataSetId, H5Dclose);

    hid_t dataSpaceId = H5Dget_space(dataSetId);
    if (dataSpaceId < 0) {
      cerr << "Failed to get dataSpaceId\n";
      return result;
    }

    HIDCloser dataSpaceCloser(dataSpaceId, H5Sclose);

    int dimCount = H5Sget_simple_extent_ndims(dataSpaceId);
    if (dimCount < 1) {
      cerr << "Error: number of dimensions is less than 1\n";
      return result;
    }

    hsize_t* h5dims = new hsize_t[dimCount];
    int dimCount2 = H5Sget_simple_extent_dims(dataSpaceId, h5dims, nullptr);

    if (dimCount != dimCount2) {
      cerr << "Error: dimCounts do not match\n";
      delete[] h5dims;
      return result;
    }

    result.resize(dimCount);
    std::copy(h5dims, h5dims + dimCount, result.begin());

    delete[] h5dims;

    return result;
  }

  // void* data needs to be of the appropiate type and size.
  // start and counts, if set, get forwarded directly to
  // H5Sselect_hyperslab().
  bool readData(const string& path, hid_t dataTypeId, hid_t memTypeId,
                void* data, int* strides = nullptr, size_t* start = nullptr,
                size_t* counts = nullptr)
  {
    hid_t dataSetId = H5Dopen(m_fileId, path.c_str(), H5P_DEFAULT);
    if (dataSetId < 0) {
      cerr << "Failed to get dataSetId\n";
      return false;
    }

    // Automatically close upon leaving scope
    HIDCloser dataSetCloser(dataSetId, H5Dclose);

    hid_t dataSpaceId = H5Dget_space(dataSetId);
    if (dataSpaceId < 0) {
      cerr << "Failed to get dataSpaceId\n";
      return false;
    }

    HIDCloser dataSpaceCloser(dataSpaceId, H5Sclose);

    hid_t memSpace = H5S_ALL;
    HIDCloser memSpaceCloser(-1, H5Sclose);
    // Select a hyperslab if needed
    if (strides || start || counts) {

      // First, get the dimensions
      vector<int> dims = getDimensions(path);
      size_t ndims = dims.size();

      // Set defaults if needed
      vector<hsize_t> stridesVector;
      if (strides)
        stridesVector = vector<hsize_t>(strides, strides + ndims);
      else
        stridesVector = vector<hsize_t>(ndims, 1);

      vector<hsize_t> startVector;
      if (start)
        startVector = vector<hsize_t>(start, start + ndims);
      else
        startVector = vector<hsize_t>(ndims, 0);

      vector<hsize_t> countsVector;
      if (counts) {
        countsVector = vector<hsize_t>(counts, counts + ndims);
      } else {
        countsVector.resize(ndims);
        for (size_t i = 0; i < countsVector.size(); ++i)
          countsVector[i] = (dims[i] - startVector[i]) / stridesVector[i];
      }

      // Next, select the hyperslab
      H5Sselect_hyperslab(dataSpaceId, H5S_SELECT_SET, startVector.data(),
                          stridesVector.data(), countsVector.data(), nullptr);

      // Finally, create the mem space
      memSpace = H5Screate_simple(ndims, countsVector.data(), nullptr);
      memSpaceCloser.reset(memSpace);
    }

    hid_t typeId = H5Dget_type(dataSetId);
    HIDCloser dataTypeCloser(typeId, H5Tclose);

    if (H5Tequal(typeId, dataTypeId) == 0) {
      // The type of the data does not match the requested type.
      cerr << "Type determined does not match that requested." << endl;
      cerr << typeId << " -> " << dataTypeId << endl;
      return false;
    } else if (H5Tequal(typeId, dataTypeId) < 0) {
      cerr << "Something went really wrong....\n\n";
      return false;
    }

    return H5Dread(dataSetId, memTypeId, memSpace, dataSpaceId, H5P_DEFAULT,
                   data) >= 0;
  }

  bool getInfoByName(const string& path, H5O_info_t& info)
  {
    if (!fileIsValid())
      return false;

    return H5Oget_info_by_name(m_fileId, path.c_str(), &info, H5P_DEFAULT) >= 0;
  }

  bool isDataSet(const string& path)
  {
    // It's okay if some of these functions fail, turn off errors
    turnOffErrors();

    H5O_info_t info;
    if (!getInfoByName(path, info)) {
      turnOnErrors();
      return false;
    }

    turnOnErrors();
    return info.type == H5O_TYPE_DATASET;
  }

  bool isGroup(const string& path)
  {
    // It's okay if some of these functions fail, turn off errors
    turnOffErrors();

    H5O_info_t info;
    if (!getInfoByName(path, info)) {
      turnOnErrors();
      return false;
    }

    turnOnErrors();
    return info.type == H5O_TYPE_GROUP;
  }

  vector<string> allDataSets(const string& path)
  {
    if (!fileIsValid())
      return vector<string>();

    hid_t objectId;
    HIDCloser groupCloser(-1, H5Gclose);
    if (path.empty() || path == "/") {
      objectId = fileId();
    } else {
      objectId = H5Gopen(fileId(), path.c_str(), H5P_DEFAULT);
      groupCloser.reset(objectId);
    }

    ListAllDataSetsVisitor visitor;
    herr_t code = H5Ovisit(objectId, H5_INDEX_NAME, H5_ITER_INC,
                           &visitor.operation, &visitor);

    if (code < 0)
      return vector<string>();

    return visitor.dataSets;
  }

  DataType getH5ToDataType(hid_t h5type)
  {
    // Find the type
    auto it = std::find_if(H5ToDataType.cbegin(), H5ToDataType.cend(),
                           [h5type](const std::pair<hid_t, DataType>& t) {
                             return H5Tequal(t.first, h5type);
                           });

    if (it == H5ToDataType.end()) {
      cerr << "H5ToDataType map does not contain H5 type: " << h5type << endl;
      return DataType::None;
    }

    return it->second;
  }

  // Turn off error messages and save the handlers and data
  // in member variables
  void turnOffErrors()
  {
    if (m_errorHandlingIsOff)
      return;

    H5Eget_auto2(H5E_DEFAULT, &m_errorHandler, &m_clientErrorData);
    H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
    m_errorHandlingIsOff = true;
  }

  // Turn back on error messages using the saved handlers
  // and data
  void turnOnErrors()
  {
    if (!m_errorHandlingIsOff)
      return;

    H5Eset_auto2(H5E_DEFAULT, m_errorHandler, m_clientErrorData);
    m_errorHandler = nullptr;
    m_clientErrorData = nullptr;
    m_errorHandlingIsOff = false;
  }

  bool fileIsValid() { return m_fileId >= 0; }

  void clear()
  {
    if (fileIsValid()) {
      H5Fclose(m_fileId);
      m_fileId = H5I_INVALID_HID;
    }
  }

  hid_t fileId() const { return m_fileId; }

  hid_t m_fileId = H5I_INVALID_HID;

  // The error handlers are saved when error handling is turned off,
  // and they will be restored when error handling is turned back on.
  bool m_errorHandlingIsOff = false;
  H5E_auto_t m_errorHandler;
  void* m_clientErrorData;
};

H5ReadWrite::H5ReadWrite(const string& file, OpenMode mode)
  : m_impl(new H5ReadWriteImpl(file, mode))
{}

H5ReadWrite::~H5ReadWrite() = default;

void H5ReadWrite::close()
{
  m_impl->clear();
}

vector<string> H5ReadWrite::children(const string& path, bool* ok)
{
  setOk(ok, false);
  vector<string> result;

  if (!m_impl->fileIsValid())
    return result;

  constexpr int maxNameSize = 2048;
  char groupName[maxNameSize];

  hid_t groupId = H5Gopen(m_impl->fileId(), path.c_str(), H5P_DEFAULT);
  if (groupId < 0) {
    cerr << "Failed to open group: " << path << "\n";
    return result;
  }

  // For automatic closing upon leaving scope
  HIDCloser groupCloser(groupId, H5Gclose);

  hsize_t objCount = 0;
  H5Gget_num_objs(groupId, &objCount);

  for (hsize_t i = 0; i < objCount; ++i) {
    H5Gget_objname_by_idx(groupId, i, groupName, maxNameSize);
    result.push_back(groupName);
  }

  setOk(ok, true);
  return result;
}

template <typename T>
T H5ReadWrite::attribute(const string& path, const string& name, bool* ok)
{
  setOk(ok, false);
  T result;

  const hid_t dataTypeId = BasicTypeToH5<T>::dataTypeId();
  const hid_t memTypeId = BasicTypeToH5<T>::memTypeId();

  if (m_impl->attribute(path, name, &result, dataTypeId, memTypeId))
    setOk(ok, true);

  return result;
}

// We have a specialization for std::string
template <>
string H5ReadWrite::attribute<string>(const string& path, const string& name,
                                      bool* ok)
{
  setOk(ok, false);
  string result;

  if (!m_impl->attributeExists(path, name)) {
    cerr << "Attribute " << path << name << " not found!" << endl;
    return result;
  }

  hid_t fileId = m_impl->fileId();

  hid_t attr = H5Aopen_by_name(fileId, path.c_str(), name.c_str(), H5P_DEFAULT,
                               H5P_DEFAULT);
  hid_t type = H5Aget_type(attr);

  // For automatic closing upon leaving scope
  HIDCloser attrCloser(attr, H5Aclose);
  HIDCloser typeCloser(type, H5Tclose);

  if (H5T_STRING != H5Tget_class(type)) {
    cerr << path << name << " is not a string" << endl;
    return result;
  }
  char* tmpString;
  int is_var_str = H5Tis_variable_str(type);
  if (is_var_str > 0) { // if it is a variable-length string
    if (H5Aread(attr, type, &tmpString) < 0) {
      cerr << "Failed to read attribute " << path << " " << name << endl;
      return result;
    }
    result = tmpString;
    free(tmpString);
  } else if (is_var_str == 0) { // If it is not a variable-length string
    // it must be fixed length since the "is a string" check earlier passed.
    size_t size = H5Tget_size(type);
    if (size == 0) {
      cerr << "Unknown error occurred" << endl;
      return result;
    }
    tmpString = new char[size + 1];
    if (H5Aread(attr, type, tmpString) < 0) {
      cerr << "Failed to read attribute " << path << " " << name << endl;
      delete[] tmpString;
      return result;
    }
    tmpString[size] = '\0'; // set null byte, hdf5 doesn't do this for you
    result = tmpString;
    delete[] tmpString;
  } else {
    cerr << "Unknown error occurred" << endl;
    return result;
  }

  setOk(ok, true);
  return result;
}

bool H5ReadWrite::hasAttribute(const string& path)
{
  return m_impl->hasAttribute(path);
}

bool H5ReadWrite::hasAttribute(const string& path, const string& name)
{
  return m_impl->attributeExists(path, name);
}

DataType H5ReadWrite::attributeType(const string& path, const string& name)
{
  if (!m_impl->attributeExists(path, name)) {
    cerr << "Attribute " << path << name << " not found!" << endl;
    return DataType::None;
  }

  hid_t fileId = m_impl->fileId();
  hid_t attr = H5Aopen_by_name(fileId, path.c_str(), name.c_str(), H5P_DEFAULT,
                               H5P_DEFAULT);
  hid_t h5type = H5Aget_type(attr);

  // For automatic closing upon leaving scope
  HIDCloser attrCloser(attr, H5Aclose);
  HIDCloser typeCloser(h5type, H5Tclose);

  // Special case for strings
  if (H5T_STRING == H5Tget_class(h5type))
    return DataType::String;

  return m_impl->getH5ToDataType(h5type);
}

bool H5ReadWrite::isDataSet(const string& path)
{
  return m_impl->isDataSet(path);
}

bool H5ReadWrite::isGroup(const string& path)
{
  return m_impl->isGroup(path);
}

vector<string> H5ReadWrite::allDataSets(const string& path)
{
  return m_impl->allDataSets(path);
}

DataType H5ReadWrite::dataType(const string& path)
{
  if (!m_impl->isDataSet(path)) {
    cerr << path << " is not a data set.\n";
    return DataType::None;
  }

  hid_t dataSetId = H5Dopen(m_impl->fileId(), path.c_str(), H5P_DEFAULT);
  if (dataSetId < 0) {
    cerr << "Failed to get data set id\n";
    return DataType::None;
  }

  hid_t dataTypeId = H5Dget_type(dataSetId);

  // Automatically close
  HIDCloser dataSetCloser(dataSetId, H5Dclose);
  HIDCloser dataTypeCloser(dataTypeId, H5Tclose);

  return m_impl->getH5ToDataType(dataTypeId);
}

vector<int> H5ReadWrite::getDimensions(const string& path)
{
  return m_impl->getDimensions(path);
}

int H5ReadWrite::dimensionCount(const string& path)
{
  vector<int> dims = getDimensions(path);
  if (dims.empty()) {
    cerr << "Failed to get the dimensions\n";
    return -1;
  }

  return dims.size();
}

template <typename T>
vector<T> H5ReadWrite::readData(const string& path)
{
  vector<int> dims;
  vector<T> result = readData<T>(path, dims);
  if (result.empty()) {
    cerr << "Failed to read the data\n";
    return result;
  }

  // Make sure there is one dimension
  if (dims.size() != 1) {
    cerr << "Warning: single-dimensional readData() called, but "
         << "multi-dimensional data was obtained.\n";
    cerr << "Number of dims is: " << dims.size() << "\n";
    return vector<T>();
  }

  return result;
}

template <typename T>
vector<T> H5ReadWrite::readData(const string& path, vector<int>& dims)
{
  vector<T> result;

  dims = getDimensions(path);
  if (dims.empty()) {
    cerr << "Failed to get the dimensions\n";
    return result;
  }

  // Multiply all the dimensions together
  auto size =
    std::accumulate(dims.cbegin(), dims.cend(), 1, std::multiplies<int>());

  result.resize(size);
  if (!readData(path, result.data())) {
    cerr << "Failed to read the data\n";
    return vector<T>();
  }

  return result;
}

template <typename T>
bool H5ReadWrite::readData(const string& path, T* data)
{
  const hid_t dataTypeId = BasicTypeToH5<T>::dataTypeId();
  const hid_t memTypeId = BasicTypeToH5<T>::memTypeId();

  if (!m_impl->readData(path, dataTypeId, memTypeId, data)) {
    cerr << "Failed to read the data\n";
    return false;
  }

  return true;
}

bool H5ReadWrite::readData(const string& path, const DataType& type, void* data,
                           int* strides, size_t* start, size_t* counts)
{
  auto it = DataTypeToH5DataType.find(type);
  if (it == DataTypeToH5DataType.end()) {
    cerr << "Failed to get H5 data type for " << dataTypeToString(type) << "\n";
    return false;
  }

  hid_t dataTypeId = it->second;

  auto memIt = DataTypeToH5MemType.find(type);
  if (memIt == DataTypeToH5MemType.end()) {
    cerr << "Failed to get H5 mem type for " << dataTypeToString(type) << "\n";
    return false;
  }

  hid_t memTypeId = memIt->second;

  if (!m_impl->readData(path, dataTypeId, memTypeId, data, strides, start,
                        counts)) {
    cerr << "Failed to read the data\n";
    return false;
  }

  return true;
}

template <typename T>
bool H5ReadWrite::writeData(const string& path, const string& name,
                            const vector<int>& dims, const T* data)
{
  const hid_t dataTypeId = BasicTypeToH5<T>::dataTypeId();
  const hid_t memTypeId = BasicTypeToH5<T>::memTypeId();

  return m_impl->writeData(path, name, dims, data, dataTypeId, memTypeId);
}

bool H5ReadWrite::writeData(const string& path, const string& name,
                            const vector<int>& dims, const DataType& type,
                            const void* data)
{
  auto it = DataTypeToH5DataType.find(type);
  if (it == DataTypeToH5DataType.end()) {
    cerr << "Failed to get H5 data type for " << dataTypeToString(type) << "\n";
    return false;
  }

  hid_t dataTypeId = it->second;

  auto memIt = DataTypeToH5MemType.find(type);
  if (memIt == DataTypeToH5MemType.end()) {
    cerr << "Failed to get H5 mem type for " << dataTypeToString(type) << "\n";
    return false;
  }

  hid_t memTypeId = memIt->second;

  return m_impl->writeData(path, name, dims, data, dataTypeId, memTypeId);
}

template <typename T>
bool H5ReadWrite::setAttribute(const string& path, const string& name, T value)
{
  const hid_t dataTypeId = BasicTypeToH5<T>::dataTypeId();
  const hid_t memTypeId = BasicTypeToH5<T>::memTypeId();

  return m_impl->setAttribute(path, name, &value, dataTypeId, memTypeId, 1);
}

// Specialization for string
template <>
bool H5ReadWrite::setAttribute<const string&>(const string& path,
                                              const string& name,
                                              const string& value)
{
  if (!m_impl->fileIsValid()) {
    cerr << "File is not valid\n";
    return false;
  }

  hid_t fileId = m_impl->fileId();
  bool onData = m_impl->isDataSet(path);

  hid_t parentId;
  herr_t (*closer)(hid_t) = nullptr;
  if (onData) {
    parentId = H5Dopen(fileId, path.c_str(), H5P_DEFAULT);
    closer = H5Dclose;
  } else {
    parentId = H5Gopen(fileId, path.c_str(), H5P_DEFAULT);
    closer = H5Gclose;
  }

  HIDCloser parentCloser(parentId, closer);

  hsize_t dims = 1;
  hid_t dataSpaceId = H5Screate_simple(1, &dims, nullptr);

  HIDCloser dataSpaceCloser(dataSpaceId, H5Sclose);

  hid_t dataType = H5Tcopy(H5T_C_S1);
  herr_t status = H5Tset_size(dataType, H5T_VARIABLE);

  if (status < 0) {
    cerr << "Failed to set the size\n";
    return false;
  }

  hid_t attributeId = H5Acreate2(parentId, name.c_str(), dataType, dataSpaceId,
                                 H5P_DEFAULT, H5P_DEFAULT);

  if (attributeId < 0) {
    cerr << "Failed to create attribute\n";
    return false;
  }

  HIDCloser attributeCloser(attributeId, H5Aclose);

  // Need a char**
  const char* tmp = value.c_str();
  return H5Awrite(attributeId, dataType, &tmp) >= 0;
}

template <>
bool H5ReadWrite::setAttribute<const char*>(const string& path,
                                            const string& name,
                                            const char* value)
{
  return setAttribute<const string&>(path, name, value);
}

bool H5ReadWrite::createGroup(const string& path)
{
  if (!m_impl->fileIsValid()) {
    cerr << "File is not valid\n";
    return false;
  }

  hid_t id = H5Gcreate(m_impl->fileId(), path.c_str(), H5P_DEFAULT, H5P_DEFAULT,
                       H5P_DEFAULT);

  if (id < 0) {
    cerr << "Failed to create group\n";
    return false;
  }

  H5Gclose(id);
  return true;
}

string H5ReadWrite::dataTypeToString(const DataType& type)
{
  // Internal map. Keep it updated with the enum.
  static const map<DataType, const char*> DataTypeToString = {
    { DataType::Int8, "Int8" },     { DataType::Int16, "Int16" },
    { DataType::Int32, "Int32" },   { DataType::Int64, "Int64" },
    { DataType::UInt8, "UInt8" },   { DataType::UInt16, "UInt16" },
    { DataType::UInt32, "UInt32" }, { DataType::UInt64, "UInt64" },
    { DataType::Float, "Float" },   { DataType::Double, "Double" },
    { DataType::String, "String" }, { DataType::None, "None" }
  };

  auto it = DataTypeToString.find(type);
  if (it == DataTypeToString.end())
    return "";

  return it->second;
}

// Instantiate our allowable templates here
// attribute()
template char H5ReadWrite::attribute(const string&, const string&, bool*);
template short H5ReadWrite::attribute(const string&, const string&, bool*);
template int H5ReadWrite::attribute(const string&, const string&, bool*);
template long long H5ReadWrite::attribute(const string&, const string&, bool*);
template unsigned char H5ReadWrite::attribute(const string&, const string&,
                                              bool*);
template unsigned short H5ReadWrite::attribute(const string&, const string&,
                                               bool*);
template unsigned int H5ReadWrite::attribute(const string&, const string&,
                                             bool*);
template unsigned long long H5ReadWrite::attribute(const string&, const string&,
                                                   bool*);
template float H5ReadWrite::attribute(const string&, const string&, bool*);
template double H5ReadWrite::attribute(const string&, const string&, bool*);
template string H5ReadWrite::attribute(const string&, const string&, bool*);

// readData(): single-dimensional
template vector<char> H5ReadWrite::readData(const string&);
template vector<short> H5ReadWrite::readData(const string&);
template vector<int> H5ReadWrite::readData(const string&);
template vector<long long> H5ReadWrite::readData(const string&);
template vector<unsigned char> H5ReadWrite::readData(const string&);
template vector<unsigned short> H5ReadWrite::readData(const string&);
template vector<unsigned int> H5ReadWrite::readData(const string&);
template vector<unsigned long long> H5ReadWrite::readData(const string&);
template vector<float> H5ReadWrite::readData(const string&);
template vector<double> H5ReadWrite::readData(const string&);

// readData(): multi-dimensional
template vector<char> H5ReadWrite::readData(const string&, vector<int>&);
template vector<short> H5ReadWrite::readData(const string&, vector<int>&);
template vector<int> H5ReadWrite::readData(const string&, vector<int>&);
template vector<long long> H5ReadWrite::readData(const string&, vector<int>&);
template vector<unsigned char> H5ReadWrite::readData(const string&,
                                                     vector<int>&);
template vector<unsigned short> H5ReadWrite::readData(const string&,
                                                      vector<int>&);
template vector<unsigned int> H5ReadWrite::readData(const string&,
                                                    vector<int>&);
template vector<unsigned long long> H5ReadWrite::readData(const string&,
                                                          vector<int>&);
template vector<float> H5ReadWrite::readData(const string&, vector<int>&);
template vector<double> H5ReadWrite::readData(const string&, vector<int>&);

// readData(): multi-dimensional
template bool H5ReadWrite::readData(const string&, char*);
template bool H5ReadWrite::readData(const string&, short*);
template bool H5ReadWrite::readData(const string&, int*);
template bool H5ReadWrite::readData(const string&, long long*);
template bool H5ReadWrite::readData(const string&, unsigned char*);
template bool H5ReadWrite::readData(const string&, unsigned short*);
template bool H5ReadWrite::readData(const string&, unsigned int*);
template bool H5ReadWrite::readData(const string&, unsigned long long*);
template bool H5ReadWrite::readData(const string&, float*);
template bool H5ReadWrite::readData(const string&, double*);

// setAttribute
template bool H5ReadWrite::setAttribute(const string&, const string&, char);
template bool H5ReadWrite::setAttribute(const string&, const string&, short);
template bool H5ReadWrite::setAttribute(const string&, const string&, int);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        long long);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        unsigned char);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        unsigned short);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        unsigned int);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        unsigned long long);
template bool H5ReadWrite::setAttribute(const string&, const string&, float);
template bool H5ReadWrite::setAttribute(const string&, const string&, double);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        const string&);
template bool H5ReadWrite::setAttribute(const string&, const string&,
                                        const char*);

// writeData
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const char*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const short*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const int*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const long long*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const unsigned char*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const unsigned short*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const unsigned int*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&,
                                     const unsigned long long*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const float*);
template bool H5ReadWrite::writeData(const string&, const string&,
                                     const vector<int>&, const double*);

// We need to create specializations for these
// template vector<string> H5ReadWrite::readData(const string&);
// template vector<string> H5ReadWrite::readData(const string&, vector<int>&);

} // namespace h5
