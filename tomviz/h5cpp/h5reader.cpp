/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "h5reader.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <numeric>

#include "h5capi.h"
#include "h5typemaps.h"
#include "hidcloser.h"

using std::cout;
using std::cerr;
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

namespace tomviz {

using DataType = H5Reader::DataType;

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

class H5Reader::H5ReaderImpl {
public:
  H5ReaderImpl()
  {
  }

  H5ReaderImpl(const string& file)
  {
    if (!openFile(file))
      cerr << "Warning: failed to open file " << file << "\n";
  }

  ~H5ReaderImpl()
  {
    clear();
  }

  bool openFile(const string& file)
  {
    m_fileId = H5Fopen(file.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
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

  // void* data needs to be of the appropiate type and size
  bool readData(const string& path, hid_t dataTypeId, hid_t memTypeId,
                void* data)
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

    return H5Dread(dataSetId, memTypeId, H5S_ALL, dataSpaceId, H5P_DEFAULT,
                   data) >= 0;
  }

  bool getInfoByName(const string& path, H5O_info_t& info)
  {
    if (!fileIsValid())
      return false;

    return H5Oget_info_by_name(m_fileId, path.c_str(), &info,
                               H5P_DEFAULT) >= 0;
  }

  bool isDataSet(const string& path)
  {
    H5O_info_t info;
    if (!getInfoByName(path, info)) {
      cerr << "Failed to get H5O info by name\n";
      return false;
    }

    return info.type == H5O_TYPE_DATASET;
  }

  DataType getH5ToDataType(hid_t h5type)
  {
    // Find the type
    auto it = std::find_if(H5ToDataType.cbegin(), H5ToDataType.cend(),
      [h5type](const std::pair<hid_t, DataType>& t)
      {
        return H5Tequal(t.first, h5type);
      });

    if (it == H5ToDataType.end()) {
      cerr << "H5ToDataType map does not contain H5 type: " << h5type
           << endl;
      return DataType::None;
    }

    return it->second;
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
};

H5Reader::H5Reader(const string& file)
: m_impl(new H5ReaderImpl(file))
{
}

H5Reader::~H5Reader() = default;

vector<string> H5Reader::children(const string& path, bool* ok)
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
T H5Reader::attribute(const string& path, const string& name, bool* ok)
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
template<>
string H5Reader::attribute<string>(const string& path, const string& name,
                                   bool* ok)
{
  setOk(ok, false);
  string result;

  if (!m_impl->attributeExists(path, name)) {
    cerr << "Attribute " << path << name << " not found!" << endl;
    return result;
  }

  hid_t fileId = m_impl->fileId();

  hid_t attr = H5Aopen_by_name(fileId, path.c_str(), name.c_str(),
                               H5P_DEFAULT, H5P_DEFAULT);
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
      delete [] tmpString;
      return result;
    }
    tmpString[size] = '\0'; // set null byte, hdf5 doesn't do this for you
    result = tmpString;
    delete [] tmpString;
  } else {
    cerr << "Unknown error occurred" << endl;
    return result;
  }

