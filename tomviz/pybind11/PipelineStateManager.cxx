/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineStateManager.h"
#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "OperatorFactory.h"
#include "OperatorPython.h"
#include "PipelineManager.h"
#include "PythonUtilities.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

using namespace tomviz;

Pipeline* findPipeline(QStringList& path, QString id = QString())
{
  if (path.size() < 2) {
    qCritical() << "Path doesn't have enough parts.";
    return nullptr;
  }

  auto iterator = path.begin();
  auto objType = *(iterator++);

  if (objType != "dataSources") {
    qCritical() << "Path doesn't start with 'dataSources'.";
    return nullptr;
  }

  auto pipelineIndexStr = *(iterator++);
  bool success = false;
  auto pipelineIndex = pipelineIndexStr.toInt(&success);
  if (!success) {
    qCritical() << "Unable to convert index to int.";
    return nullptr;
  }

  auto& pipelineMgr = PipelineManager::instance();
  auto pipelines = pipelineMgr.pipelines();

  if (pipelineIndex >= pipelines.size()) {
    qCritical() << "Pipeline index no longer exists.";
    return nullptr;
  }

  // Now check the ids
  auto dataSource = pipelines[pipelineIndex]->dataSource();
  QString currentId =
    QString().sprintf("%p", static_cast<const void*>(dataSource));
  if (!id.isEmpty() && currentId != id) {
    qCritical() << "Pipeline no longer exists.";
    return nullptr;
  }

  // Remove consumed parts of the path
  path.erase(path.begin(), iterator);

  return pipelines[pipelineIndex];
}

Pipeline* findPipeline(const QString& path, QString id = QString())
{
  auto parts = path.split("/", QString::SkipEmptyParts);

  return findPipeline(parts, id);
}

Operator* findOperator(QStringList& path, QString id = QString())
{
  auto pipeline = findPipeline(path);
  if (pipeline == nullptr) {
    qCritical() << "Unable to find pipeline.";
    return nullptr;
  }

  if (path.size() < 2) {
    qCritical() << "Path doesn't have enough parts.";
    return nullptr;
  }

  auto iterator = path.begin();
  auto objType = *(iterator++);
  if (objType != "operators") {
    qCritical() << "Path doesn't contain 'operators'.";
    return nullptr;
  }

  auto opIndexStr = *(iterator++);
  bool success = false;
  auto opIndex = opIndexStr.toInt(&success);
  if (!success) {
    qCritical() << "Unable to convert index to int.";
    return nullptr;
  }

  auto operators = pipeline->dataSource()->operators();
  if (opIndex >= operators.size()) {
    qCritical() << "Operator index no longer exists.";
    return nullptr;
  }

  // Now check the ids
  auto op = operators[opIndex];
  QString currentId = QString().sprintf("%p", static_cast<const void*>(op));
  if (!id.isEmpty() && currentId != id) {
    qCritical() << "Operator no longer exists.";
    return nullptr;
  }

  // Remove consumed parts of the path
  path.erase(path.begin(), iterator);

  return op;
}

Operator* findOperator(const QString& path, QString id = QString())
{
  auto parts = path.split("/", QString::SkipEmptyParts);

  return findOperator(parts, id);
}

DataSource* findDataSource(QStringList& path, QString id = QString())
{
  auto copy = QStringList(path);
  auto pipeline = findPipeline(copy);
  if (pipeline == nullptr) {
    qCritical() << "Unable to find pipeline.";
    return nullptr;
  }

  // If the path no longer contains 'dataSources' we are done (its the root data
  // source)
  if (!copy.contains("dataSources")) {
    path.erase(path.begin(), path.begin() + 2);

    // Now check the ids
    auto dataSource = pipeline->dataSource();
    QString currentId =
      QString().sprintf("%p", static_cast<const void*>(dataSource));
    if (!id.isEmpty() && currentId != id) {
      qCritical() << "Datasource no longer exists.";
      return nullptr;
    }

    return dataSource;
  }

  // Find the operator (the child data source has to be assocated with one ...)
  auto op = findOperator(path);
  if (op == nullptr) {
    qCritical() << "Unable to find operator.";
    return nullptr;
  }

  auto dataSource = op->childDataSource();
  QString currentId =
    QString().sprintf("%p", static_cast<const void*>(dataSource));
  if (!id.isEmpty() && currentId != id) {
    qCritical() << "Datasource no longer exists.";
    return nullptr;
  }

  // Remove the consumed data source
  path.erase(path.begin(), path.begin() + 2);

  return dataSource;
}

DataSource* findDataSource(const QString& path, QString id = QString())
{
  auto parts = path.split("/", QString::SkipEmptyParts);

  return findDataSource(parts, id);
}

