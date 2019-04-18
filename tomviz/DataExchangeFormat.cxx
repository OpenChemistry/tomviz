/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataExchangeFormat.h"

#include "DataSource.h"

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTrivialProducer.h>

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
        out[(i * dim[1] + j) * dim[2] + k] = in[(k * dim[1] + j) * dim[0] + i];
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
        out[(k * dim[1] + j) * dim[0] + i] = in[(i * dim[1] + j) * dim[2] + k];
      }
    }
  }
}

bool DataExchangeFormat::read(const std::string& fileName, vtkImageData* image)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  std::string deDataNode = "/exchange/data";
  // If it isn't a data set, we are done
  if (!reader.isDataSet(deDataNode))
    return false;

  // Get the type of the data
  h5::H5ReadWrite::DataType type = reader.dataType(deDataNode);
  int vtkDataType = h5::H5VtkTypeMaps::dataTypeToVtk(type);

  // Get the dimensions
  std::vector<int> dims = reader.getDimensions(deDataNode);

  // Check if one of the dimensions is greater than 1100
  // If so, we will use a stride of 2.
  // TODO: make this an option in the UI
  int stride = 1;
  for (const auto& dim: dims) {
    if (dim > 1100) {
      stride = 2;
      std::cout << "Using a stride of " << stride << " because the data "
                << "set is very large\n";
      break;
    }
  }

  // Re-shape the dimensions according to the stride
  for (auto& dim : dims)
    dim /= stride;

  vtkNew<vtkImageData> tmp;
  tmp->SetDimensions(&dims[0]);
  tmp->AllocateScalars(vtkDataType, 1);
  image->SetDimensions(&dims[0]);
  image->AllocateScalars(vtkDataType, 1);

  if (!reader.readData(deDataNode, type, tmp->GetScalarPointer(), stride)) {
    cerr << "Failed to read the data\n";
    return false;
  }

  // Data Exchange stores data as row major order.
  // VTK expects column major order.
  auto inPtr = tmp->GetPointData()->GetScalars()->GetVoidPointer(0);
  auto outPtr = image->GetPointData()->GetScalars()->GetVoidPointer(0);
  switch (image->GetPointData()->GetScalars()->GetDataType()) {
    vtkTemplateMacro(tomviz::ReorderArrayF(reinterpret_cast<VTK_TT*>(inPtr),
                                           reinterpret_cast<VTK_TT*>(outPtr),
                                           &dims[0]));
    default:
      cout << "Data Exchange Format: Unknown data type" << endl;
  }
  image->Modified();

  return true;
}

bool DataExchangeFormat::write(const std::string& fileName, DataSource* source)
{
  auto t = source->producer();
  auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  return this->write(fileName, image);
}

bool DataExchangeFormat::write(const std::string& fileName, vtkImageData* image)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::WriteOnly;
  H5ReadWrite writer(fileName, mode);

  // Now create an "/exchange" group
  writer.createGroup("/exchange");

  int dim[3] = { 0, 0, 0 };
  image->GetDimensions(dim);
  std::vector<int> dims({ dim[0], dim[1], dim[2] });

  // We must allocate a new array, and copy the reordered array into it.
  auto arrayPtr = image->GetPointData()->GetScalars();
  auto dataPtr = arrayPtr->GetVoidPointer(0);
  vtkNew<vtkImageData> reorderedImageData;
  reorderedImageData->SetDimensions(dim);
  reorderedImageData->AllocateScalars(arrayPtr->GetDataType(), 1);
  auto outPtr =
    reorderedImageData->GetPointData()->GetScalars()->GetVoidPointer(0);

  switch (arrayPtr->GetDataType()) {
    vtkTemplateMacro(tomviz::ReorderArrayC(reinterpret_cast<VTK_TT*>(dataPtr),
                                           reinterpret_cast<VTK_TT*>(outPtr),
                                           dim));
    default:
      cout << "Data Exchange Format: Unknown data type" << endl;
  }

  H5ReadWrite::DataType type =
    h5::H5VtkTypeMaps::VtkToDataType(arrayPtr->GetDataType());

  return writer.writeData("/exchange", "data", dims, type, outPtr);
}

} // namespace tomviz
