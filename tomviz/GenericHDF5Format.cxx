/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "GenericHDF5Format.h"

#include <DataExchangeFormat.h>

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

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

bool GenericHDF5Format::readVolume(h5::H5ReadWrite& reader,
                                   const std::string& path, vtkImageData* image)
{
  // Get the type of the data
  h5::H5ReadWrite::DataType type = reader.dataType(path);
  int vtkDataType = h5::H5VtkTypeMaps::dataTypeToVtk(type);

  // Get the dimensions
  std::vector<int> dims = reader.getDimensions(path);

  // Check if one of the dimensions is greater than 1100
  // If so, we will use a stride of 2.
  // TODO: make this an option in the UI
  int stride = 1;
  for (const auto& dim : dims) {
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

  if (!reader.readData(path, type, tmp->GetScalarPointer(), stride)) {
    std::cerr << "Failed to read the data\n";
    return false;
  }

  // HDF5 typically stores data as row major order.
  // VTK expects column major order.
  auto inPtr = tmp->GetPointData()->GetScalars()->GetVoidPointer(0);
  auto outPtr = image->GetPointData()->GetScalars()->GetVoidPointer(0);
  switch (image->GetPointData()->GetScalars()->GetDataType()) {
    vtkTemplateMacro(tomviz::ReorderArrayF(reinterpret_cast<VTK_TT*>(inPtr),
                                           reinterpret_cast<VTK_TT*>(outPtr),
                                           &dims[0]));
    default:
      cout << "Generic HDF5 Format: Unknown data type" << endl;
  }
  image->Modified();

  return true;
}

bool GenericHDF5Format::read(const std::string& fileName, vtkImageData* image)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  // If /exchange/data is a dataset, assume this is a DataExchangeFormat
  if (reader.isDataSet("/exchange/data")) {
    reader.close();
    DataExchangeFormat deFormat;
    return deFormat.read(fileName, image);
  }

  // Find all 3D datasets. If there is more than one, have the user choose.
  std::vector<std::string> datasets = reader.allDataSets();
  for (auto it = datasets.begin(); it != datasets.end();) {
    // Remove all non-3D datasets
    std::vector<int> dims = reader.getDimensions(*it);
    if (dims.size() != 3)
      datasets.erase(it);
    else
      ++it;
  }

  if (datasets.empty()) {
    std::cerr << "No 3D datasets found in " << fileName.c_str() << "\n";
    return false;
  }

  std::string dataNode = datasets[0];

  if (datasets.size() != 1) {
    // If there is more than one dataset, have the user choose one
    QDialog dialog;
    QVBoxLayout layout;
    dialog.setLayout(&layout);
    dialog.setWindowTitle("Choose volume");
    QLabel label("Choose volume to load:");
    layout.addWidget(&label);

    QComboBox combo;
    for (const auto& dataset : datasets)
      combo.addItem(dataset.c_str());
    layout.addWidget(&combo);

    QDialogButtonBox okButton(QDialogButtonBox::Ok);
    layout.addWidget(&okButton);
    layout.setAlignment(&okButton, Qt::AlignCenter);
    QObject::connect(&okButton, &QDialogButtonBox::accepted, &dialog,
                     &QDialog::accept);

    dialog.exec();
    dataNode = datasets[combo.currentIndex()];
  }

  return GenericHDF5Format::readVolume(reader, dataNode, image);
}

} // namespace tomviz