Module* findModule(QStringList& path, QString id = QString())
{
  // First find the data source the module is attached to.
  auto dataSource = findDataSource(path);
  if (dataSource == nullptr) {
    qCritical() << "Unable to find data source.";
    return nullptr;
  }

  if (path.size() < 2) {
    qCritical() << "Path doesn't have enough parts.";
    return nullptr;
  }

  auto iterator = path.begin();
  auto objType = *(iterator++);
  if (objType != "modules") {
    qCritical() << "Path doesn't contain 'modules'.";
    return nullptr;
  }

  auto modIndexStr = *(iterator++);
  bool success = false;
  auto modIndex = modIndexStr.toInt(&success);
  if (!success) {
    qCritical() << "Unable to convert index to int.";
    return nullptr;
  }

  auto& moduleMgr = ModuleManager::instance();
  // TODO: We should use the view information here as well ...
  auto modules = moduleMgr.findModulesGeneric(
    dataSource, ActiveObjects::instance().activeView());

  if (modIndex >= modules.size()) {
    qCritical() << "Module index no longer exists.";
    return nullptr;
  }

  auto module = modules[modIndex];
  QString currentId = QString().sprintf("%p", static_cast<const void*>(module));
  if (!id.isEmpty() && currentId != id) {
    qCritical() << "Module no longer exists.";
    return nullptr;
  }

  return module;
}

Module* findModule(const QString& path, QString id = QString())
{
  auto parts = path.split("/", QString::SkipEmptyParts);

  return findModule(parts, id);
}

/*PipelineStateManager& PipelineStateManager::instance()
{
  static PipelineStateManager theInstance;
  return theInstance;
}*/

PipelineStateManager::PipelineStateManager()
{
  QObject::connect(&ModuleManager::instance(), &ModuleManager::dataSourceAdded,
                   [this](DataSource* ds) {
                     QObject::connect(ds, &DataSource::dataChanged, [this]() {
                       this->syncToPython();
                     });

                     QObject::connect(
                       ds, &DataSource::operatorAdded, [this](Operator* op) {
                         QObject::connect(op, &Operator::transformModified,
                                          [this]() { this->syncToPython(); });
                       });

                     QObject::connect(ds, &DataSource::operatorRemoved,
                                      [this]() { this->syncToPython(); });
                   });

  QObject::connect(&ModuleManager::instance(),
                   &ModuleManager::dataSourceRemoved,
                   [this]() { this->syncToPython(); });

  QObject::connect(&ModuleManager::instance(), &ModuleManager::moduleAdded,
                   [this](Module* module) {
                     QObject::connect(module, &Module::renderNeeded, [this]() {
                       this->syncToPython();
                     });
                   });

  QObject::connect(&ModuleManager::instance(), &ModuleManager::moduleRemoved,
                   [this]() {
                     this->syncToPython();
                   });

  QObject::connect(&ModuleManager::instance(), &ModuleManager::visibilityChanged, [this]() {
    this->syncToPython();
  });
}

void PipelineStateManager::syncToPython()
{
  Python python;
  auto tomvizState = python.import("tomviz.state");
  if (!tomvizState.isValid()) {
    qCritical() << "Failed to import tomviz.state";
  }

  auto sync = tomvizState.findFunction("_sync_to_python");
  if (!sync.isValid()) {
    qCritical() << "Unable to locate _sync_to_python.";
  }

  auto state = serialize();

  Python::Tuple args(1);
  Python::Object pyState(state);
  args.set(0, pyState);
  Python::Dict kwargs;
  sync.call(args, kwargs);
}

