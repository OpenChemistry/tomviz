/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "GenericHDF5Format.h"

#include <DataExchangeFormat.h>
#include <Hdf5SubsampleWidget.h>

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QStringList>
#include <QVBoxLayout>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

#include <string>
#include <vector>

#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

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

  // This is the easiest way I could find to get the size of the type
  int size = vtkDataArray::GetDataTypeSize(vtkDataType);

  // Get the dimensions
  std::vector<int> dims = reader.getDimensions(path);

  int bs[6];
  int stride = 1;

  bool anyOver = std::any_of(dims.cbegin(), dims.cend(), [this](int i) {
    return i >= m_subsampleDimOverride;
  });
  if (m_askForSubsample || anyOver) {
    int dimensions[3] = { dims[0], dims[1], dims[2] };
    QDialog dialog;
    dialog.setWindowTitle("Pick Subsample");
    QVBoxLayout layout;
    dialog.setLayout(&layout);

    Hdf5SubsampleWidget widget(dimensions, size);
    layout.addWidget(&widget);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(&buttons);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog,
                     &QDialog::accept);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog,
                     &QDialog::reject);

    // Check if the user cancels
    if (!dialog.exec())
      return false;

    widget.bounds(bs);
    stride = widget.stride();
  } else {
    // Set the bounds to the default values
    for (int i = 0; i < 3; ++i) {
      bs[i * 2] = 0;
      bs[i * 2 + 1] = dims[i];
    }
  }

  // Set up the stride and counts
  size_t start[3] = { static_cast<size_t>(bs[0]), static_cast<size_t>(bs[2]),
                      static_cast<size_t>(bs[4]) };
  size_t counts[3];
  for (size_t i = 0; i < 3; ++i)
    counts[i] = (bs[i * 2 + 1] - start[i]) / stride;

  // vtk requires the counts to be an int array
  int vtkCounts[3];
  for (int i = 0; i < 3; ++i)
    vtkCounts[i] = counts[i];

  vtkNew<vtkImageData> tmp;
  tmp->SetDimensions(&vtkCounts[0]);
  tmp->AllocateScalars(vtkDataType, 1);
  image->SetDimensions(&vtkCounts[0]);
  image->AllocateScalars(vtkDataType, 1);

  if (!reader.readData(path, type, tmp->GetScalarPointer(), stride, start,
                       counts)) {
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
                                           &vtkCounts[0]));
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
    return deFormat.read(fileName, image, m_askForSubsample);
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
    QStringList items;
    for (auto& d : datasets)
      items.append(QString::fromStdString(d));

    bool ok;
    QString res = QInputDialog::getItem(
      nullptr, "Choose volume", "Choose volume to load:", items, 0, false, &ok);

    // Check if user canceled
    if (!ok)
      return false;

    dataNode = datasets[items.indexOf(res)];
  }

  return readVolume(reader, dataNode, image);
}

} // namespace tomviz
