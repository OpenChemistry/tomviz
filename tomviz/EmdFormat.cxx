/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "EmdFormat.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "GenericHDF5Format.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "Pipeline.h"

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTrivialProducer.h>
#include <vtkTypeInt8Array.h>

#include <vtkSMSourceProxy.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cassert>
#include <string>
#include <vector>

#include <iostream>

namespace tomviz {

// Forward declarations
static bool writeExtraScalars(h5::H5ReadWrite& writer,
                              const std::string& groupPath,
                              vtkImageData* image);
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
                     const QVariantMap& options)
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
  if (emdNode.length() == 0) {
    // We couldn't find a valid EMD node, exit early.
    return false;
  }

  return readNode(reader, emdNode, image, options);
}

bool EmdFormat::readNode(h5::H5ReadWrite& reader, const std::string& emdNode,
                         vtkImageData* image, const QVariantMap& options)
{
  bool ok;
  std::string emdDataNode = emdNode + "/data";
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

  // Now read in any extra scalars
  readExtraScalars(reader, emdNode, image);

  // If this is a tilt series, swap the X and Z axes
  if (!angles.isEmpty()) {
    GenericHDF5Format::swapXAndZAxes(image);
    DataSource::setTiltAngles(image, angles);
    DataSource::setType(image, DataSource::TiltSeries);
  }

  return true;
}

bool EmdFormat::write(const std::string& fileName, DataSource* source)
{
  return this->write(fileName, source->imageData());
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

  return writeNode(writer, "/data/tomography", image);
}

bool EmdFormat::writeNode(h5::H5ReadWrite& writer, const std::string& path,
                          vtkImageData* image)
{
  // Create the emd_group_type attribute.
  writer.setAttribute(path, "emd_group_type", 1u);

  // See if we have tilt angles
  auto hasTiltAngles = DataSource::hasTiltAngles(image);

  // If this is a tilt series, swap the X and Z axes
  vtkSmartPointer<vtkImageData> permutedImage = image;
  if (DataSource::hasTiltAngles(image)) {
    permutedImage = vtkImageData::New();
    permutedImage->ShallowCopy(image);
    GenericHDF5Format::swapXAndZAxes(permutedImage);
  }

  GenericHDF5Format::writeVolume(writer, path, "data", permutedImage);

  // Set a "name" attribute on the data so we can remember the
  // scalar name that the user gave it.
  std::string activeName =
    permutedImage->GetPointData()->GetScalars()->GetName();
  writer.setAttribute(path + "/data", "name", activeName.c_str());

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
  writer.writeData(path, "dim1", side, imageDimDataX);
  if (hasTiltAngles) {
    writer.setAttribute(path + "/dim1", "name", "angles");
    writer.setAttribute(path + "/dim1", "units", "[deg]");
  } else {
    writer.setAttribute(path + "/dim1", "name", "x");
    writer.setAttribute(path + "/dim1", "units", "[n_m]");
  }

  side[0] = imageDimDataY.size();
  writer.writeData(path, "dim2", side, imageDimDataY);
  writer.setAttribute(path + "/dim2", "name", "y");
  writer.setAttribute(path + "/dim2", "units", "[n_m]");

  side[0] = imageDimDataZ.size();
  writer.writeData(path, "dim3", side, imageDimDataZ);
  if (hasTiltAngles) {
    writer.setAttribute(path + "/dim3", "name", "x");
    writer.setAttribute(path + "/dim3", "units", "[n_m]");
  } else {
    writer.setAttribute(path + "/dim3", "name", "z");
    writer.setAttribute(path + "/dim3", "units", "[n_m]");
  }

  // Write any extra scalars we might have
  writeExtraScalars(writer, path, permutedImage);

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

  // Note: the following function does not include soft/external links
  auto datasets = reader.allDataSets(scalarsPath);
  for (const auto& name : datasets) {
    auto path = scalarsPath + "/" + name;
    if (reader.isSoftLink(path)) {
      // Skip over soft links. Although reader.allDataSets() should not
      // return soft links, we add this check here in case it does in
      // the future.
      continue;
    }

    GenericHDF5Format::addScalarArray(reader, path, image, name);
  }
}

static bool writeExtraScalars(h5::H5ReadWrite& writer,
                              const std::string& groupPath,
                              vtkImageData* image)
{
  std::string path = groupPath + "/tomviz_scalars";
  writer.createGroup(path);

  vtkPointData* pointData = image->GetPointData();

  // Make a soft link to the one we have already written
  std::string activeName(pointData->GetScalars()->GetName());

  // Write out all other scalars
  int numArrays = pointData->GetNumberOfArrays();
  for (int i = 0; i < numArrays; i++) {
    auto arrayName = pointData->GetArrayName(i);
    if (arrayName == activeName) {
      // Make a soft link to the one we have already written
      writer.createSoftLink(groupPath + "/data", path + "/" + arrayName);
      continue;
    }

    // Make it active and write it
    pointData->SetActiveScalars(arrayName);
    GenericHDF5Format::writeVolume(writer, path, arrayName, image);
  }

  // Make the original one active again
  pointData->SetActiveScalars(activeName.c_str());
  return true;
}

