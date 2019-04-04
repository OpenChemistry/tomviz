/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "EmdFormat.h"

#include "DataSource.h"

#include <h5cpp/h5readwrite.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkImagePermute.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTrivialProducer.h>
#include <vtkTypeInt8Array.h>

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

template <typename T>
void ReorderArrayC(T* in, T* out, int dim[3])
{
  for (int i = 0; i < dim[0]; ++i) {
    for (int j = 0; j < dim[1]; ++j) {
      for (int k = 0; k < dim[2]; ++k) {
        out[(i * dim[1] + j) * dim[2] + k] =
          in[(k * dim[1] + j) * dim[0] + i];
      }
    }
  }
}

template <typename T>
void ReorderArrayF(T* in, T* out, int dim[3])
{
  for (int i = 0; i < dim[0]; ++i) {
    for (int j = 0; j < dim[1]; ++j) {
      for (int k = 0; k < dim[2]; ++k) {
        out[(k * dim[1] + j) * dim[0] + i] =
          in[(i * dim[1] + j) * dim[2] + k];
      }
    }
  }
}

std::string firstEmdNode(h5::H5ReadWrite& reader)
{
  // Find the first valid EMD node, and return its path.
  std::vector<std::string> firstLevel = reader.children("/");
  for (size_t i = 0; i < firstLevel.size(); ++i) {
    std::vector<std::string> nodes = reader.children("/" + firstLevel[i]);
    for (size_t j = 0; j < nodes.size(); ++j) {
      std::string path = "/" + firstLevel[i] + "/" + nodes[j];
      if (reader.hasAttribute(path, "emd_group_type"))
        return path;

      // This is a little hackish, some EMDs don't use the attribute.
      std::vector<std::string> third = reader.children(path);
      for (size_t k = 0; k < third.size(); ++k) {
        if (third[k] == "data") {
          return path;
        }
      }
    }
  }
  return "";
}

