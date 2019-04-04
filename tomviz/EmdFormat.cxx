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

#include <cassert>
#include <string>
#include <vector>

#include <iostream>

namespace tomviz {

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

static const std::map<h5::H5ReadWrite::DataType, int> DataTypeToVTK =
{
  { h5::H5ReadWrite::DataType::Int8,   VTK_SIGNED_CHAR        },
  { h5::H5ReadWrite::DataType::Int16,  VTK_SHORT              },
  { h5::H5ReadWrite::DataType::Int32,  VTK_INT                },
  { h5::H5ReadWrite::DataType::Int64,  VTK_LONG_LONG          },
  { h5::H5ReadWrite::DataType::UInt8,  VTK_UNSIGNED_CHAR      },
  { h5::H5ReadWrite::DataType::UInt16, VTK_UNSIGNED_SHORT     },
  { h5::H5ReadWrite::DataType::UInt32, VTK_UNSIGNED_INT       },
  { h5::H5ReadWrite::DataType::UInt64, VTK_UNSIGNED_LONG_LONG },
  { h5::H5ReadWrite::DataType::Float,  VTK_FLOAT              },
  { h5::H5ReadWrite::DataType::Double, VTK_DOUBLE             }
};

int dataTypeToVTK(h5::H5ReadWrite::DataType& type)
{
  auto it = DataTypeToVTK.find(type);

  if (it == DataTypeToVTK.end()) {
    cerr << "Could not convert DataType to VTK!\n";
    return -1;
  }

  return it->second;
}

h5::H5ReadWrite::DataType VTKToDataType(int type)
{
  auto it = DataTypeToVTK.cbegin();
  while (it != DataTypeToVTK.cend()) {

    if (it->second == type)
      return it->first;

    ++it;
  }

  cerr << "Failed to convert VTK to DataType\n";
  return h5::H5ReadWrite::DataType::None;
}

bool EmdFormat::read(const std::string& fileName, vtkImageData* image)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  bool ok;
  reader.attribute<unsigned int>("/", "version_major", &ok);

  if (!ok) {
    cout << "Failed to find version_major" << endl;
  }

  reader.attribute<unsigned int>("/", "version_minor", &ok);
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
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::WriteOnly;
  H5ReadWrite writer(fileName, mode);

  // Now to create the attributes, groups, etc.
  writer.setAttribute("/", "version_major", 0u);
  writer.setAttribute("/", "version_minor", 2u);

  // Now create a "data" group
  writer.createGroup("/data");
  writer.createGroup("/data/tomography");

  // Now create the emd_group_type attribute.
  writer.setAttribute("/data/tomography", "emd_group_type", 1u);

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

  int dim[3] = { 0, 0, 0 };
  permutedImage->GetDimensions(dim);
  std::vector<int> dims({dim[0], dim[1], dim[2]});

  // We must allocate a new array, and copy the reordered array into it.
  auto arrayPtr = permutedImage->GetPointData()->GetScalars();
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

  H5ReadWrite::DataType type = VTKToDataType(arrayPtr->GetDataType());

  writer.writeData("/data/tomography", "data", dims, type, outPtr);

  // Create the 3 dim sets too...
  std::vector<int> side(1);
  side[0] = imageDimDataX.size();
  writer.writeData("/data/tomography", "dim1", side, imageDimDataX);
  if (hasTiltAngles) {
    writer.setAttribute("/data/tomography/dim1", "name", "angles");
    writer.setAttribute("/data/tomography/dim1", "units", "[deg]");
  }
  else {
    writer.setAttribute("/data/tomography/dim1", "name", "x");
    writer.setAttribute("/data/tomography/dim1", "units", "[n_m]");
  }

  side[0] = imageDimDataY.size();
  writer.writeData("/data/tomography", "dim2", side, imageDimDataY);
  writer.setAttribute("/data/tomography/dim2", "name", "y");
  writer.setAttribute("/data/tomography/dim2", "units", "[n_m]");

  side[0] = imageDimDataZ.size();
  writer.writeData("/data/tomography", "dim3", side, imageDimDataZ);
  if (hasTiltAngles) {
    writer.setAttribute("/data/tomography/dim3", "name", "x");
    writer.setAttribute("/data/tomography/dim3", "units", "[n_m]");
  } else {
    writer.setAttribute("/data/tomography/dim3", "name", "z");
    writer.setAttribute("/data/tomography/dim3", "units", "[n_m]");
  }

  return true;
}
} // namespace tomviz
