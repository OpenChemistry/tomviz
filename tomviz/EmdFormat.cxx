/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "EmdFormat.h"

#include "DataSource.h"
#include "GenericHDF5Format.h"

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

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

// Forward declarations
static bool writeExtraScalars(h5::H5ReadWrite& writer, vtkImageData* image);
static void readExtraScalars(h5::H5ReadWrite& reader,
                             const std::string& emdNode, vtkImageData* image);

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

bool EmdFormat::read(const std::string& fileName, vtkImageData* image,
                     const QJsonObject& options)
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

  if (!GenericHDF5Format::readVolume(reader, emdDataNode, image, options)) {
    std::cerr << "Failed to read the volume at " << emdDataNode << "\n";
    return false;
  }

  // If it has a "name" attribute, let's name the scalar that
  if (reader.hasAttribute(emdDataNode, "name")) {
    auto name = reader.attribute<std::string>(emdDataNode, "name", &ok);
    if (ok) {
      auto scalars = image->GetPointData()->GetScalars();
      scalars->SetName(name.c_str());
    }
  }

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

  // Now read in any extra scalars
  readExtraScalars(reader, emdNode, image);

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

  GenericHDF5Format::writeVolume(writer, "/data/tomography", "data",
                                 permutedImage);

  // Set a "name" attribute on the data so we can remember the
  // scalar name that the user gave it.
  std::string activeName =
    permutedImage->GetPointData()->GetScalars()->GetName();
  writer.setAttribute("/data/tomography/data", "name", activeName.c_str());

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
      imageDimDataX[i] = i * spacing[0];
    }
  }
  for (int i = 0; i < dimensions[1]; ++i) {
    imageDimDataY[i] = i * spacing[1];
  }
  for (int i = 0; i < dimensions[2]; ++i) {
    imageDimDataZ[i] = i * spacing[2];
  }

  // Create the 3 dim sets too...
  std::vector<int> side(1);
  side[0] = imageDimDataX.size();
  writer.writeData("/data/tomography", "dim1", side, imageDimDataX);
  if (hasTiltAngles) {
    writer.setAttribute("/data/tomography/dim1", "name", "angles");
    writer.setAttribute("/data/tomography/dim1", "units", "[deg]");
  } else {
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

  // Write any extra scalars we might have
  writeExtraScalars(writer, permutedImage);

  return true;
}

static void readExtraScalars(h5::H5ReadWrite& reader,
                             const std::string& emdNode, vtkImageData* image)
{
  std::string scalarsPath = emdNode + "/tomviz_scalars";
  if (!reader.isGroup(scalarsPath)) {
    // No extra scalars
    return;
  }

  auto datasets = reader.allDataSets(scalarsPath);
  for (const auto& name : datasets) {
    auto path = scalarsPath + "/" + name;
    GenericHDF5Format::addScalarArray(reader, path, image, name);
  }
}

static bool writeExtraScalars(h5::H5ReadWrite& writer, vtkImageData* image)
{
  std::string path = "/data/tomography/tomviz_scalars";
  writer.createGroup(path);

  vtkPointData* pointData = image->GetPointData();

  // Skip over the one we have already written
  std::string activeName(pointData->GetScalars()->GetName());

  // Write out all other scalars
  int numArrays = pointData->GetNumberOfArrays();
  for (int i = 0; i < numArrays; i++) {
    auto arrayName = pointData->GetArrayName(i);
    // Skip over the one we have already written
    if (arrayName == activeName)
      continue;

    // Make it active and write it
    pointData->SetActiveScalars(arrayName);
    GenericHDF5Format::writeVolume(writer, path, arrayName, image);
  }

  // Make the original one active again
  pointData->SetActiveScalars(activeName.c_str());
  return true;
}

} // namespace tomviz