  setOk(ok, true);
  return result;
}

bool H5Reader::hasAttribute(const string& path)
{
  return m_impl->hasAttribute(path);
}

bool H5Reader::hasAttribute(const string& path, const string& name)
{
  return m_impl->attributeExists(path, name);
}

DataType H5Reader::attributeType(const string& path, const string& name)
{
  if (!m_impl->attributeExists(path, name)) {
    cerr << "Attribute " << path << name << " not found!" << endl;
    return DataType::None;
  }

  hid_t fileId = m_impl->fileId();
  hid_t attr = H5Aopen_by_name(fileId, path.c_str(), name.c_str(),
                               H5P_DEFAULT, H5P_DEFAULT);
  hid_t h5type = H5Aget_type(attr);

  // For automatic closing upon leaving scope
  HIDCloser attrCloser(attr, H5Aclose);
  HIDCloser typeCloser(h5type, H5Tclose);

  // Special case for strings
  if (H5T_STRING == H5Tget_class(h5type))
    return DataType::String;

  return m_impl->getH5ToDataType(h5type);
}

bool H5Reader::isDataSet(const string& path)
{
  return m_impl->isDataSet(path);
}

vector<string> H5Reader::allDataSets()
{
  if (!m_impl->fileIsValid())
    return vector<string>();

  ListAllDataSetsVisitor visitor;
  herr_t code = H5Ovisit(m_impl->fileId(), H5_INDEX_NAME, H5_ITER_INC,
                         &visitor.operation, &visitor);

  if (code < 0)
    return vector<string>();

  return visitor.dataSets;
}

DataType H5Reader::dataType(const string& path)
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

vector<int> H5Reader::getDimensions(const string& path)
{
  vector<int> result;
  if (!m_impl->isDataSet(path)) {
    cerr << path << " is not a data set.\n";
    return result;
  }

  hid_t dataSetId = H5Dopen(m_impl->fileId(), path.c_str(), H5P_DEFAULT);
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

int H5Reader::dimensionCount(const string& path)
{
  vector<int> dims = getDimensions(path);
  if (dims.empty()) {
    cerr << "Failed to get the dimensions\n";
    return -1;
  }

  return dims.size();
}

template <typename T>
vector<T> H5Reader::readData(const string& path)
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
vector<T> H5Reader::readData(const string& path, vector<int>& dims)
{
  vector<T> result;

  dims = getDimensions(path);
  if (dims.empty()) {
    cerr << "Failed to get the dimensions\n";
    return result;
  }

  // Multiply all the dimensions together
  auto size = std::accumulate(dims.cbegin(), dims.cend(), 1,
                              std::multiplies<int>());

  result.resize(size);
  if (!readData(path, result.data())) {
    cerr << "Failed to read the data\n";
    return vector<T>();
  }

  return result;
}

template <typename T>
bool H5Reader::readData(const string& path, T* data)
{
  const hid_t dataTypeId = BasicTypeToH5<T>::dataTypeId();
  const hid_t memTypeId = BasicTypeToH5<T>::memTypeId();

  if (!m_impl->readData(path, dataTypeId, memTypeId, data)) {
    cerr << "Failed to read the data\n";
    return false;
  }

  return true;
}

string H5Reader::dataTypeToString(const DataType& type)
{
  // Internal map. Keep it updated with the enum.
  static const map<DataType, const char*> DataTypeToString =
  {
    { DataType::Int8,   "Int8"   },
    { DataType::Int16,  "Int16"  },
    { DataType::Int32,  "Int32"  },
    { DataType::Int64,  "Int64"  },
    { DataType::UInt8,  "UInt8"  },
    { DataType::UInt16, "UInt16" },
    { DataType::UInt32, "UInt32" },
    { DataType::UInt64, "UInt64" },
    { DataType::Float,  "Float"  },
    { DataType::Double, "Double" },
    { DataType::String, "String" },
    { DataType::None,   "None"   }
  };

  auto it = DataTypeToString.find(type);
  if (it == DataTypeToString.end())
    return "";

  return it->second;
}

// Instantiate our allowable templates here
// attribute()
template char H5Reader::attribute(const string&, const string&, bool*);
template short H5Reader::attribute(const string&, const string&, bool*);
template int H5Reader::attribute(const string&, const string&, bool*);
template long long H5Reader::attribute(const string&, const string&, bool*);
template unsigned char H5Reader::attribute(const string&, const string&,
                                           bool*);
template unsigned short H5Reader::attribute(const string&, const string&,
                                            bool*);
template unsigned int H5Reader::attribute(const string&, const string&, bool*);
template unsigned long long H5Reader::attribute(const string&, const string&,
                                                bool*);
template float H5Reader::attribute(const string&, const string&, bool*);
template double H5Reader::attribute(const string&, const string&, bool*);
template string H5Reader::attribute(const string&, const string&, bool*);

// readData(): single-dimensional
template vector<char> H5Reader::readData(const string&);
template vector<short> H5Reader::readData(const string&);
template vector<int> H5Reader::readData(const string&);
template vector<long long> H5Reader::readData(const string&);
template vector<unsigned char> H5Reader::readData(const string&);
template vector<unsigned short> H5Reader::readData(const string&);
template vector<unsigned int> H5Reader::readData(const string&);
template vector<unsigned long long> H5Reader::readData(const string&);
template vector<float> H5Reader::readData(const string&);
template vector<double> H5Reader::readData(const string&);

// readData(): multi-dimensional
template vector<char> H5Reader::readData(const string&, vector<int>&);
template vector<short> H5Reader::readData(const string&, vector<int>&);
template vector<int> H5Reader::readData(const string&, vector<int>&);
template vector<long long> H5Reader::readData(const string&, vector<int>&);
template vector<unsigned char> H5Reader::readData(const string&, vector<int>&);
template vector<unsigned short> H5Reader::readData(const string&,
                                                   vector<int>&);
template vector<unsigned int> H5Reader::readData(const string&, vector<int>&);
template vector<unsigned long long> H5Reader::readData(const string&,
                                                       vector<int>&);
template vector<float> H5Reader::readData(const string&, vector<int>&);
template vector<double> H5Reader::readData(const string&, vector<int>&);

// readData(): multi-dimensional
template bool H5Reader::readData(const string&, char*);
template bool H5Reader::readData(const string&, short*);
template bool H5Reader::readData(const string&, int*);
template bool H5Reader::readData(const string&, long long*);
template bool H5Reader::readData(const string&, unsigned char*);
template bool H5Reader::readData(const string&, unsigned short*);
template bool H5Reader::readData(const string&, unsigned int*);
template bool H5Reader::readData(const string&, unsigned long long*);
template bool H5Reader::readData(const string&, float*);
template bool H5Reader::readData(const string&, double*);

// We need to create specializations for these
//template vector<string> H5Reader::readData(const string&);
//template vector<string> H5Reader::readData(const string&, vector<int>&);

} // namespace tomviz