bool EmdFormat::writeFullState(const std::string& fileName)
{
  // First, write the standard EMD file
  DataSource* source = ActiveObjects::instance().activeDataSource();
  write(fileName, source);

  // We will create a soft link to the active id later
  auto activeId = source->id().toStdString();

  // Now, write the state file string to a dataset
  QFileInfo info(fileName.c_str());
  QJsonObject stateObject;
  auto success = ModuleManager::instance().serialize(stateObject,
                                                     info.dir(), false);
  if (!success) {
    std::cerr << "Failed to serialize the state of Tomviz" << std::endl;
    return false;
  }

  QByteArray state = QJsonDocument(stateObject).toJson();

  // Write the state file to "tomviz_state"
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadWrite;
  H5ReadWrite writer(fileName, mode);

  if (!writer.writeData("/", "tomviz_state", { state.size() }, state.data())) {
    std::cerr << "Failed to write tomviz_state" << std::endl;
    return false;
  }

  // Now, write all the data sources
  writer.createGroup("/tomviz_datasources");
  auto sources = ModuleManager::instance().allDataSources();
  for (auto* ds: sources) {
    // Name the group after its id
    auto id = ds->id().toStdString();

    if (id == activeId) {
      // Make a soft link rather than writing the data again
      writer.createSoftLink("/data/tomography", "/tomviz_datasources/" + id);
      continue;
    }

    std::string group = "/tomviz_datasources/" + id;
    writer.createGroup(group);

    // Write the data here
    if (!writeNode(writer, group, ds->imageData())) {
      std::cerr << "Failed to write data source: " << id << std::endl;
      return false;
    }
  }

  return true;
}

bool EmdFormat::readFullState(const std::string& fileName)
{
  // Read the state from "tomviz_state"
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName, mode);

  auto stateVec = reader.readData<char>("tomviz_state");
  QString stateStr = std::string(stateVec.begin(), stateVec.end()).c_str();

  auto doc = QJsonDocument::fromJson(stateStr.toUtf8());
  if (doc.isNull()) {
    std::cerr << "Failed to read state from: " << fileName << std::endl;
    return false;
  }

  QJsonObject state = doc.object();
  QFileInfo info(fileName.c_str());
  auto success = ModuleManager::instance().deserialize(state, info.dir(),
                                                       false);

  if (!success) {
    std::cerr << "Failed to deserialize state from: " << fileName << std::endl;
    return false;
  }

  // Turn off automatic execution of pipelines
  bool prev = ModuleManager::instance().executePipelinesOnLoad();
  ModuleManager::instance().executePipelinesOnLoad(false);

  // Now load in the data sources
  if (state["dataSources"].isArray()) {
    auto dataSources = state["dataSources"].toArray();
    foreach (auto ds, dataSources) {
      loadDataSource(reader, ds.toObject());
    }
  }

  ModuleManager::instance().executePipelinesOnLoad(prev);

  // Loading the modules most likely modified the view. Restore
  // the view to the state given in the state file.
  ModuleManager::instance().setViews(state["views"].toArray());

  return true;
}

bool EmdFormat::loadDataSource(h5::H5ReadWrite& reader,
                               const QJsonObject& dsObject,
                               Operator* parent)
{
  auto id = dsObject.value("id").toString().toStdString();
  if (id.empty()) {
    std::cerr << "Failed to obtain id from data source object" << std::endl;
    return false;
  }

  // First, create the image data
  std::string path = "/tomviz_datasources/" + id;
  vtkNew<vtkImageData> image;
  QVariantMap options = { { "askForSubsample", false } };
  if (!readNode(reader, path, image, options)) {
    std::cerr << "Failed to read data at: " << path << std::endl;
    return false;
  }

  // Next, create the data source
  DataSource::DataSourceType type = DataSource::hasTiltAngles(image)
                                      ? DataSource::TiltSeries
                                      : DataSource::Volume;

  auto* pipeline = parent ? parent->dataSource()->pipeline() : nullptr;
  auto* dataSource = new DataSource(image, type, pipeline,
                                    DataSource::PersistenceState::Transient);
  if (parent) {
    // This is a child data source. Hook it up to the operator parent.
    parent->setChildDataSource(dataSource);
    parent->setHasChildDataSource(true);
    parent->newChildDataSource(dataSource);
    // If it has a parent, it will be deserialized later.
  }
  else {
    // This is a root data source
    LoadDataReaction::dataSourceAdded(dataSource, false, false);
    dataSource->deserialize(dsObject);
  }

  // If there is no label, try to make one from the reader file name
  if (dataSource->label().isEmpty() && dsObject.contains("reader")) {
    auto fileNames = dsObject["reader"].toObject()["fileNames"].toArray();
    if (!fileNames.empty()) {
      dataSource->setLabel(QFileInfo(fileNames[0].toString()).baseName());
    }
  }

  // Set the active data source
  if (dsObject.value("active").toBool()) {
    ActiveObjects::instance().setActiveDataSource(dataSource);
  }

  // If there are operators, load child data sources too
  if (dsObject["operators"].isArray()) {
    auto opPtrs = dataSource->operators();
    auto operators = dsObject["operators"].toArray();
    for (int i = 0; i < operators.size() && i < opPtrs.size(); ++i) {
      auto op = operators.at(i).toObject();
      if (op["dataSources"].isArray()) {
        auto sources = op["dataSources"].toArray();
        foreach (auto s, sources) {
          loadDataSource(reader, s.toObject(), opPtrs[i]);
        }
      }
    }
  }

  // Mark all parent operators as complete
  for (auto* op: dataSource->operators())
    op->setComplete();

  if (pipeline) {
    // Make sure the pipeline is not paused in case the user wishes to
    // re-run some operators.
    pipeline->resume();
    // This will deserialize all children.
    pipeline->finished();
  }

  return true;
}

} // namespace tomviz
