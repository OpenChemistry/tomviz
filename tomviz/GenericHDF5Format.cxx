/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "GenericHDF5Format.h"

#include <DataExchangeFormat.h>
#include <DataSource.h>
#include <Hdf5SubsampleWidget.h>
#include <Utilities.h>

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkImagePermute.h>
#include <vtkPointData.h>

#include <string>
#include <vector>

#include <iostream>
#include <sstream>

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
        out[static_cast<size_t>(i * dim[1] + j) * dim[2] + k] =
          in[static_cast<size_t>(k * dim[1] + j) * dim[0] + i];
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
        out[static_cast<size_t>(k * dim[1] + j) * dim[0] + i] =
          in[static_cast<size_t>(i * dim[1] + j) * dim[2] + k];
      }
    }
  }
}

template <typename T>
void readAngleArray(vtkDataArray* array, QVector<double>& angles)
{
  auto* data = static_cast<T*>(array->GetVoidPointer(0));
  for (int i = 0; i < array->GetNumberOfTuples(); ++i)
    angles.append(data[i]);
}

QVector<double> GenericHDF5Format::readAngles(h5::H5ReadWrite& reader,
                                              const std::string& path,
                                              const QVariantMap& options)
{
  Q_UNUSED(options)

  QVector<double> angles;
  if (!reader.isDataSet(path)) {
    std::cerr << "No angles at: " + path + "\n";
    return angles;
  }

  // Get the type of the data
  h5::H5ReadWrite::DataType type = reader.dataType(path);
  int vtkDataType = h5::H5VtkTypeMaps::dataTypeToVtk(type);

  // This should only be one dimension
  std::vector<int> dims = reader.getDimensions(path);
  if (dims.size() != 1) {
    std::cerr << "Exactly one dimension is required to read angles.\n";
    return angles;
  }

  vtkSmartPointer<vtkDataArray> array =
    vtkDataArray::CreateDataArray(vtkDataType);
  array->SetNumberOfTuples(dims[0]);
  if (!reader.readData(path, type, array->GetVoidPointer(0))) {
    std::cerr << "Failed to read the angles\n";
    return angles;
  }

  switch (array->GetDataType()) {
    // This is done so we can ensure we read the type correctly...
    vtkTemplateMacro(readAngleArray<VTK_TT>(array, angles));
  }

  return angles;
}

