/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Tvh5Format.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EmdFormat.h"
#include "LoadDataReaction.h"
#include "ModuleManager.h"
#include "Pipeline.h"

#include <h5cpp/h5readwrite.h>

#include <vtkImageData.h>
#include <vtkNew.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

using std::cerr;
using std::endl;

namespace tomviz {

bool Tvh5Format::write(const std::string& fileName)
{
  // First, write the standard EMD file
  DataSource* source = ActiveObjects::instance().activeDataSource();

  if (!EmdFormat::write(fileName, source)) {
    cerr << "Failed to write the standard EMD node" << endl;
    return false;
  }

  // We will create a soft link to the active id later
  auto activeId = source->id().toStdString();

  // Now, write the state file string to a dataset
  QFileInfo info(fileName.c_str());
  QJsonObject stateObject;
  auto success =
    ModuleManager::instance().serialize(stateObject, info.dir(), false);
  if (!success) {
    cerr << "Failed to serialize the state of Tomviz" << endl;
    return false;
  }

  QByteArray state = QJsonDocument(stateObject).toJson();

  // Write the state file to "tomviz_state"
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadWrite;
  H5ReadWrite writer(fileName, mode);

  if (!writer.writeData("/", "tomviz_state", { static_cast<int>(state.size()) }, state.data())) {
    cerr << "Failed to write tomviz_state" << endl;
    return false;
  }

  // Now, write all the data sources
  writer.createGroup("/tomviz_datasources");
  auto sources = ModuleManager::instance().allDataSources();
  for (auto* ds : sources) {
    if (!ds) {
      // Somehow, invalid data sources ended up in here...
      continue;
    }

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
    if (!EmdFormat::writeNode(writer, group, ds->imageData())) {
      cerr << "Failed to write data source: " << id << endl;
      return false;
    }
  }

  return true;
}

bool Tvh5Format::read(const std::string& fileName)
{
  // Read the state from "tomviz_state"
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName, mode);

  auto stateVec = reader.readData<char>("tomviz_state");
  QString stateStr = std::string(stateVec.begin(), stateVec.end()).c_str();

  auto doc = QJsonDocument::fromJson(stateStr.toUtf8());
  if (doc.isNull()) {
    cerr << "Failed to read state from: " << fileName << endl;
    return false;
  }

  QJsonObject state = doc.object();
  QFileInfo info(fileName.c_str());
  auto success =
    ModuleManager::instance().deserialize(state, info.dir(), false);

  if (!success) {
    cerr << "Failed to deserialize state from: " << fileName << endl;
    return false;
  }

  // Turn off automatic execution of pipelines
  bool prev = ModuleManager::instance().executePipelinesOnLoad();
  ModuleManager::instance().executePipelinesOnLoad(false);

  // Get the active data source
  DataSource* active = nullptr;

  // Now load in the data sources
  if (state["dataSources"].isArray()) {
    auto dataSources = state["dataSources"].toArray();
    foreach (auto ds, dataSources) {
      loadDataSource(reader, ds.toObject(), &active);
    }
  }
  ModuleManager::instance().executePipelinesOnLoad(prev);

  if (active) {
    // Set the active data source if one was flagged as active
    // We have to use "setSelectedDataSource" instead of
    // "setActiveDataSource" or else the Histogram color map won't
    // match.
    ActiveObjects::instance().setSelectedDataSource(active);
  }

  // Loading the modules most likely modified the view. Restore
  // the view to the state given in the state file.
  ModuleManager::instance().setViews(state["views"].toArray());

  return true;
}

bool Tvh5Format::loadDataSource(h5::H5ReadWrite& reader,
                                const QJsonObject& dsObject,
                                DataSource** active, Operator* parent)
{
  auto id = dsObject.value("id").toString().toStdString();
  if (id.empty()) {
    cerr << "Failed to obtain id from data source object" << endl;
    return false;
  }

  // First, create the image data
  std::string path = "/tomviz_datasources/" + id;
  vtkNew<vtkImageData> image;
  QVariantMap options = { { "askForSubsample", false } };
  if (!EmdFormat::readNode(reader, path, image, options)) {
    cerr << "Failed to read data at: " << path << endl;
    return false;
  }

  // Next, create the data source
  DataSource::DataSourceType type = DataSource::hasTiltAngles(image)
                                      ? DataSource::TiltSeries
                                      : DataSource::Volume;

  auto* pipeline = parent ? parent->dataSource()->pipeline() : nullptr;
  auto* dataSource = new DataSource(image, type, pipeline);

  // Save this info in case we write the data source in the future
  dataSource->setFileName(reader.fileName().c_str());
  dataSource->setTvh5NodePath(path.c_str());

  if (parent) {
    // This is a child data source. Hook it up to the operator parent.
    parent->setChildDataSource(dataSource);
    parent->setHasChildDataSource(true);
    parent->newChildDataSource(dataSource);
    // If it has a parent, it will be deserialized later.
  } else {
    // This is a root data source
    LoadDataReaction::dataSourceAdded(dataSource, false, false);
    dataSource->deserialize(dsObject);
  }

  // Set the active data source
  if (dsObject.value("active").toBool()) {
    *active = dataSource;
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
          loadDataSource(reader, s.toObject(), active, opPtrs[i]);
        }
      }
    }
  }

  // Mark all parent operators as complete
  for (auto* op : dataSource->operators())
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
