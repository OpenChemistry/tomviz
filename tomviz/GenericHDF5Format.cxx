/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "GenericHDF5Format.h"

#include <DataExchangeFormat.h>
#include <DataSource.h>
#include <Hdf5SubsampleWidget.h>
#include <Utilities.h>

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

bool GenericHDF5Format::addScalarArray(h5::H5ReadWrite& reader,
                                       const std::string& path,
                                       vtkImageData* image,
                                       const std::string& name)
{
  // Get the type of the data
  h5::H5ReadWrite::DataType type = reader.dataType(path);
  int vtkDataType = h5::H5VtkTypeMaps::dataTypeToVtk(type);

  // Get the dimensions
  std::vector<int> dims = reader.getDimensions(path);

  // Set up the stride and counts
  int stride = 1;
  size_t start[3], counts[3];
  if (DataSource::wasSubsampled(image)) {
    // If the main image was subsampled, we need to use the same
    // subsampling for the scalars
    stride = DataSource::subsampleStride(image);
    int bs[6];
    DataSource::subsampleVolumeBounds(image, bs);

    for (int i = 0; i < 3; ++i) {
      start[i] = static_cast<size_t>(bs[i * 2]);
      counts[i] = (bs[i * 2 + 1] - start[i]) / stride;
    }
  } else {
    for (int i = 0; i < 3; ++i) {
      start[i] = 0;
      counts[i] = dims[i];
    }
  }

  // vtk requires the counts to be an int array
  int vtkCounts[3];
  for (int i = 0; i < 3; ++i)
    vtkCounts[i] = counts[i];

  vtkNew<vtkImageData> tmp;
  tmp->SetDimensions(&vtkCounts[0]);
  tmp->AllocateScalars(vtkDataType, 1);

  if (!reader.readData(path, type, tmp->GetScalarPointer(), stride, start,
                       counts)) {
    std::cerr << "Failed to read the data\n";
    return false;
  }

  auto* array = vtkAbstractArray::CreateArray(vtkDataType);
  array->SetNumberOfTuples(counts[0] * counts[1] * counts[2]);
  array->SetName(name.c_str());
  image->GetPointData()->AddArray(array);

  // HDF5 typically stores data as row major order.
  // VTK expects column major order.
  auto inPtr = tmp->GetPointData()->GetScalars()->GetVoidPointer(0);
  auto outPtr = array->GetVoidPointer(0);
  switch (vtkDataType) {
    vtkTemplateMacro(tomviz::ReorderArrayF(reinterpret_cast<VTK_TT*>(inPtr),
                                           reinterpret_cast<VTK_TT*>(outPtr),
                                           &vtkCounts[0]));
    default:
      cout << "Generic HDF5 Format: Unknown data type" << endl;
  }
  image->Modified();

  return true;
}

bool GenericHDF5Format::readVolume(h5::H5ReadWrite& reader,
                                   const std::string& path, vtkImageData* image,
                                   const QVariantMap& options)
{
  // Get the type of the data
  h5::H5ReadWrite::DataType type = reader.dataType(path);
  int vtkDataType = h5::H5VtkTypeMaps::dataTypeToVtk(type);

  // This is the easiest way I could find to get the size of the type
  int size = vtkDataArray::GetDataTypeSize(vtkDataType);

  // Get the dimensions
  std::vector<int> dims = reader.getDimensions(path);

  int bs[6] = { -1, -1, -1, -1, -1, -1 };
  int stride = 1;
  if (options.contains("subsampleVolumeBounds")) {
    // Get the subsample volume bounds if the caller specified them
    QVariantList list = options["subsampleVolumeBounds"].toList();
    for (int i = 0; i < list.size() && i < 6; ++i)
      bs[i] = list[i].toInt();
    DataSource::setWasSubsampled(image, true);
    DataSource::setSubsampleVolumeBounds(image, bs);
  } else {
    // Set it to the defaults
    for (int i = 0; i < 3; ++i) {
      bs[i * 2] = 0;
      bs[i * 2 + 1] = dims[i];
    }
  }

  if (options.contains("subsampleStride")) {
    // Get the stride if the caller specified it
    stride = options["subsampleStride"].toInt();
    if (stride == 0)
      stride = 1;

    DataSource::setWasSubsampled(image, true);
    DataSource::setSubsampleStride(image, stride);
  }

  bool askForSubsample = false;
  if (options.contains("askForSubsample")) {
    // If the options specify whether to ask for a subsample, use that
    askForSubsample = options["askForSubsample"].toBool();
  } else {
    // Otherwise, only ask for a subsample if the data looks large
    int subsampleDimOverride = 1200;
    if (options.contains("subsampleDimOverride"))
      subsampleDimOverride = options["subsampleDimOverride"].toInt();

    askForSubsample =
      std::any_of(dims.cbegin(), dims.cend(), [subsampleDimOverride](int i) {
        return i >= subsampleDimOverride;
      });
  }

  if (askForSubsample) {
    int dimensions[3] = { dims[0], dims[1], dims[2] };
    QDialog dialog;
    dialog.setWindowTitle("Pick Subsample");
    QVBoxLayout layout;
    dialog.setLayout(&layout);

    Hdf5SubsampleWidget widget(dimensions, size);
    layout.addWidget(&widget);

    if (DataSource::wasSubsampled(image)) {
      // If it was previously subsampled, start with the previous values
      widget.setStride(DataSource::subsampleStride(image));

      DataSource::subsampleVolumeBounds(image, bs);
      widget.setBounds(bs);
    }

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                             QDialogButtonBox::Help);
    layout.addWidget(&buttons);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog,
                     &QDialog::accept);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog,
                     &QDialog::reject);
    QObject::connect(&buttons, &QDialogButtonBox::helpRequested,
                     []() { openHelpUrl("data/#hdf5-subsampling"); });

    // Check if the user cancels
    if (!dialog.exec())
      return false;

    widget.bounds(bs);
    stride = widget.stride();

    DataSource::setWasSubsampled(image, true);
    DataSource::setSubsampleStride(image, stride);
    DataSource::setSubsampleVolumeBounds(image, bs);
  }

  // Do one final check to make sure none of the bounds are less than 0
  if (std::any_of(std::begin(bs), std::end(bs), [](int i) { return i < 0; })) {
    // Set them to their defaults so we don't seg fault
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

bool GenericHDF5Format::read(const std::string& fileName, vtkImageData* image,
                             const QVariantMap& options)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  // If /exchange/data is a dataset, assume this is a DataExchangeFormat
  if (reader.isDataSet("/exchange/data")) {
    reader.close();
    DataExchangeFormat deFormat;
    return deFormat.read(fileName, image, options);
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

bool GenericHDF5Format::writeVolume(h5::H5ReadWrite& writer,
                                    const std::string& path,
                                    const std::string& name,
                                    vtkImageData* image)
{
  int dim[3];
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
      cout << "Generic HDF5 Format: Unknown data type" << endl;
  }

  h5::H5ReadWrite::DataType type =
    h5::H5VtkTypeMaps::VtkToDataType(arrayPtr->GetDataType());

  return writer.writeData(path, name, dims, type, outPtr);
}

} // namespace tomviz