void GenericHDF5Format::swapXAndZAxes(vtkImageData* image)
{
  // TODO: vtkImagePermute currently only swaps the axes of the active
  // scalars. This can cause some big problems for the other scalars,
  // since the dimensions of the image also change.
  // Until this gets fixed in VTK, we have a complicated work-around
  // that involves swapping the scalars one-by-one and then setting
  // them back on the original image.
  auto* pd = image->GetPointData();
  std::string activeName = pd->GetScalars()->GetName();

  int dim[3];
  double spacing[3], origin[3];
  image->GetDimensions(dim);
  image->GetSpacing(spacing);
  image->GetOrigin(origin);

  vtkNew<vtkImagePermute> permute;
  permute->SetFilteredAxes(2, 1, 0);

  // Extract all of the arrays from the image data,
  // and swap each of their axes.
  std::vector<vtkSmartPointer<vtkDataArray>> arrays;
  while (pd->GetNumberOfArrays() != 0) {
    auto* name = pd->GetArrayName(0);
    vtkSmartPointer<vtkDataArray> array = pd->GetScalars(name);
    pd->RemoveArray(name);

    vtkNew<vtkImageData> tmp;
    tmp->SetDimensions(dim);
    tmp->GetPointData()->SetScalars(array);

    permute->SetInputData(tmp);
    permute->Update();
    arrays.push_back(permute->GetOutput()->GetPointData()->GetScalars());
  }

  // There should be no data left in the image. Go ahead and
  // swap the dimensions before adding the data back in.
  image->SetDimensions(dim[2], dim[1], dim[0]);
  image->SetSpacing(spacing[2], spacing[1], spacing[0]);
  image->SetOrigin(origin[2], origin[1], origin[0]);

  // Add them back into the image
  while (arrays.size() != 0) {
    pd->AddArray(arrays.back());
    arrays.pop_back();
  }

  pd->SetActiveScalars(activeName.c_str());
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

  // Set up the strides and counts
  int strides[3] = { 1, 1, 1 };
  size_t start[3], counts[3];
  if (DataSource::wasSubsampled(image)) {
    // If the main image was subsampled, we need to use the same
    // subsampling for the scalars
    DataSource::subsampleStrides(image, strides);
    int bs[6];
    DataSource::subsampleVolumeBounds(image, bs);

    for (int i = 0; i < 3; ++i) {
      start[i] = static_cast<size_t>(bs[i * 2]);
      counts[i] = (bs[i * 2 + 1] - start[i]) / strides[i];
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

  // Make sure the dimensions match those of the image, or else
  // we will probably experience a crash later...
  int imageDims[3];
  image->GetDimensions(imageDims);
  for (int i = 0; i < 3; ++i) {
    if (vtkCounts[i] != imageDims[i]) {
      std::stringstream ss;
      if (DataSource::wasSubsampled(image)) {
        ss << "Subsampled dimensions of ";
      } else {
        ss << "Dimensions of ";
      }
      ss << path << " (" << vtkCounts[0] << ", " << vtkCounts[1] << ", "
         << vtkCounts[2] << ") do not match the dimensions of the image ("
         << imageDims[0] << ", " << imageDims[1] << ", " << imageDims[2]
         << ")\nThe array cannot be added.";

      std::cerr << "Error in GenericHDF5Format::addScalarArray():\n"
                << ss.str() << std::endl;
      QMessageBox::critical(nullptr, "Dimensions do not match",
                            ss.str().c_str());
      return false;
    }
  }

  vtkNew<vtkImageData> tmp;
  tmp->SetDimensions(&vtkCounts[0]);
  tmp->AllocateScalars(vtkDataType, 1);

  if (!reader.readData(path, type, tmp->GetScalarPointer(), strides, start,
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

bool GenericHDF5Format::isDataExchange(const std::string& fileName)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  // If /exchange/data is a dataset, assume this is a DataExchangeFormat
  return reader.isDataSet("/exchange/data");
}

bool GenericHDF5Format::isFxi(const std::string& fileName)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  // Look for a few keys, loosely defined BNL FXI data format.
  return reader.isDataSet("/img_tomo") && reader.isDataSet("/img_bkg");
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

  if (dims.size() != 3) {
    std::cerr << "Error: " << path
              << " does not have three dimensions." << std::endl;
    return false;
  }

  int bs[6] = { -1, -1, -1, -1, -1, -1 };
  int strides[3] = { 1, 1, 1 };
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

  if (options.contains("subsampleStrides")) {
    // Get the strides if the caller specified them
    QVariantList list = options["subsampleStrides"].toList();
    for (int i = 0; i < list.size() && i < 3; ++i) {
      strides[i] = list[i].toInt();
      if (strides[i] < 1)
        strides[i] = 1;
    }

    DataSource::setWasSubsampled(image, true);
    DataSource::setSubsampleStrides(image, strides);
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
      DataSource::subsampleStrides(image, strides);
      widget.setStrides(strides);

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
    widget.strides(strides);

    DataSource::setWasSubsampled(image, true);
    DataSource::setSubsampleStrides(image, strides);
    DataSource::setSubsampleVolumeBounds(image, bs);
  }

  // Do one final check to make sure all bounds are valid
  bool changed = false;
  for (int i = 0; i < 3; ++i) {
    if (bs[i * 2 + 1] < 0 || bs[i * 2 + 1] > dims[i]) {
      // Upper bound is not valid. Reset it.
      bs[i * 2 + 1] = dims[i];
      changed = true;
    }
    if (bs[i * 2] < 0 || bs[i * 2] > bs[i * 2 + 1]) {
      // Lower bound is not valid. Reset it.
      bs[i * 2] = 0;
      changed = true;
    }
  }

  if (changed) {
    // Update the volume bounds that were used
    DataSource::setSubsampleVolumeBounds(image, bs);
  }

  // Set up the strides and counts
  size_t start[3] = { static_cast<size_t>(bs[0]), static_cast<size_t>(bs[2]),
                      static_cast<size_t>(bs[4]) };
  size_t counts[3];
  for (size_t i = 0; i < 3; ++i)
    counts[i] = (bs[i * 2 + 1] - start[i]) / strides[i];

  // vtk requires the counts to be an int array
  int vtkCounts[3];
  for (int i = 0; i < 3; ++i)
    vtkCounts[i] = counts[i];

  vtkNew<vtkImageData> tmp;
  tmp->SetDimensions(&vtkCounts[0]);
  tmp->AllocateScalars(vtkDataType, 1);
  image->SetDimensions(&vtkCounts[0]);
  image->AllocateScalars(vtkDataType, 1);

  if (!reader.readData(path, type, tmp->GetScalarPointer(), strides, start,
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
  Q_UNUSED(options)

  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

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

  if (datasets.size() == 1) {
    // Only one volume. Load and return.
    return readVolume(reader, dataNode, image);
  }

  // If there is more than one volume, have the user choose.
  QStringList items;
  for (auto& d : datasets)
    items.append(QString::fromStdString(d));

  // Choose the volumes to load
  QDialog dialog;
  dialog.setWindowTitle("Choose volumes");
  QVBoxLayout layout;
  dialog.setLayout(&layout);

  // Use a scroll area in case there are a lot of options
  QScrollArea scrollArea;
  scrollArea.setWidgetResizable(true); // Necessary for some reason
  layout.addWidget(&scrollArea);

  QWidget scrollAreaWidget;
  QVBoxLayout scrollAreaLayout;
  scrollAreaWidget.setLayout(&scrollAreaLayout);
  scrollArea.setWidget(&scrollAreaWidget);

  // Add the checkboxes
  QList<QCheckBox*> checkboxes;
  for (const auto& item : items) {
    checkboxes.append(new QCheckBox(item, &scrollAreaWidget));
    scrollAreaLayout.addWidget(checkboxes.back());
  }

  // Check the first checkbox
  if (!checkboxes.empty())
    checkboxes[0]->setChecked(true);

  // Setup Ok and Cancel buttons
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  layout.addWidget(&buttons);
  QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog,
                   &QDialog::accept);
  QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);

  if (!dialog.exec()) {
    // User canceled, just return
    return false;
  }

  // Find the checked checkboxes
  std::vector<std::string> selectedDatasets;
  for (const auto& cb : checkboxes) {
    if (cb->isChecked()) {
      // Take advantage of the fact that the text is the name exactly
      selectedDatasets.push_back(cb->text().toStdString());
    }
  }

  if (selectedDatasets.empty()) {
    QString msg = "At least one volume must be selected";
    std::cerr << msg.toStdString() << std::endl;
    QMessageBox::critical(nullptr, "Invalid Selection", msg);
    return false;
  }

  // Read the first dataset with readVolume(). This might ask for
  // subsampling options, which will be applied to the rest of the
  // datasets.
  if (!readVolume(reader, selectedDatasets[0], image)) {
    auto msg =
      QString("Failed to read the data at: ") + selectedDatasets[0].c_str();
    std::cerr << msg.toStdString() << std::endl;
    QMessageBox::critical(nullptr, "Read Failed", msg);
    return false;
  }

  // Set the name of the first array
  auto activeName = selectedDatasets[0];
  image->GetPointData()->GetScalars()->SetName(activeName.c_str());

  // Add any more datasets with addScalarArray()
  for (size_t i = 1; i < selectedDatasets.size(); ++i) {
    const auto& path = selectedDatasets[i];
    if (!addScalarArray(reader, path, image, path)) {
      auto msg = QString("Failed to read or add the data of: ") + path.c_str();
      std::cerr << msg.toStdString() << std::endl;
      QMessageBox::critical(nullptr, "Failure", msg);
      return false;
    }
  }

  // Look for some common places where there are angles, and
  // load in the angles if we find them.
  QVector<double> angles;
  std::vector<std::string> placesToSearch = { "angle", "angles" };
  for (const auto& path : placesToSearch) {
    if (reader.isDataSet(path)) {
      angles = readAngles(reader, path, options);
      break;
    }
  }

  if (!angles.isEmpty()) {
    swapXAndZAxes(image);
    DataSource::setTiltAngles(image, angles);
    DataSource::setType(image, DataSource::TiltSeries);
  }

  // Made it to the end...
  return true;
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