int dataTypeToVTK(h5::H5ReadWrite::DataType& type)
{
  using DataType = h5::H5ReadWrite::DataType;
  switch (type) {
    case DataType::Float:
      return VTK_FLOAT;
    case DataType::Double:
      return VTK_DOUBLE;
    case DataType::Int8:
      return VTK_SIGNED_CHAR;
    case DataType::Int16:
      return VTK_SHORT;
    case DataType::Int32:
      return VTK_INT;
    case DataType::Int64:
      return VTK_LONG_LONG;
    case DataType::UInt8:
      return VTK_UNSIGNED_CHAR;
    case DataType::UInt16:
      return VTK_UNSIGNED_SHORT;
    case DataType::UInt32:
      return VTK_UNSIGNED_INT;
    case DataType::UInt64:
      return VTK_UNSIGNED_LONG_LONG;
    default:
      cout << "Error: could not convert DataType to VTK\n";
      return -1;
  }
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
    h5dim[0] = dim[0];
    h5dim[1] = dim[1];
    h5dim[2] = dim[2];

    // We must allocate a new array, and copy the reordered array into it.
    auto arrayPtr = data->GetPointData()->GetScalars();
    auto dataPtr = arrayPtr->GetVoidPointer(0);
    vtkNew<vtkImageData> reorderedImageData;
    reorderedImageData->SetDimensions(dim);
    reorderedImageData->AllocateScalars(arrayPtr->GetDataType(), 1);
    auto outPtr =
      reorderedImageData->GetPointData()->GetScalars()->GetVoidPointer(0);

    switch (arrayPtr->GetDataType()) {
    vtkTemplateMacro(tomviz::ReorderArrayC(
      reinterpret_cast<VTK_TT*>(dataPtr), reinterpret_cast<VTK_TT*>(outPtr),
      dim));
    default:
      cout << "EMD: Unknown data type" << endl;
    }

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
    hid_t dataspaceId = H5Screate_simple(3, &h5dim[0], NULL);

    switch (data->GetScalarType()) {
      vtkTemplateMacro(success =
                         writeVolume((VTK_TT*)(outPtr), groupId, name.c_str(),
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
};

EmdFormat::EmdFormat() : d(new Private) {}

bool EmdFormat::read(const std::string& fileName, vtkImageData* image)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  bool ok;
  reader.attribute<unsigned int>("/", "version_major");

  if (!ok) {
    cout << "Failed to find version_major" << endl;
  }

  reader.attribute<unsigned int>("/", "version_minor");
  if (!ok) {
    cout << "Failed to find version_minor" << endl;
  }

  std::string emdNode = firstEmdNode(reader);
  std::string emdDataNode = emdNode + "/data";

  if (emdNode.length() == 0) {
    // We couldn't find a valid EMD node, exit early.
    return false;
  }

  // If it isn't a data set, we are done
  if (!reader.isDataSet(emdDataNode))
    return false;

  // Get the type of the data
  h5::H5ReadWrite::DataType type = reader.dataType(emdDataNode);
  int vtkDataType = dataTypeToVTK(type);

  // Get the dimensions
  std::vector<int> dims = reader.getDimensions(emdDataNode);

  vtkNew<vtkImageData> tmp;
  tmp->SetDimensions(&dims[0]);
  tmp->AllocateScalars(vtkDataType, 1);
  image->SetDimensions(&dims[0]);
  image->AllocateScalars(vtkDataType, 1);

  if (!reader.readData(emdDataNode, type, tmp->GetScalarPointer())) {
    cerr << "Failed to read the data\n";
    return false;
  }

  // EMD stores data as row major order. VTK expects column major order.
  auto inPtr = tmp->GetPointData()->GetScalars()->GetVoidPointer(0);
  auto outPtr = image->GetPointData()->GetScalars()->GetVoidPointer(0);
  switch (image->GetPointData()->GetScalars()->GetDataType()) {
  vtkTemplateMacro(tomviz::ReorderArrayF(
    reinterpret_cast<VTK_TT*>(inPtr), reinterpret_cast<VTK_TT*>(outPtr),
    &dims[0]));
  default:
    cout << "EMD: Unknown data type" << endl;
  }
  image->Modified();

  // Now to read back in the units, note the reordering for C vs Fortran...
  std::string dimNode = emdNode + "/dim1";
  auto dim1 = reader.readData<float>(dimNode);
  dimNode = emdNode + "/dim2";
  auto dim2 = reader.readData<float>(dimNode);
  dimNode = emdNode + "/dim3";
  auto dim3 = reader.readData<float>(dimNode);

  if (dim1.size() > 1 && dim2.size() > 1 && dim3.size() > 1) {
    double spacing[3];
    spacing[0] = static_cast<double>(dim1[1] - dim1[0]);
    spacing[1] = static_cast<double>(dim2[1] - dim2[0]);
    spacing[2] = static_cast<double>(dim3[1] - dim3[0]);
    image->SetSpacing(spacing);
  }

  QVector<double> angles;
  auto units = reader.attribute<std::string>(emdNode + "/dim1", "units", &ok);
  if (ok) {
    if (units == "[deg]") {
      for (unsigned i = 0; i < dim1.size(); ++i) {
        angles.push_back(dim1[i]);
      }
    } else if (units == "[rad]") {
      for (unsigned i = 0; i < dim1.size(); ++i) {
        // Convert radians to degrees since tomviz assumes degrees everywhere.
        angles.push_back(dim1[i] * 180.0 / vtkMath::Pi());
      }
    }
  }

  // If this is a tilt series, swap the X and Z axes
  if (!angles.isEmpty()) {
    vtkNew<vtkImagePermute> permute;
    permute->SetFilteredAxes(2, 1, 0);
    permute->SetInputData(image);
    permute->Update();
    image->ShallowCopy(permute->GetOutput());
    DataSource::setTiltAngles(image, angles);

    // Now set the field data to preserve the tilt series state
    vtkNew<vtkTypeInt8Array> typeArray;
    typeArray->SetNumberOfComponents(1);
    typeArray->SetNumberOfTuples(1);
    typeArray->SetName("tomviz_data_source_type");
    typeArray->SetTuple1(0, DataSource::TiltSeries);

    auto fd = image->GetFieldData();
    fd->AddArray(typeArray);
  }

  return true;
}

bool EmdFormat::write(const std::string& fileName, DataSource* source)
{
  // Now create the tomography data store!
  auto t = source->producer();
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

  // See if we have tilt angles
  auto hasTiltAngles = DataSource::hasTiltAngles(image);

  // If this is a tilt series, swap the X and Z axes
  vtkImageData* permutedImage = image;
  vtkNew<vtkImagePermute> permute;
  if (DataSource::hasTiltAngles(image)) {
    permute->SetFilteredAxes(2, 1, 0);
    permute->SetInputData(image);
    permute->Update();
    permutedImage = permute->GetOutput();
  }

  int xIndex = 0;
  int yIndex = 1;
  int zIndex = 2;

  // Use constant spacing, with zero offset, so just populate the first two.
  double spacing[3];
  permutedImage->GetSpacing(spacing);
  int dimensions[3];
  permutedImage->GetDimensions(dimensions);

  std::vector<float> imageDimDataX(dimensions[0]);
  std::vector<float> imageDimDataY(dimensions[1]);
  std::vector<float> imageDimDataZ(dimensions[2]);

  if (hasTiltAngles) {
    auto angles = DataSource::getTiltAngles(permutedImage);
    imageDimDataX.reserve(angles.size());
    for (int i = 0; i < angles.size(); ++i) {
      imageDimDataX[i] = static_cast<float>(angles[i]);
    }
  } else {
    for (int i = 0; i < dimensions[0]; ++i) {
      imageDimDataX[i] = i * spacing[xIndex];
    }
  }
  for (int i = 0; i < dimensions[1]; ++i) {
    imageDimDataY[i] = i * spacing[yIndex];
  }
  for (int i = 0; i < dimensions[2]; ++i) {
    imageDimDataZ[i] = i * spacing[zIndex];
  }

  d->writeData("/data/tomography", "data", permutedImage);

  // Create the 3 dim sets too...
  std::vector<int> side(1);
  side[0] = imageDimDataX.size();
  d->writeData("/data/tomography", "dim1", side, imageDimDataX);
  if (hasTiltAngles) {
    d->setAttribute("/data/tomography/dim1", "name", "angles", true);
    d->setAttribute("/data/tomography/dim1", "units", "[deg]", true);
  }
  else {
    d->setAttribute("/data/tomography/dim1", "name", "x", true);
    d->setAttribute("/data/tomography/dim1", "units", "[n_m]", true);
  }

  side[0] = imageDimDataY.size();
  d->writeData("/data/tomography", "dim2", side, imageDimDataY);
  d->setAttribute("/data/tomography/dim2", "name", "y", true);
  d->setAttribute("/data/tomography/dim2", "units", "[n_m]", true);

  side[0] = imageDimDataZ.size();
  d->writeData("/data/tomography", "dim3", side, imageDimDataZ);
  if (hasTiltAngles) {
    d->setAttribute("/data/tomography/dim3", "name", "x", true);
    d->setAttribute("/data/tomography/dim3", "units", "[n_m]", true);
  } else {
    d->setAttribute("/data/tomography/dim3", "name", "z", true);
    d->setAttribute("/data/tomography/dim3", "units", "[n_m]", true);
  }

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
} // namespace tomviz
