/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "Pipeline.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "DockerUtilities.h"
#include "EmdFormat.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "PipelineWorker.h"
#include "ProgressDialog.h"
#include "Utilities.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaEnum>
#include <QObject>
#include <QTextStream>
#include <QTimer>
#include <QtConcurrent>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>

namespace tomviz {

PipelineSettings::PipelineSettings()
{
  m_settings = pqApplicationCore::instance()->settings();
}

void PipelineSettings::setExecutionMode(Pipeline::ExecutionMode executor)
{
  auto metaEnum = QMetaEnum::fromType<Pipeline::ExecutionMode>();
  setExecutionMode(QString(metaEnum.valueToKey(executor)));
}

void PipelineSettings::setExecutionMode(const QString& executor)
{
  m_settings->setValue("pipeline/mode", executor);
}

Pipeline::ExecutionMode PipelineSettings::executionMode()
{
  if (!m_settings->contains("pipeline/mode")) {
    return Pipeline::ExecutionMode::Threaded;
  }
  auto metaEnum = QMetaEnum::fromType<Pipeline::ExecutionMode>();

  return static_cast<Pipeline::ExecutionMode>(metaEnum.keyToValue(
    m_settings->value("pipeline/mode").toString().toLatin1().data()));
}

QString PipelineSettings::dockerImage()
{
  return m_settings->value("pipeline/docker.image").toString();
}

bool PipelineSettings::dockerPull()
{
  return m_settings->value("pipeline/docker.pull", true).toBool();
}

bool PipelineSettings::dockerRemove()
{
  return m_settings->value("pipeline/docker.remove", true).toBool();
}

void PipelineSettings::setDockerImage(const QString& image)
{
  m_settings->setValue("pipeline/docker.image", image);
}

void PipelineSettings::setDockerPull(bool pull)
{
  m_settings->setValue("pipeline/docker.pull", pull);
}

void PipelineSettings::setDockerRemove(bool remove)
{
  m_settings->setValue("pipeline/docker.remove", remove);
}

Pipeline::Pipeline(DataSource* dataSource, QObject* parent) : QObject(parent)
{
  m_data = dataSource;
  m_data->setParent(this);

  addDataSource(dataSource);

  PipelineSettings settings;
  auto executor = settings.executionMode();
  setExecutionMode(executor);
}

Pipeline::~Pipeline() = default;

void Pipeline::execute()
{
  execute(m_data);
}

void Pipeline::execute(DataSource* start, bool last)
{
  emit started();
  Operator* lastOp = nullptr;

  if (last) {
    lastOp = start->operators().last();
  }
  m_executor->execute(start, lastOp);
}

void Pipeline::execute(DataSource* start)
{
  emit started();
  m_executor->execute(start);
}

void Pipeline::branchFinished(DataSource* start, vtkDataObject* newData)
{
  // We only add the transformed child data source if the last operator
  // doesn't already have an explicit child data source i.e.
  // hasChildDataSource is true.
  auto lastOp = start->operators().last();
  if (!lastOp->hasChildDataSource()) {
    DataSource* newChildDataSource = nullptr;
    if (lastOp->childDataSource() == nullptr) {
      newChildDataSource = new DataSource("Output");
      newChildDataSource->setPersistenceState(
        tomviz::DataSource::PersistenceState::Transient);
      newChildDataSource->setForkable(false);
      newChildDataSource->setParent(this);
      lastOp->setChildDataSource(newChildDataSource);
      auto rootDataSource = dataSource();
      // connect signal to flow units and spacing to child data source.
      connect(dataSource(), &DataSource::dataPropertiesChanged,
              [rootDataSource, newChildDataSource]() {
                // Only flow the properties if no user modifications have been
                // made.
                if (!newChildDataSource->unitsModified()) {
                  newChildDataSource->setUnits(rootDataSource->getUnits(),
                                               false);
                  double spacing[3];
                  rootDataSource->getSpacing(spacing);
                  newChildDataSource->setSpacing(spacing, false);
                }
              });
    }

    lastOp->childDataSource()->setData(newData);
    lastOp->childDataSource()->dataModified();

    if (newChildDataSource != nullptr) {
      emit lastOp->newChildDataSource(newChildDataSource);
      // Move modules from root data source.
      bool oldMoveObjectsEnabled =
        ActiveObjects::instance().moveObjectsEnabled();
      ActiveObjects::instance().setMoveObjectsMode(false);
      auto view = ActiveObjects::instance().activeView();
      foreach (Module* module, ModuleManager::instance().findModules<Module*>(
                                 dataSource(), nullptr)) {
        // TODO: We should really copy the module properties as well.
        auto newModule = ModuleManager::instance().createAndAddModule(
          module->label(), newChildDataSource, view);
        // Copy over properties using the serialization code.
        newModule->deserialize(module->serialize());
        ModuleManager::instance().removeModule(module);
      }
      ActiveObjects::instance().setMoveObjectsMode(oldMoveObjectsEnabled);
    }
  }
}

void Pipeline::pause()
{
  m_paused = true;
}

bool Pipeline::paused()
{
  return m_paused;
}


void Pipeline::resume(bool run)
{
  m_paused = false;
  if (run) {
    execute();
  }
}

void Pipeline::resume(DataSource* at)
{
  m_paused = false;
  execute(at);
}

void Pipeline::cancel(std::function<void()> canceled)
{
  m_executor->cancel(canceled);
}

bool Pipeline::isRunning()
{
  return m_executor->isRunning();
}

DataSource* Pipeline::findTransformedDataSource(DataSource* dataSource)
{
  auto op = findTransformedDataSourceOperator(dataSource);
  if (op != nullptr) {
    return op->childDataSource();
  }

  return nullptr;
}

Operator* Pipeline::findTransformedDataSourceOperator(DataSource* dataSource)
{
  if (dataSource == nullptr) {
    return nullptr;
  }

  auto operators = dataSource->operators();
  for (auto itr = operators.rbegin(); itr != operators.rend(); ++itr) {
    auto op = *itr;
    if (op->childDataSource() != nullptr) {
      auto child = op->childDataSource();
      // If the child has operators we need to go deeper
      if (!child->operators().isEmpty()) {
        return findTransformedDataSourceOperator(child);
      }

      return op;
    }
  }

  return nullptr;
}

void Pipeline::addDataSource(DataSource* dataSource)
{
  connect(dataSource, &DataSource::operatorAdded,
          [this](Operator* op) { execute(op->dataSource(), true); });
  // Wire up transformModified to execute pipeline
  connect(dataSource, &DataSource::operatorAdded, [this](Operator* op) {
    // Extract out source and execute all.
    connect(op, &Operator::transformModified, this, [this]() { execute(); });

    // Ensure that new child data source signals are correctly wired up.
    connect(op,
            static_cast<void (Operator::*)(DataSource*)>(
              &Operator::newChildDataSource),
            [this](DataSource* ds) { addDataSource(ds); });

    // We need to ensure we move add datasource to the end of the branch
    auto operators = op->dataSource()->operators();
    if (operators.size() > 1) {
      auto transformedDataSourceOp =
        findTransformedDataSourceOperator(op->dataSource());
      if (transformedDataSourceOp != nullptr) {
        auto transformedDataSource = transformedDataSourceOp->childDataSource();
        transformedDataSourceOp->setChildDataSource(nullptr);
        op->setChildDataSource(transformedDataSource);
        emit operatorAdded(op, transformedDataSource);
      } else {
        emit operatorAdded(op);
      }
    } else {
      emit operatorAdded(op);
    }
  });
  // Wire up operatorRemoved. TODO We need to check the branch of the
  // pipeline we are currently executing.
  connect(dataSource, &DataSource::operatorRemoved, [this](Operator* op) {
    // Do we need to move the transformed data source, !hasChildDataSource as we
    // don't want to move "explicit" child data sources.
    if (!op->hasChildDataSource() && op->childDataSource() != nullptr) {
      auto transformedDataSource = op->childDataSource();
      auto operators = op->dataSource()->operators();
      // We have an operator to move it to.
      if (!operators.isEmpty()) {
        auto newOp = operators.last();
        op->setChildDataSource(nullptr);
        newOp->setChildDataSource(transformedDataSource);
        emit newOp->dataSourceMoved(transformedDataSource);
      }
      // Clean it up
      else {
        transformedDataSource->removeAllOperators();
        transformedDataSource->deleteLater();
      }
    }

    // If pipeline is running see if we can safely remove the operator
    if (isRunning()) {
      // If we can't safely cancel the execution then trigger the rerun of the
      // pipeline.
      if (!m_executor->cancel(op)) {
        execute(op->dataSource());
      }
    } else {
      // Trigger the pipeline to run
      execute(op->dataSource());
    }
  });
}

void Pipeline::addDefaultModules(DataSource* dataSource)
{
  // Note: In the future we can pull this out into a setting.
  QStringList defaultModules = { "Outline", "Orthogonal Slice" };
  bool oldMoveObjectsEnabled = ActiveObjects::instance().moveObjectsEnabled();
  ActiveObjects::instance().setMoveObjectsMode(false);
  auto view = ActiveObjects::instance().activeView();

  Module* module = nullptr;
  foreach (QString name, defaultModules) {
    module =
      ModuleManager::instance().createAndAddModule(name, dataSource, view);
  }
  ActiveObjects::instance().setActiveModule(module);
  ActiveObjects::instance().setMoveObjectsMode(oldMoveObjectsEnabled);

  auto pqview = tomviz::convert<pqView*>(view);
  pqview->resetDisplay();
  pqview->render();
}

Pipeline::ImageFuture::ImageFuture(Operator* op, vtkImageData* imageData,
                                   PipelineWorker::Future* future,
                                   QObject* parent)
  : QObject(parent), m_operator(op), m_imageData(imageData), m_future(future)
{

  if (m_future != nullptr) {
    connect(m_future, &PipelineWorker::Future::finished, this,
            &Pipeline::ImageFuture::finished);
    connect(m_future, &PipelineWorker::Future::canceled, this,
            &Pipeline::ImageFuture::canceled);
  }
}

Pipeline::ImageFuture::~ImageFuture()
{
  if (m_future != nullptr) {
    m_future->deleteLater();
  }
}

Pipeline::ImageFuture* Pipeline::getCopyOfImagePriorTo(Operator* op)
{
  return m_executor->getCopyOfImagePriorTo(op);
}

DataSource* Pipeline::transformedDataSource(DataSource* ds)
{
  if (ds == nullptr) {
    ds = dataSource();
  }

  auto transformed = findTransformedDataSource(ds);
  if (transformed != nullptr) {
    return transformed;
  }

  // Default to dataSource at being of pipeline
  return ds;
}

void Pipeline::setExecutionMode(ExecutionMode executor)
{

  if (executor == ExecutionMode::Docker) {
    m_executor.reset(new DockerPipelineExecutor(this));
  } else {
    m_executor.reset(new ThreadPipelineExecutor(this));
  }
}

PipelineExecutor::PipelineExecutor(Pipeline* pipeline) : QObject(pipeline)
{
}

Pipeline* PipelineExecutor::pipeline()
{
  return qobject_cast<Pipeline*>(parent());
}

bool PipelineExecutor::cancel(Operator* op)
{
  Q_UNUSED(op)
  // Default implementation doesn't allow canceling operators during execution.
  return false;
}

ThreadPipelineExecutor::ThreadPipelineExecutor(Pipeline* pipeline)
  : PipelineExecutor(pipeline)
{
  m_worker = new PipelineWorker(this);
}

void ThreadPipelineExecutor::execute(DataSource* dataSource, Operator* start)
{
  executePipelineBranch(dataSource, start);
}

void ThreadPipelineExecutor::cancel(std::function<void()> canceled)
{
  if (m_future) {
    if (canceled) {
      connect(m_future, &PipelineWorker::Future::canceled, canceled);
    }
    m_future->cancel();
  }
}

bool ThreadPipelineExecutor::cancel(Operator* op)
{
  if (m_future && m_future->isRunning()) {
    return m_future->cancel(op);
  }

  return false;
}

bool ThreadPipelineExecutor::isRunning()
{
  return m_future != nullptr && m_future->isRunning();
}

void ThreadPipelineExecutor::executePipelineBranch(DataSource* dataSource,
                                                   Operator* start)
{
  if (pipeline()->paused()) {
    return;
  }

  auto operators = dataSource->operators();
  if (operators.isEmpty()) {
    emit pipeline()->finished();
    return;
  }

  // Cancel any running operators. TODO in the future we should be able to add
  // operators to end of a running pipeline.
  if (m_future && m_future->isRunning()) {
    m_future->cancel();
  }

  vtkDataObject* data = nullptr;

  if (start != nullptr) {
    // Use the transform DataSource as the starting point.
    auto transformDataSource = pipeline()->transformedDataSource(dataSource);
    data = transformDataSource->copyData();

    // See if we have any canceled operators in the pipeline, if so we have to
    // re-run this branch of the pipeline.
    bool haveCanceled = false;
    for (auto itr = dataSource->operators().begin(); *itr != start; ++itr) {
      auto currentOp = *itr;
      if (currentOp->isCanceled()) {
        currentOp->resetState();
        haveCanceled = true;
        break;
      }
    }

    // If we have canceled operators we have to run call operators.
    if (!haveCanceled) {
      operators = operators.mid(operators.indexOf(start));
      start->resetState();
    }
  }

  m_future = m_worker->run(data, operators);
  data->FastDelete();
  connect(m_future, &PipelineWorker::Future::finished, this,
          &ThreadPipelineExecutor::pipelineBranchFinished);
  connect(m_future, &PipelineWorker::Future::canceled, this,
          &ThreadPipelineExecutor::pipelineBranchCanceled);
}

void ThreadPipelineExecutor::pipelineBranchFinished(bool result)
{
  PipelineWorker::Future* future =
    qobject_cast<PipelineWorker::Future*>(sender());
  if (result) {
    auto lastOp = future->operators().last();

    // TODO Need to refactor and moved to Pipeline ...
    pipeline()->branchFinished(lastOp->dataSource(), future->result());

    // Do we have another branch to execute
    if (lastOp->childDataSource() != nullptr) {
      execute(lastOp->childDataSource());
      // Ensure the pipeline has ownership of the transformed data source.
      lastOp->childDataSource()->setParent(pipeline());
    }
    // The pipeline execution is finished
    else {
      emit pipeline()->finished();
    }

    future->deleteLater();
    if (m_future == future) {
      m_future = nullptr;
    }
  }
}

void ThreadPipelineExecutor::pipelineBranchCanceled()
{
  auto future = qobject_cast<PipelineWorker::Future*>(sender());
  future->deleteLater();
  if (m_future == future) {
    m_future = nullptr;
  }
}

Pipeline::ImageFuture* ThreadPipelineExecutor::getCopyOfImagePriorTo(
  Operator* op)
{
  auto operators = pipeline()->dataSource()->operators();

  // If the op has not been added then we can just use the "Output" data source.
  if (!operators.isEmpty() && !operators.contains(op)) {
    auto transformed = pipeline()->transformedDataSource();
    auto dataObject = vtkImageData::SafeDownCast(transformed->copyData());
    auto imageFuture = new Pipeline::ImageFuture(op, dataObject);
    dataObject->FastDelete();
    // Delay emitting signal until next event loop
    QTimer::singleShot(0, [=] { emit imageFuture->finished(true); });

    return imageFuture;
  } else {
    auto dataSource = pipeline()->dataSource();
    auto dataObject = vtkImageData::SafeDownCast(dataSource->copyData());
    if (operators.size() > 1) {
      auto index = operators.indexOf(op);
      // Only run operators if we have some to run
      if (index > 0) {
        auto future = m_worker->run(dataObject, operators.mid(0, index));
        auto imageFuture = new Pipeline::ImageFuture(op, dataObject, future);
        dataObject->FastDelete();

        return imageFuture;
      }
    }

    auto imageFuture = new Pipeline::ImageFuture(op, dataObject);
    dataObject->FastDelete();

    // Delay emitting signal until next event loop
    QTimer::singleShot(0, [=] { emit imageFuture->finished(true); });

    return imageFuture;
  }
}

const char* TRANSFORM_FILENAME = "transformed.emd";
const char* STATE_FILENAME = "state.tvsm";
const char* CONTAINER_MOUNT = "/tomviz";
const char* PROGRESS_PATH = "progress";

DockerPipelineExecutor::DockerPipelineExecutor(Pipeline* pipeline)
  : PipelineExecutor(pipeline), m_localServer(new QLocalServer(this)),
    m_threadPool(new QThreadPool(this)), m_statusCheckTimer(new QTimer(this))
{
  auto server = m_localServer.data();
  connect(server, &QLocalServer::newConnection, server, [this]() {
    auto connection = m_localServer->nextPendingConnection();
    m_progressConnection.reset(connection);

    if (m_progressConnection) {
      connect(connection, &QIODevice::readyRead, this,
              &DockerPipelineExecutor::progressReady);
      connect(connection, static_cast<void (QLocalSocket::*)(
                            QLocalSocket::LocalSocketError socketError)>(
                            &QLocalSocket::error),
              [this](QLocalSocket::LocalSocketError socketError) {
                if (socketError != QLocalSocket::PeerClosedError) {
                  displayError(
                    "Socket Error",
                    QString("Socket connection error: %1").arg(socketError));
                }
              });
    }
  });

  m_statusCheckTimer->setInterval(5000);
  connect(m_statusCheckTimer, &QTimer::timeout, this,
          &DockerPipelineExecutor::checkContainerStatus);
}

DockerPipelineExecutor::~DockerPipelineExecutor()
{}

void DockerPipelineExecutor::run(const QString& image, const QStringList& args,
                                 const QMap<QString, QString>& bindMounts)
{
  auto runInvocation =
    docker::run(image, QString(), args, bindMounts);
  connect(runInvocation, &docker::DockerRunInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(runInvocation, &docker::DockerRunInvocation::finished, runInvocation,
          [this, runInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
            Q_UNUSED(exitStatus)
            if (exitCode) {
              displayError("Docker Error",
                           QString("Docker run failed with: %1\n\n%2")
                             .arg(exitCode)
                             .arg(runInvocation->stdErr()));
              ;
              return;
            } else {
              m_containerId = runInvocation->containerId();
              // Start to monitor the status of the container
              m_statusCheckTimer->start();
            }
            runInvocation->deleteLater();
          });
}

void DockerPipelineExecutor::remove(const QString& containerId)
{
  auto removeInvocation = docker::remove(containerId);
  connect(removeInvocation, &docker::DockerRunInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(
    removeInvocation, &docker::DockerRunInvocation::finished, removeInvocation,
    [this, removeInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
      Q_UNUSED(exitStatus)
      if (exitCode) {
        displayError("Docker Error",
                     QString("Docker remove failed with: %1\n\n%2")
                       .arg(exitCode)
                       .arg(removeInvocation->stdErr()));
        return;
      }
      removeInvocation->deleteLater();
    });
}

docker::DockerStopInvocation* DockerPipelineExecutor::stop(
  const QString& containerId)
{
  auto stopInvocation = docker::stop(containerId, 0);
  connect(stopInvocation, &docker::DockerStopInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(
    stopInvocation, &docker::DockerStopInvocation::finished, stopInvocation,
    [this, stopInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
      Q_UNUSED(exitStatus)
      if (exitCode) {
        displayError("Docker Error",
                     QString("Docker stop failed with: %1").arg(exitCode));
        return;
      }

      PipelineSettings settings;
      if (settings.dockerRemove()) {
        // Remove the container
        remove(m_containerId);
      }

      stopInvocation->deleteLater();
    });

  return stopInvocation;
}

void DockerPipelineExecutor::execute(DataSource* dataSource, Operator* start)
{

  foreach (Operator* op, pipeline()->dataSource()->operators()) {
    op->setState(OperatorState::Queued);
  }

  m_temporaryDir.reset(new QTemporaryDir());
  if (!m_temporaryDir->isValid()) {
    displayError("Directory Error", "Unable to create temporary directory.");
    return;
  }

  // First serialize the pipeline.
  auto source = dataSource->serialize();
  auto reader = source["reader"].toObject();
  auto fileNames = reader["fileNames"].toArray();

  if (fileNames.count() > 1) {
    displayError("Unsupported", "Docker executor doesn't support stacks.");
    return;
  }

  // Locate data and copy to temp dir
  auto filePath = fileNames[0].toString();
  QFileInfo info(filePath);
  auto fileName = info.fileName();

  QFile::copy(filePath, m_temporaryDir->filePath(fileName));

  // Now update the file path to where the it will appear in the
  // docker container.
  fileNames[0] = QJsonValue(fileName);
  // TODO There must be a better way!
  reader["fileNames"] = fileNames;
  source["reader"] = reader;
  QJsonObject pipeline;
  QJsonArray dataSources;
  dataSources.append(source);
  pipeline["dataSources"] = dataSources;

  // Now write the update state file to the temporary directory.
  QFile stateFile(m_temporaryDir->filePath(STATE_FILENAME));
  if (!stateFile.open(QIODevice::WriteOnly)) {
    displayError("Write Error", "Couldn't open state file for write.");
    return;
  }
  stateFile.write(QJsonDocument(pipeline).toJson());
  stateFile.close();

  // Start listening for local socket connections for progress update from
  // within the docker container.
  m_localServer->close();
  m_localServer->listen(m_temporaryDir->filePath(PROGRESS_PATH));

  // We are now ready to run the pipeline
  auto mount = QDir(CONTAINER_MOUNT);
  auto stateFilePath = mount.filePath(STATE_FILENAME);
  auto outputPath = mount.filePath(TRANSFORM_FILENAME);
  QStringList args;
  args << "-s";
  args << stateFilePath;
  args << "-o";
  args << outputPath;
  args << "-p";
  args << "socket";
  args << "-u";
  args << mount.filePath(PROGRESS_PATH);
  QMap<QString, QString> bindMounts;
  bindMounts[m_temporaryDir->path()] = CONTAINER_MOUNT;
  QString image = "tomviz/pipeline";

  PipelineSettings settings;
  // Pull the latest version of the image, if haven't already
  if (settings.dockerPull() && m_pullImage) {
    auto msg = QString("Pulling docker image: %1").arg(image);
    auto progress =
      new ProgressDialog("Docker Pull", msg, tomviz::mainWidget());
    progress->show();
    m_pullImage = false;
    auto pullInvocation = docker::pull(image);
    connect(pullInvocation, &docker::DockerPullInvocation::error, this,
            &DockerPipelineExecutor::error);
    connect(pullInvocation, &docker::DockerPullInvocation::finished,
            pullInvocation,
            [this, image, args, bindMounts, pullInvocation,
             progress](int exitCode, QProcess::ExitStatus exitStatus) {
              Q_UNUSED(exitStatus)
              progress->hide();
              progress->deleteLater();
              if (exitCode) {
                displayError("Docker Error",
                             QString("Docker pull failed with: %1\n\n%2")
                               .arg(exitCode)
                               .arg(pullInvocation->stdErr()));
                return;
              } else {
                run(image, args, bindMounts);
              }
              pullInvocation->deleteLater();
            });
  } else {
    run(image, args, bindMounts);
  }
}

Pipeline::ImageFuture* DockerPipelineExecutor::getCopyOfImagePriorTo(
  Operator* op)
{
  // TODO
  return nullptr;
}

void DockerPipelineExecutor::cancel(std::function<void()> canceled)
{
  auto stopInvocation = stop(m_containerId);
  connect(stopInvocation, &docker::DockerStopInvocation::finished,
          stopInvocation,
          [this, canceled](int exitCode, QProcess::ExitStatus exitStatus) {
            Q_UNUSED(exitStatus)
            if (!exitCode) {
              canceled();
            }
          });
}

bool DockerPipelineExecutor::cancel(Operator* op)
{

  // Simply stop the container.
  stop(m_containerId);

  return true;
}

bool DockerPipelineExecutor::isRunning()
{
  return !m_containerId.isEmpty();
}

void DockerPipelineExecutor::error(QProcess::ProcessError error)
{
  auto invocation = qobject_cast<docker::DockerInvocation*>(sender());
  displayError("Execution Error",
               QString("An error occurred executing '%1', '%2'")
                 .arg(invocation->commandLine())
                 .arg(error));
}

void DockerPipelineExecutor::containerError(int containerExitCode)
{
  auto logsInvocation = docker::logs(m_containerId);
  connect(logsInvocation, &docker::DockerRunInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(
    logsInvocation, &docker::DockerRunInvocation::finished, logsInvocation,
    [this, logsInvocation, containerExitCode](int exitCode, QProcess::ExitStatus exitStatus) {
      Q_UNUSED(exitStatus)
      if (exitCode) {
        displayError("Docker Error",
                     QString("Docker logs failed with: %1\n\n%2")
                       .arg(exitCode)
                       .arg(logsInvocation->stdErr()));
        return;
      } else {
        auto logs = logsInvocation->logs();
        displayError(
          "Pipeline Error",
          QString("Docker container exited with non-zero exit code: %1."
                  "\n\nDocker logs below: \n\n %2")
            .arg(containerExitCode)
            .arg(logs));
      }
      logsInvocation->deleteLater();
      PipelineSettings settings;
      if (settings.dockerRemove()) {
        remove(m_containerId);
      }
    });
}

void DockerPipelineExecutor::checkContainerStatus()
{
  auto inspectInvocation = docker::inspect(m_containerId);
  connect(inspectInvocation, &docker::DockerInspectInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(
    inspectInvocation, &docker::DockerInspectInvocation::finished,
    inspectInvocation,
    [this, inspectInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
      Q_UNUSED(exitStatus)
      if (exitCode) {
        displayError("Docker Error",
                     QString("Docker inspect failed with: %1\n\n%2")
                       .arg(exitCode)
                       .arg(inspectInvocation->stdErr()));
        return;
      } else {
        // Check we haven't exited with an error.
        auto status = inspectInvocation->status();
        if (status == "exited") {
          if (inspectInvocation->exitCode()) {
            containerError(inspectInvocation->exitCode());
          }
          // Cancel the status checks we are done.
          m_statusCheckTimer->stop();
        }
      }
      inspectInvocation->deleteLater();
    });
}

void DockerPipelineExecutor::progressReady()
{
  auto progressMessage = m_progressConnection->readLine();
  auto progressDoc = QJsonDocument::fromJson(progressMessage);
  if (!progressDoc.isObject()) {
    qCritical()
      << QString("Invalid progress message '%1'").arg(QString(progressMessage));
    return;
  }
  auto progressObj = progressDoc.object();
  auto type = progressObj["type"].toString();

  // Operator progress
  if (progressObj.contains("operator")) {
    auto opIndex = progressObj["operator"].toInt();
    auto op = pipeline()->dataSource()->operators()[opIndex];

    if (type == "started") {
      operatorStarted(op);
    } else if (type == "finished") {
      operatorFinished(op);
    } else if (type == "error") {
      auto error = progressObj["error"].toString();
      operatorError(op, error);
    } else if (type == "progress.maximum") {
      auto value = progressObj["value"].toInt();
      operatorProgressMaximum(op, value);
    } else if (type == "progress.step") {
      auto value = progressObj["value"].toInt();
      operatorProgressStep(op, value);
    } else if (type == "progress.message") {
      auto value = progressObj["value"].toInt();
      operatorProgressMaximum(op, value);
    } else {
      qCritical() << QString("Unrecognized message type: %1").arg(type);
    }
  }
  // Overrall pipeline progress
  else {
    if (type == "started") {
      pipelineStarted();
    } else if (type == "finished") {
      pipelineFinished();
    } else {
      qCritical() << QString("Unrecognized message type: %1").arg(type);
    }
  }

  // If we have more data schedule ourselve again.
  if (m_progressConnection->bytesAvailable() > 0) {
    QTimer::singleShot(0, this, &DockerPipelineExecutor::progressReady);
  }
}

void DockerPipelineExecutor::operatorStarted(Operator* op)
{
  op->setState(OperatorState::Running);
  QtConcurrent::run(m_threadPool, [op]() { emit op->transformingStarted(); });
}

void DockerPipelineExecutor::operatorFinished(Operator* op)
{
  op->setState(OperatorState::Complete);
  QtConcurrent::run(m_threadPool, [op]() {
    emit op->transformingDone(TransformResult::Complete);
  });
}

void DockerPipelineExecutor::operatorError(Operator* op, const QString& error)
{
  op->setState(OperatorState::Error);
  QtConcurrent::run(m_threadPool, [op]() {
    emit op->transformingDone(TransformResult::Error);
  });
  qCritical() << error;
}

void DockerPipelineExecutor::operatorProgressMaximum(Operator* op, int max)
{
  op->setTotalProgressSteps(max);
}

void DockerPipelineExecutor::operatorProgressStep(Operator* op, int step)
{
  op->setProgressStep(step);
}
void DockerPipelineExecutor::operatorProgressMessage(Operator* op,
                                                     const QString& msg)
{
  op->setProgressMessage(msg);
}

void DockerPipelineExecutor::pipelineStarted()
{
  qDebug("Pipeline started in docker container!");
}

void DockerPipelineExecutor::pipelineFinished()
{
  auto transformedFilePath = m_temporaryDir->filePath(TRANSFORM_FILENAME);
  EmdFormat emdFile;
  vtkNew<vtkImageData> transformedData;
  if (emdFile.read(transformedFilePath.toLatin1().data(), transformedData)) {
    pipeline()->branchFinished(pipeline()->dataSource(), transformedData);
    emit pipeline()->finished();
  } else {
    displayError("Read Error", QString("Unable to load transformed data at: %1")
                                 .arg(transformedFilePath));
  }

  // Cancel status checks
  m_statusCheckTimer->stop();

  // Clean up temp directory
  m_temporaryDir.reset(nullptr);

  PipelineSettings settings;
  if (settings.dockerRemove()) {
    // Remove the container
    remove(m_containerId);
  }

  m_containerId = QString();
}

void DockerPipelineExecutor::displayError(const QString& title,
                                          const QString& msg)
{
  QMessageBox::critical(tomviz::mainWidget(), title, msg);
  qCritical() << msg;
}

} // namespace tomviz
