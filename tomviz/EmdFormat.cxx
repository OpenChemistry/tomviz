/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "EmdFormat.h"

#include "DataSource.h"

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkTrivialProducer.h>

#include <vtkSMSourceProxy.h>

#include "vtk_hdf5.h"

#include <cassert>
#include <string>
#include <vector>

#include <iostream>

namespace tomviz {

/**
 * Write the data into the supplied group. This is really just mapping the C++
 * vtkImageData types to the HDF5 API/types.
 *
 * dataTypeId refers to the type that will be stored in the HDF5 file
 * memTypeId refers to the memory type that will be copied from vtkImageData
 */
template <typename T>
bool writeVolume(T* buffer, hid_t groupId, const char* name, hid_t dataspaceId,
                 hid_t dataTypeId, hid_t memTypeId)
{
  bool success = true;
  hid_t dataId = H5Dcreate(groupId, name, dataTypeId, dataspaceId, H5P_DEFAULT,
                           H5P_DEFAULT, H5P_DEFAULT);
  if (dataId < 0) { // Failed to create object.
    return false;
  }
  hid_t status =
    H5Dwrite(dataId, memTypeId, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
  if (status < 0) {
    success = false;
  }
  status = H5Dclose(dataId);
  if (status < 0) {
    success = false;
  }
  return success;
}

class EmdFormat::Private
{
public:
  Private() : fileId(H5I_INVALID_HID) {}
  hid_t fileId;

  hid_t createGroup(const std::string& group)
  {
    return H5Gcreate(fileId, group.c_str(), H5P_DEFAULT, H5P_DEFAULT,
                     H5P_DEFAULT);
  }

  bool attribute(const std::string& group, const std::string& name, void* value,
                 hid_t typeId)
  {
    if (H5Aexists_by_name(fileId, group.c_str(), name.c_str(), H5P_DEFAULT) <=
        0) {
      // The specified attribute does not exist.
      cout << group << name << " not found!" << endl;
      return false;
    }

    hid_t attr = H5Aopen_by_name(fileId, group.c_str(), name.c_str(),
                                 H5P_DEFAULT, H5P_DEFAULT);
    hid_t type = H5Aget_type(attr);
    if (H5Tequal(type, typeId) == 0) {
      // The type of the attribute does not match the requested type.
      cout << "Type determined does not match that requested." << endl;
      cout << type << " -> " << typeId << " : " << H5T_STD_U32LE << endl;
      H5Aclose(attr);
      return false;
    } else if (H5Tequal(type, typeId) < 0) {
      cout << "Something went really wrong....\n\n";
      H5Aclose(attr);
      return false;
    }
    hid_t status = H5Aread(attr, H5T_NATIVE_INT, value);
    return status >= 0;
  }

  bool attribute(const std::string& group, const std::string& name, int& value)
  {
    return attribute(group, name, reinterpret_cast<void*>(&value),
                     H5T_STD_U32LE);
  }

  bool setAttribute(const std::string& group, const std::string& name,
                    void* value, hid_t fileTypeId, hid_t typeId, hsize_t dims,
                    bool onData)
  {
    hid_t parentId;
    if (onData) {
      parentId = H5Dopen(fileId, group.c_str(), H5P_DEFAULT);
    } else {
      parentId = H5Gopen(fileId, group.c_str(), H5P_DEFAULT);
    }
    hid_t dataspaceId = H5Screate_simple(1, &dims, NULL);
    hid_t attributeId = H5Acreate2(parentId, name.c_str(), fileTypeId,
                                   dataspaceId, H5P_DEFAULT, H5P_DEFAULT);
    herr_t status = H5Awrite(attributeId, typeId, value);
    if (status < 0) {
      return false;
    }
    status = H5Aclose(attributeId);
    if (status < 0) {
      return false;
    }
    status = H5Sclose(dataspaceId);
    if (status < 0) {
      return false;
    }
    if (onData) {
      status = H5Dclose(parentId);
    } else {
      status = H5Gclose(parentId);
    }
    if (status < 0) {
      return false;
    }
    return true;
  }

  bool setAttribute(const std::string& group, const std::string& name,
                    float value, bool onData = false)
  {
    return setAttribute(group, name, reinterpret_cast<void*>(&value),
                        H5T_IEEE_F32LE, H5T_NATIVE_FLOAT, 1, onData);
  }

  bool setAttribute(const std::string& group, const std::string& name,
                    int value, bool onData = false)
  {
    return setAttribute(group, name, reinterpret_cast<void*>(&value),
                        H5T_STD_U32LE, H5T_NATIVE_INT, 1, onData);
  }

  bool setAttribute(const std::string& group, const std::string& name,
                    const std::string& value, bool onData = false)
  {
    hid_t parentId;
    if (onData) {
      parentId = H5Dopen(fileId, group.c_str(), H5P_DEFAULT);
    } else {
      parentId = H5Gopen(fileId, group.c_str(), H5P_DEFAULT);
    }
    hsize_t dims = 1;
    hid_t dataspaceId = H5Screate_simple(1, &dims, NULL);
    hid_t dataType = H5Tcopy(H5T_C_S1);
    herr_t status = H5Tset_size(dataType, H5T_VARIABLE);
    hid_t attributeId = H5Acreate2(parentId, name.c_str(), dataType,
                                   dataspaceId, H5P_DEFAULT, H5P_DEFAULT);
    const char* tmp = value.c_str();
    status = H5Awrite(attributeId, dataType, &tmp);
    if (status < 0) {
      return false;
    }
    status = H5Aclose(attributeId);
    if (status < 0) {
      return false;
    }
    status = H5Sclose(dataspaceId);
    if (status < 0) {
      return false;
    }
    if (onData) {
      status = H5Dclose(parentId);
    } else {
      status = H5Gclose(parentId);
    }
    if (status < 0) {
      return false;
    }
    return true;
  }

  bool writeData(const std::string& group, const std::string& name,
                 const std::vector<int>& dims, const std::vector<float>& data)
  {
    bool success = true;
    std::vector<hsize_t> h5dim;
    for (size_t i = 0; i < dims.size(); ++i) {
      h5dim.push_back(static_cast<hsize_t>(dims[i]));
    }
    hid_t groupId = H5Gopen(fileId, group.c_str(), H5P_DEFAULT);
    hid_t dataspaceId =
      H5Screate_simple(static_cast<int>(dims.size()), &h5dim[0], NULL);
    hid_t dataId = H5Dcreate(groupId, name.c_str(), H5T_IEEE_F32LE, dataspaceId,
                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    hid_t status = H5Dwrite(dataId, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                            H5P_DEFAULT, &data[0]);
    if (status < 0) {
      success = false;
    }
    status = H5Dclose(dataId);
    status = H5Sclose(dataspaceId);
    status = H5Gclose(groupId);
    return success;
  }

  bool writeData(const std::string& group, const std::string& name,
                 vtkImageData* data)
  {
    bool success = true;
    hsize_t h5dim[3];
    int dim[3] = { 0, 0, 0 };
    data->GetDimensions(dim);
    h5dim[0] = dim[2];
    h5dim[1] = dim[1];
    h5dim[2] = dim[0];

    auto arrayPtr = data->GetPointData()->GetScalars();
    auto dataPtr = arrayPtr->GetVoidPointer(0);

    // Map the VTK types to the HDF5 types for storage and memory. We should
    // probably add more, but I got the important ones for testing in first.
    hid_t dataTypeId = 0;
    hid_t memTypeId = 0;
    switch (data->GetScalarType()) {
      case VTK_DOUBLE:
        dataTypeId = H5T_IEEE_F64LE;
        memTypeId = H5T_NATIVE_DOUBLE;
        break;
      case VTK_FLOAT:
        dataTypeId = H5T_IEEE_F32LE;
        memTypeId = H5T_NATIVE_FLOAT;
        break;
      case VTK_UNSIGNED_INT:
        dataTypeId = H5T_STD_U32LE;
        memTypeId = H5T_NATIVE_UINT;
        break;
      case VTK_UNSIGNED_SHORT:
        dataTypeId = H5T_STD_U16LE;
        memTypeId = H5T_NATIVE_USHORT;
        break;
      case VTK_UNSIGNED_CHAR:
        dataTypeId = H5T_STD_U8LE;
        memTypeId = H5T_NATIVE_UCHAR;
        break;
      default:
        success = false;
    }

    hid_t groupId = H5Gopen(fileId, group.c_str(), H5P_DEFAULT);
    hid_t dataspaceId =
      H5Screate_simple(3, &h5dim[0], NULL);

    switch (data->GetScalarType()) {
      vtkTemplateMacro(success =
                         writeVolume((VTK_TT*)(dataPtr), groupId, name.c_str(),
                                     dataspaceId, dataTypeId, memTypeId));
      default:
        success = false;
    }

    hid_t status = H5Sclose(dataspaceId);
    if (status < 0) {
      success = false;
    }
    status = H5Gclose(groupId);
    if (status < 0) {
      success = false;
    }
    return success;
  }

  std::vector<float> readData(const std::string& path)
  {
    std::vector<float> result;

    // Verify that the path exists in the HDF5 file.
    bool dataLinkExists = false;
    H5O_info_t info;
    if (H5Oget_info_by_name(fileId, path.c_str(), &info,
                            H5P_DEFAULT < 0)) {
      dataLinkExists = false;
    } else {
      dataLinkExists = true;
    }

    if (!dataLinkExists || info.type != H5O_TYPE_DATASET) {
      return result;
    }

    std::vector<int> dims;
    hid_t datasetId = H5Dopen(fileId, path.c_str(), H5P_DEFAULT);
    if (datasetId < 0) {
      return result;
    }
    hid_t dataspaceId = H5Dget_space(datasetId);
    if (dataspaceId < 0) {
      H5Dclose(dataspaceId);
      return result;
    }
    int dimCount = H5Sget_simple_extent_ndims(dataspaceId);
    if (dimCount < 1) {
      H5Sclose(dataspaceId);
      H5Dclose(datasetId);
      return result;
    }

    hsize_t* h5dims = new hsize_t[dimCount];
    int dimCount2 = H5Sget_simple_extent_dims(dataspaceId, h5dims, nullptr);
    if (dimCount == dimCount2) {
      dims.resize(dimCount);
      std::copy(h5dims, h5dims + dimCount, dims.begin());
    }
    if (dimCount == 3) {
      dims[0] = h5dims[2];
      dims[2] = h5dims[0];
    }
    delete[] h5dims;

    hid_t dataTypeId = H5Dget_type(datasetId);
    hid_t memTypeId = 0;

    if (H5Tequal(dataTypeId, H5T_IEEE_F32LE)) {
      memTypeId = H5T_NATIVE_FLOAT;
    } else {
      // Not accounted for, fail for now, should probably improve this soon.
      std::cout << "Unknown type encountered!" << dataTypeId << std::endl;
      H5Tclose(dataTypeId);
      H5Sclose(dataspaceId);
      H5Dclose(datasetId);
      return result;
    }
    H5Tclose(dataTypeId);

    if (dimCount != 1) {
      // Only implemented for single dimensional data - vector of type double.
      H5Sclose(dataspaceId);
      H5Dclose(datasetId);
      std::cout << "Dimensions are " << dimCount << std::endl;
      return result;
    }

    result.resize(dims[0]);

    H5Dread(datasetId, memTypeId, H5S_ALL, dataspaceId, H5P_DEFAULT,
            result.data());

    H5Sclose(dataspaceId);
    H5Dclose(datasetId);

    return result;
  }

  bool readData(const std::string& path, vtkImageData* data)
  {
    std::vector<int> dims;
    hid_t datasetId = H5Dopen(fileId, path.c_str(), H5P_DEFAULT);
    if (datasetId < 0) {
      return false;
    }
    hid_t dataspaceId = H5Dget_space(datasetId);
    if (dataspaceId < 0) {
      H5Dclose(dataspaceId);
      return false;
    }
    int dimCount = H5Sget_simple_extent_ndims(dataspaceId);
    if (dimCount < 1) {
      H5Sclose(dataspaceId);
      H5Dclose(datasetId);
      return false;
    }

    hsize_t* h5dims = new hsize_t[dimCount];
    int dimCount2 = H5Sget_simple_extent_dims(dataspaceId, h5dims, nullptr);
    if (dimCount == dimCount2) {
      dims.resize(dimCount);
      std::copy(h5dims, h5dims + dimCount, dims.begin());
    }
    if (dimCount == 3) {
      dims[0] = h5dims[2];
      dims[2] = h5dims[0];
    }
    delete[] h5dims;

    // Map the HDF5 types to the VTK types for storage and memory. We should
    // probably add more, but I got the important ones for testing in first.
    int vtkDataType = VTK_FLOAT;
    hid_t dataTypeId = H5Dget_type(datasetId);
    hid_t memTypeId = 0;

    if (H5Tequal(dataTypeId, H5T_IEEE_F64LE)) {
      memTypeId = H5T_NATIVE_DOUBLE;
      vtkDataType = VTK_DOUBLE;
    } else if (H5Tequal(dataTypeId, H5T_IEEE_F32LE)) {
      memTypeId = H5T_NATIVE_FLOAT;
      vtkDataType = VTK_FLOAT;
    } else if (H5Tequal(dataTypeId, H5T_STD_I32LE)) {
      memTypeId = H5T_NATIVE_INT;
      vtkDataType = VTK_INT;
    } else if (H5Tequal(dataTypeId, H5T_STD_U32LE)) {
      memTypeId = H5T_NATIVE_UINT;
      vtkDataType = VTK_UNSIGNED_INT;
    } else if (H5Tequal(dataTypeId, H5T_STD_I16LE)) {
      memTypeId = H5T_NATIVE_SHORT;
      vtkDataType = VTK_SHORT;
    } else if (H5Tequal(dataTypeId, H5T_STD_U16LE)) {
      memTypeId = H5T_NATIVE_USHORT;
      vtkDataType = VTK_UNSIGNED_SHORT;
    } else if (H5Tequal(dataTypeId, H5T_STD_I8LE)) {
        memTypeId = H5T_NATIVE_CHAR;
        vtkDataType = VTK_SIGNED_CHAR;
    } else if (H5Tequal(dataTypeId, H5T_STD_U8LE)) {
      memTypeId = H5T_NATIVE_UCHAR;
      vtkDataType = VTK_UNSIGNED_CHAR;
    } else {
      // Not accounted for, fail for now, should probably improve this soon.
      std::cout << "Unknown type encountered!" << dataTypeId << std::endl;
      H5Tclose(dataTypeId);
      H5Sclose(dataspaceId);
      H5Dclose(datasetId);
      return false;
    }
    H5Tclose(dataTypeId);

    data->SetDimensions(&dims[0]);
    data->AllocateScalars(vtkDataType, 1);

    H5Dread(datasetId, memTypeId, H5S_ALL, dataspaceId, H5P_DEFAULT,
            data->GetScalarPointer());
    data->Modified();

    H5Sclose(dataspaceId);
    H5Dclose(datasetId);

    return true;
  }

  std::vector<std::string> children(const std::string path)
  {
    std::vector<std::string> result;
    hsize_t objCount = 0;
    constexpr int maxName = 2048;
    char groupName[maxName];
    hid_t groupId = H5Gopen(fileId, path.c_str(), H5P_DEFAULT);
    //H5Iget_name(groupId, groupName, maxName);
    //std::cout << "group name " << groupName << std::endl;
    H5Gget_num_objs(groupId, &objCount);
    for (hsize_t i = 0; i < objCount; ++i) {
      H5Gget_objname_by_idx(groupId, i, groupName, maxName);
      result.push_back(groupName);
    }
    H5Gclose(groupId);
    return result;
  }

  std::string firstEmdNode()
  {
    // Find the first valid EMD node, and return its path.
    int emdVersion = 1;
    std::vector<std::string> firstLevel = children("/");
    for (size_t i = 0; i < firstLevel.size(); ++i) {
      std::vector<std::string> nodes = children("/" + firstLevel[i]);
      for (size_t j = 0; j < nodes.size(); ++j) {
        std::string path = "/" + firstLevel[i] + "/" + nodes[j];
        if (attribute(path, "emd_group_type", emdVersion)) {
          return path;
        }
        // This is a little hackish, some EMDs don't use the attribute.
        std::vector<std::string> third = children(path);
        for (size_t k = 0; k < third.size(); ++k) {
          if (third[k] == "data") {
            return path;
          }
        }
      }
    }
    return "";
  }
};

EmdFormat::EmdFormat() : d(new Private)
{
}

bool EmdFormat::read(const std::string& fileName, vtkImageData* image)
{
  d->fileId = H5Fopen(fileName.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);

  int version[2];
  if (!d->attribute("/", "version_major", version[0])) {
    cout << "Failed to find version_major" << endl;
  }
  if (!d->attribute("/", "version_minor", version[1])) {
    cout << "Failed to find version_minor" << endl;
  }

  std::string emdNode = d->firstEmdNode();
  std::string emdDataNode = emdNode + "/data";

  if (emdNode.length() == 0) {
    // We couldn't find a valid EMD node, exit early.
    return false;
  }

  // Verify that the path exists in the HDF5 file.
  bool dataLinkExists = false;
  if (H5Oexists_by_name(d->fileId, emdNode.c_str(), H5P_DEFAULT) == 1) {
    dataLinkExists = true;
  }

  H5O_info_t info;
  if (H5Oget_info_by_name(d->fileId, emdDataNode.c_str(), &info,
                          H5P_DEFAULT < 0)) {
    dataLinkExists = false;
  } else {
    dataLinkExists = true;
  }

  if (dataLinkExists && info.type == H5O_TYPE_DATASET) {
    d->readData(emdDataNode, image);
  } else {
    return false;
  }

  // Now to read back in the units, note the reordering for C vs Fortran...
  auto dim1 = d->readData("/data/tomography/dim1");
  auto dim2 = d->readData("/data/tomography/dim2");
  auto dim3 = d->readData("/data/tomography/dim3");

  if (dim1.size() > 1 && dim2.size() > 1 && dim3.size() > 1) {
    double spacing[3];
    spacing[2] = static_cast<double>(dim1[1] - dim1[0]);
    spacing[1] = static_cast<double>(dim2[1] - dim2[0]);
    spacing[0] = static_cast<double>(dim3[1] - dim3[0]);
    image->SetSpacing(spacing);
  }

  // Close up the file now we are done.
  if (d->fileId != H5I_INVALID_HID) {
    H5Fclose(d->fileId);
    d->fileId = H5I_INVALID_HID;
  }

  return true;
}

bool EmdFormat::write(const std::string& fileName, DataSource* source)
{
  // Now create the tomography data store!
  auto t =
    vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
  auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  return this->write(fileName, image);
}

bool EmdFormat::write(const std::string& fileName, vtkImageData* image)
{
  d->fileId =
    H5Fcreate(fileName.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  // Create the version attribute on the root group.
  hid_t groupId = H5Gopen(d->fileId, "/", H5P_DEFAULT);

  // Now to create the attributes, groups, etc.
  d->setAttribute("/", "version_major", 0);
  d->setAttribute("/", "version_minor", 2);

  // Now create a "data" group
  hid_t dataGroupId = d->createGroup("/data");
  hid_t tomoGroupId = d->createGroup("/data/tomography");

  // Now create the emd_group_type attribute.
  d->setAttribute("/data/tomography", "emd_group_type", 1);

  hid_t status;

  // Use constant spacing, with zero offset, so just populate the first two.
  double spacing[3];
  image->GetSpacing(spacing);
  std::vector<float> imageDimDataX(2);
  std::vector<float> imageDimDataY(2);
  std::vector<float> imageDimDataZ(2);
  for (int i = 0; i < 2; ++i) {
    // Note the flipping to make our ordering work in C-ordered codes correctly.
    imageDimDataX[i] = i * spacing[2];
    imageDimDataY[i] = i * spacing[1];
    imageDimDataZ[i] = i * spacing[0];
  }

  d->writeData("/data/tomography", "data", image);

  // Create the 3 dim sets too...
  std::vector<int> side;
  side.push_back(2);
  d->writeData("/data/tomography", "dim1", side, imageDimDataX);
  d->setAttribute("/data/tomography/dim1", "name", "x", true);
  d->setAttribute("/data/tomography/dim1", "units", "[n_m]", true);

  d->writeData("/data/tomography", "dim2", side, imageDimDataY);
  d->setAttribute("/data/tomography/dim2", "name", "y", true);
  d->setAttribute("/data/tomography/dim2", "units", "[n_m]", true);

  d->writeData("/data/tomography", "dim3", side, imageDimDataZ);
  d->setAttribute("/data/tomography/dim3", "name", "z", true);
  d->setAttribute("/data/tomography/dim3", "units", "[n_m]", true);

  status = H5Gclose(tomoGroupId);
  status = H5Gclose(dataGroupId);

  status = H5Gclose(groupId);

  // Close up the file now we are done.
  if (d->fileId != H5I_INVALID_HID) {
    status = H5Fclose(d->fileId);
    d->fileId = H5I_INVALID_HID;
  }
  return status >= 0;
}

EmdFormat::~EmdFormat()
{
  delete d;
}
}