std::string PipelineStateManager::serialize()
{
  QJsonObject state;
  QFileInfo info(QApplication::applicationDirPath());
  auto success = ModuleManager::instance().serialize(state, info.dir(), false);

  if (!success) {
    qCritical() << "Serialization failed ...";
  }

  QJsonDocument doc(state);
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

void PipelineStateManager::load(const std::string& state,
                                const std::string& stateRelDir)
{
  auto relDir = QDir(QByteArray::fromStdString(stateRelDir));
  auto json = QByteArray::fromStdString(state);
  auto doc = QJsonDocument::fromJson(json);

  emit(&ModuleManager::instance())->enablePythonConsole(false);
  // QEventLoop loop;
  QObject::connect(
    &ModuleManager::instance(), &ModuleManager::stateDoneLoading, []() {
      tomviz::Python python;

      auto stateModule = python.import("tomviz.state");
      if (!stateModule.isValid()) {
        qCritical() << "Failed to import tomviz.state";
      }

      auto tomviz = stateModule.findFunction("_init");
      if (!tomviz.isValid()) {
        qCritical() << "Unable to locate _init.";
      }

      Python::Tuple args(0);
      Python::Dict kwargs;
      tomviz.call(args, kwargs);

      emit(&ModuleManager::instance())->enablePythonConsole(true);
    });
  ModuleManager::instance().deserialize(doc.object(), relDir);
  // loop.exec();
}

std::string PipelineStateManager::modulesJson()
{
  vtkNew<vtkImageData> data;
  data->SetDimensions(2, 2, 2);
  data->AllocateScalars(VTK_INT, 1);
  auto dataSource = new DataSource(data);

  QJsonObject modules;

  for (auto t : ModuleFactory::moduleTypes()) {
    auto module = ModuleFactory::createModule(
      t, dataSource, ActiveObjects::instance().activeView());
    module->setVisibility(false);
    QFileInfo info(QApplication::applicationDirPath());
    auto state = module->serialize();
    modules[t] = state;
    module->deleteLater();
  }

  QJsonDocument doc(modules);
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  // dataSource->deleteLater();

  return stateByteArray.toStdString();
}

std::string PipelineStateManager::operatorsJson()
{
  vtkNew<vtkImageData> data;
  data->SetDimensions(2, 2, 2);
  data->AllocateScalars(VTK_INT, 1);
  auto dataSource = new DataSource(data);

  QJsonObject operators;
  auto operatorTypes = OperatorFactory::instance().operatorTypes();
  // Remove Python we will deal with these separately
  operatorTypes.removeAll("Python");
  for (auto& type : operatorTypes) {
    auto op = OperatorFactory::instance().createOperator(type, dataSource);
    auto state = op->serialize();
    operators[type] = state;
    op->deleteLater();
  }

  // Now process the python operators
  for (auto& info : OperatorFactory::instance().registeredPythonOperators()) {
    auto op = new OperatorPython(dataSource);
    auto doc = QJsonDocument::fromJson(info.json.toUtf8());
    auto name = doc.object()["name"].toString();
    // If not name is provide camelCase the label
    if (name.isEmpty()) {
      auto parts = info.label.split(" ");
      auto iter = parts.begin();
      name = *(iter++);

      for (; iter != parts.end(); iter++) {
        auto part = *iter;
        part[0] = part[0].toUpper();
        name += part;
      }
    }
    op->setJSONDescription(info.json);
    op->setLabel(info.label);
    op->setScript(info.source);
    auto state = op->serialize();
    operators[name] = state;
    op->deleteLater();
  }

  dataSource->deleteLater();

  QJsonDocument doc(operators);
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

std::string PipelineStateManager::serializeOperator(const std::string& path,
                                                    const std::string& id)
{
  auto p = QString::fromStdString(path);
  auto i = QString::fromStdString(id);
  auto op = findOperator(p, i);
  if (op == nullptr) {
    return "";
  }

  QJsonDocument doc(op->serialize());
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

void PipelineStateManager::updateOperator(const std::string& path,
                                          const std::string& state)
{
  auto p = QString::fromStdString(path);
  auto json = QByteArray::fromStdString(state);
  auto doc = QJsonDocument::fromJson(json);
  auto id = doc.object()["id"].toString();
  auto op = findOperator(p, id);
  if (op == nullptr) {
    return;
  }
  op->deserialize(doc.object());
}

std::string PipelineStateManager::serializeModule(const std::string& path,
                                                  const std::string& id)
{
  auto p = QString::fromStdString(path);
  auto i = QString::fromStdString(id);
  auto module = findModule(p, i);
  if (module == nullptr) {
    return "";
  }

  QJsonDocument doc(module->serialize());
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

void PipelineStateManager::updateModule(const std::string& path,
                                        const std::string& state)
{
  auto p = QString::fromStdString(path);
  auto json = QByteArray::fromStdString(state);
  auto doc = QJsonDocument::fromJson(json);
  auto id = doc.object()["id"].toString();
  auto module = findModule(p, id);
  if (module == nullptr) {
    return;
  }
  module->deserialize(doc.object());
}


std::string PipelineStateManager::serializeDataSource(const std::string& path, const std::string& id)
{
  auto p = QString::fromStdString(path);
  auto i = QString::fromStdString(id);
  auto ds = findDataSource(p, i);
  if (ds == nullptr) {
    return "";
  }

  QJsonDocument doc(ds->serialize());
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

void PipelineStateManager::updateDataSource(const std::string& path,
                                            const std::string& state)
{
  auto p = QString::fromStdString(path);
  auto json = QByteArray::fromStdString(state);
  auto doc = QJsonDocument::fromJson(json);
  auto id = doc.object()["id"].toString();
  auto ds = findDataSource(p, id);
  if (ds == nullptr) {
    return;
  }
  ds->deserialize(doc.object());
}

void PipelineStateManager::modified(std::vector<std::string> opPaths,
                                    std::vector<std::string> modulePaths)
{
  for (auto path : opPaths) {
    auto p = QString::fromStdString(path);
    auto op = findOperator(p);
    emit op->transformModified();
  }

  for (auto path : modulePaths) {
    auto p = QString::fromStdString(path);
    auto mod = findModule(p);
    emit mod->renderNeeded();
  }
}

std::string PipelineStateManager::addModule(const std::string& dataSourcePath,
                                            const std::string& dataSourceId,
                                            const std::string& moduleType)
{
  auto p = QString::fromStdString(dataSourcePath);
  auto t = QString::fromStdString(moduleType);
  auto i = QString::fromStdString(dataSourceId);
  auto dataSource = findDataSource(p, i);
  auto module = ModuleManager::instance().createAndAddModule(
    t, dataSource, ActiveObjects::instance().activeView());

  if (module == nullptr)
    return "";

  QJsonDocument doc(module->serialize());
  auto stateByteArray = doc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

std::string PipelineStateManager::addOperator(const std::string& dataSourcePath,
                                       const std::string& dataSourceId,
                                       const std::string& opState)
{
  auto p = QString::fromStdString(dataSourcePath);
  auto i = QString::fromStdString(dataSourceId);
  auto json = QByteArray::fromStdString(opState);
  auto doc = QJsonDocument::fromJson(json);
  auto opJson = doc.object();
  auto dataSource = findDataSource(p, i);
  auto type = opJson["type"].toString();

  auto op = OperatorFactory::instance().createOperator(type, dataSource);
  if (op == nullptr) {
    qCritical() << "Failed to create operator.";
    return "";
  }
  if (op->deserialize(opJson)) {
    dataSource->addOperator(op);
  } else {
    qCritical() << "Failed to deserialize operator.";
  }

  QJsonDocument newDoc(op->serialize());
  auto stateByteArray = newDoc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

std::string PipelineStateManager::addDataSource(const std::string& dataSourceState)
{
  auto json = QByteArray::fromStdString(dataSourceState);
  auto doc = QJsonDocument::fromJson(json);
  auto obj = doc.object();
  auto ds = ModuleManager::instance().loadDataSource(obj);

  QJsonDocument newDoc(ds->serialize());
  auto stateByteArray = newDoc.toJson(QJsonDocument::Compact);

  return stateByteArray.toStdString();
}

void PipelineStateManager::removeOperator(const std::string& opPath,
                                          const std::string& dataSourceId,
                                          const std::string& opId)
{
  auto p = QString::fromStdString(opPath);
  auto i = QString::fromStdString(dataSourceId);
  auto o = QString::fromStdString(opId);

  // Are we removing them all?
  if (o.isEmpty()) {
    auto ds = findDataSource(p, i);
    if (ds == nullptr) {
      qCritical() << "Failed to find data source.";
      return;
    }

    auto ops = ds->operators();
    while (ops.size() > 0) {
      auto lastOperator = ops.takeLast();
      ModuleManager::instance().removeOperator(lastOperator);
    }
  } else {
    auto op = findOperator(p, o);
    if (op == nullptr) {
      qCritical() << "Failed to find operator.";
      return;
    }
    ModuleManager::instance().removeOperator(op);
  }
}

void PipelineStateManager::removeModule(const std::string& modulePath,
                                        const std::string& dataSourceId,
                                        const std::string& moduleId)
{
  auto p = QString::fromStdString(modulePath);
  auto i = QString::fromStdString(dataSourceId);
  auto m = QString::fromStdString(moduleId);

  // Are we removing them all?
  if (m.isEmpty()) {
    auto ds = findDataSource(p, i);
    if (ds == nullptr) {
      qCritical() << "Failed to find data source.";
      return;
    }
    ModuleManager::instance().removeAllModules(ds);
  } else {
    auto module = findModule(p, m);
    if (module == nullptr) {
      qCritical() << "Failed to find module.";
      return;
    }
    ModuleManager::instance().removeModule(module);
  }
}

void PipelineStateManager::removeDataSource(const std::string& dataSourcePath,
                                            const std::string& dataSourceId)
{
  auto p = QString::fromStdString(dataSourcePath);
  auto i = QString::fromStdString(dataSourceId);

  // Are we removing them all?
  if (i.isEmpty()) {
    ModuleManager::instance().removeAllDataSources();
  } else {
    auto ds = findDataSource(p, i);
    if (ds == nullptr) {
      qCritical() << "Failed to find data source.";
      return;
    }
    ModuleManager::instance().removeDataSource(ds);
  }
}