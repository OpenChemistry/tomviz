/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Pipeline.h"

#include "ActiveObjects.h"
#include "DataExchangeFormat.h"
#include "DataSource.h"
#include "DockerUtilities.h"
#include "EmdFormat.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "OperatorPython.h"
#include "Pipeline.h"
#include "PipelineExecutor.h"
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
#include <QTimer>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>

#include <functional>

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>);

namespace tomviz {
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

class PipelineFutureThreadedInternal : public Pipeline::Future
{
public:
  PipelineFutureThreadedInternal(vtkImageData* imageData,
                                 QList<Operator*> operators,
                                 PipelineWorker::Future* future = nullptr,
                                 QObject* parent = nullptr);

private:
  PipelineWorker::Future* m_future;
};

PipelineFutureThreadedInternal::PipelineFutureThreadedInternal(
  vtkImageData* imageData, QList<Operator*> operators,
  PipelineWorker::Future* future, QObject* parent)
  : Pipeline::Future(imageData, operators, parent), m_future(future)
{
  connect(m_future, &PipelineWorker::Future::finished, this,
          &Pipeline::Future::finished);
  connect(m_future, &PipelineWorker::Future::canceled, this,
          &Pipeline::Future::canceled);

  connect(m_future, &PipelineWorker::Future::finished, future,
          [future]() { future->deleteLater(); });

  connect(m_future, &PipelineWorker::Future::canceled, future,
          [future]() { future->deleteLater(); });
}

ThreadPipelineExecutor::ThreadPipelineExecutor(Pipeline* pipeline)
  : PipelineExecutor(pipeline)
{
  m_worker = new PipelineWorker(this);
}

Pipeline::Future* ThreadPipelineExecutor::execute(vtkDataObject* data,
                                                  QList<Operator*> operators,
                                                  int start, int end)
{
  if (end == -1) {
    end = operators.size();
  }
  operators = operators.mid(start, end - start);

  // Cancel any running operators. TODO in the future we should be able to add
  // operators to end of a running pipeline.
  if (!m_future.isNull() && m_future->isRunning()) {
    m_future->cancel();
  }

  auto copy = data->NewInstance();
  copy->DeepCopy(data);

  if (operators.isEmpty()) {
    emit pipeline()->finished();
    auto future = new Pipeline::Future();
    future->setResult(vtkImageData::SafeDownCast(copy));
    copy->FastDelete();
    QTimer::singleShot(0, [future] { emit future->finished(); });
    return future;
  }

  m_future = m_worker->run(copy, operators);
  auto future = new PipelineFutureThreadedInternal(
    vtkImageData::SafeDownCast(copy), operators, m_future.data(), this);
  copy->FastDelete();

  return future;
}

void ThreadPipelineExecutor::cancel(std::function<void()> canceled)
{
  if (!m_future.isNull()) {
    if (canceled) {
      connect(m_future, &PipelineWorker::Future::canceled, canceled);
    }
    m_future->cancel();
  }
}

bool ThreadPipelineExecutor::cancel(Operator* op)
{
  if (!m_future.isNull() && m_future->isRunning()) {
    return m_future->cancel(op);
  }

  return false;
}

bool ThreadPipelineExecutor::isRunning()
{
  return !m_future.isNull() && m_future->isRunning();
}

const char* ORIGINAL_FILENAME = "original";
const char* TRANSFORM_FILENAME = "transformed.emd";
const char* STATE_FILENAME = "state.tvsm";
const char* CONTAINER_MOUNT = "/tomviz";
const char* PROGRESS_PATH = "progress";

class PipelineFutureDockerInternal : public Pipeline::Future
{
public:
  PipelineFutureDockerInternal(QList<Operator*> operators,
                               QObject* parent = nullptr);
};

PipelineFutureDockerInternal::PipelineFutureDockerInternal(
  QList<Operator*> operators, QObject* parent)
  : Pipeline::Future(operators, parent)
{
}

DockerPipelineExecutor::DockerPipelineExecutor(Pipeline* pipeline)
  : PipelineExecutor(pipeline), m_statusCheckTimer(new QTimer(this))
{
  m_statusCheckTimer->setInterval(5000);
  connect(m_statusCheckTimer, &QTimer::timeout, this,
          &DockerPipelineExecutor::checkContainerStatus);
}

DockerPipelineExecutor::~DockerPipelineExecutor() = default;

QString DockerPipelineExecutor::originalFileName()
{
  QString ext = ".emd";
  auto* dataSource = pipeline()->dataSource();
  if (dataSource->darkData() && dataSource->whiteData())
  {
    // Let's write out to data exchange
    ext = ".h5";
  }

  return ORIGINAL_FILENAME + ext;
}

docker::DockerRunInvocation* DockerPipelineExecutor::run(
  const QString& image, const QStringList& args,
  const QMap<QString, QString>& bindMounts)
{
  auto runInvocation = docker::run(image, QString(), args, bindMounts);
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
              return;
            } else {
              m_containerId = runInvocation->containerId();
              // Start to monitor the status of the container
              m_statusCheckTimer->start();
            }
            runInvocation->deleteLater();
          });

  return runInvocation;
}

void DockerPipelineExecutor::remove(const QString& containerId, bool force)
{
  auto removeInvocation = docker::remove(containerId, force);
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
                     QString("Docker stop failed with: %1\n\n%2")
                       .arg(exitCode)
                       .arg(stopInvocation->stdErr()));
        stopInvocation->deleteLater();
        return;
      }

      PipelineSettings settings;
      if (settings.dockerRemove() && !m_containerId.isEmpty()) {
        // Remove the container
        remove(m_containerId, true);
      }

      stopInvocation->deleteLater();
    });

  return stopInvocation;
}

Pipeline::Future* DockerPipelineExecutor::execute(vtkDataObject* data,
                                                  QList<Operator*> operators,
                                                  int start, int end)
{
  if (end == -1) {
    end = operators.size();
  }

  m_temporaryDir.reset(new QTemporaryDir());
  if (!m_temporaryDir->isValid()) {
    displayError("Directory Error", "Unable to create temporary directory.");
    return Pipeline::emptyFuture();
    ;
  }

  QString origFileName = originalFileName();

  // First generate a state file for this pipeline
  QJsonObject state;
  QJsonObject dataSource;
  QJsonObject reader;
  QJsonArray fileNames;
  fileNames.append(QDir(CONTAINER_MOUNT).filePath(origFileName));
  reader["fileNames"] = fileNames;
  dataSource["reader"] = reader;
  QJsonArray pipelineOps;
  foreach (Operator* op, operators) {
    pipelineOps.append(op->serialize());
  }
  dataSource["operators"] = pipelineOps;
  QJsonArray dataSources;
  dataSources.append(dataSource);
  state["dataSources"] = dataSources;

  // Now write the update state file to the temporary directory.
  QFile stateFile(QDir(m_temporaryDir->path()).filePath(STATE_FILENAME));
  if (!stateFile.open(QIODevice::WriteOnly)) {
    displayError("Write Error", "Couldn't open state file for write.");
    return Pipeline::emptyFuture();
  }
  stateFile.write(QJsonDocument(state).toJson());
  stateFile.close();

  // Determine the format from the extension and write out the data
  auto dataFilePath = QDir(m_temporaryDir->path()).filePath(origFileName);
  if (origFileName.endsWith("h5")) {
    DataExchangeFormat format;
    if (!format.write(dataFilePath.toLatin1().data(),
        pipeline()->dataSource())) {
      displayError("Write Error",
                   QString("Unable to write data at: %1").arg(dataFilePath));
      return Pipeline::emptyFuture();
    }
  }
  else if (origFileName.endsWith("emd")) {
    EmdFormat emdFile;
    auto imageData = vtkImageData::SafeDownCast(data);
    if (!emdFile.write(dataFilePath.toLatin1().data(), imageData)) {
      displayError("Write Error",
                   QString("Unable to write data at: %1").arg(dataFilePath));
      return Pipeline::emptyFuture();
    }
  }
  else {
    displayError("Write Error",
                 QString("Format not supported for file: %1").arg(origFileName));
    return Pipeline::emptyFuture();
  }

  // Start reading progress updates
  auto progressPath = QDir(m_temporaryDir->path()).filePath(PROGRESS_PATH);

// On Windows and MacOS we have to use files to pass progress updates rather
// than a local socket which we can use on Linux. Looks like docker on MacOS
// may support sharing local sockets as some point, see
// https://github.com/docker/for-mac/issues/483
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
  QString progressMode("files");
  m_progressReader.reset(new FilesProgressReader(progressPath, operators));
#else
  QString progressMode("socket");
  m_progressReader.reset(
    new LocalSocketProgressReader(progressPath, operators));
#endif

  auto future = new PipelineFutureDockerInternal(operators);
  m_progressReader->start();
  connect(m_progressReader.data(), &ProgressReader::operatorStarted, this,
          &DockerPipelineExecutor::operatorStarted);
  connect(m_progressReader.data(), &ProgressReader::operatorFinished, this,
          &DockerPipelineExecutor::operatorFinished);
  connect(m_progressReader.data(), &ProgressReader::operatorError, this,
          &DockerPipelineExecutor::operatorError);
  // connect(m_progressReader.data(), &ProgressReader::operatorCanceled,
  //    this, &DockerPipelineExecutor::operatorCanceled);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressMaximum,
          this, &DockerPipelineExecutor::operatorProgressMaximum);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressStep, this,
          &DockerPipelineExecutor::operatorProgressStep);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressMessage,
          this, &DockerPipelineExecutor::operatorProgressMessage);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressData, this,
          &DockerPipelineExecutor::operatorProgressData);
  connect(m_progressReader.data(), &ProgressReader::pipelineStarted, this,
          &DockerPipelineExecutor::pipelineStarted);
  connect(m_progressReader.data(), &ProgressReader::pipelineFinished, this,
          [this, future]() {
            auto transformedFilePath =
              QDir(m_temporaryDir->path()).filePath(TRANSFORM_FILENAME);
            EmdFormat transformedEmdFile;
            vtkSmartPointer<vtkDataObject> transformedData =
              vtkImageData::New();
            vtkImageData* transformedImageData =
              vtkImageData::SafeDownCast(transformedData.Get());
            if (transformedEmdFile.read(transformedFilePath.toLatin1().data(),
                                        transformedImageData)) {
              future->setResult(transformedImageData);
            } else {
              displayError("Read Error",
                           QString("Unable to load transformed data at: %1")
                             .arg(transformedFilePath));
            }
            emit future->finished();
            transformedImageData->FastDelete();
          });
  connect(future, &Pipeline::Future::finished, this,
          &DockerPipelineExecutor::reset);

  // We need to hook up the transformCanceled signal to each of the operators,
  // so we can stop the container if the user cancels any of them.
  for (Operator* op : operators) {
    connect(op, &Operator::transformCanceled, future,
            [this]() { this->cancel(nullptr); });
  }

  // We are now ready to run the pipeline
  auto mount = QDir(CONTAINER_MOUNT);
  auto stateFilePath = mount.filePath(STATE_FILENAME);
  auto outputPath = mount.filePath(TRANSFORM_FILENAME);
  QStringList args;
  args << "-s";
  args << stateFilePath;
  args << "-i";
  args << QString::number(start);
  args << "-o";
  args << outputPath;
  args << "-p";
  args << progressMode;
  args << "-u";
  args << mount.filePath(PROGRESS_PATH);
  QMap<QString, QString> bindMounts;
  bindMounts[m_temporaryDir->path()] = CONTAINER_MOUNT;

  PipelineSettings settings;
  QString image = settings.dockerImage();
  auto startContainer = [this, image, args, bindMounts]() {
    auto msg = QString("Starting docker container.");
    auto progress = new ProgressDialog("Docker run", msg, tomviz::mainWidget());
    progress->show();
    auto runInvocation = run(image, args, bindMounts);
    connect(runInvocation, &docker::DockerPullInvocation::finished,
            runInvocation,
            [progress](int exitCode, QProcess::ExitStatus exitStatus) {
              Q_UNUSED(exitCode)
              Q_UNUSED(exitStatus)
              progress->hide();
              progress->deleteLater();
            });
  };

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
            [this, image, args, bindMounts, pullInvocation, progress,
             startContainer](int exitCode, QProcess::ExitStatus exitStatus) {
              Q_UNUSED(exitStatus)
              progress->hide();
              progress->deleteLater();
              if (exitCode) {
                displayError("Docker Error",
                             QString("Docker pull failed with: %1\n\n%2")
                               .arg(exitCode)
                               .arg(pullInvocation->stdErr()));
              } else {
                startContainer();
              }
              pullInvocation->deleteLater();
            });
  } else {
    startContainer();
  }

  return future;
}

void DockerPipelineExecutor::cancel(std::function<void()> canceled)
{
  // Call reset to stop progress updates, status checking and clean
  // update state.
  reset();
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
  Q_UNUSED(op);

  if (m_containerId.isEmpty()) {
    return false;
  }

  // Cancel status checks
  m_statusCheckTimer->stop();

  // Stop the progress reader
  m_progressReader->stop();

  // Simply stop the container.
  stop(m_containerId);

  // Clean update state.
  reset();

  // We can't cancel an individual operator so we return false, so the caller
  // knows
  return false;
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
  connect(logsInvocation, &docker::DockerRunInvocation::finished,
          logsInvocation, [this, logsInvocation, containerExitCode](
                            int exitCode, QProcess::ExitStatus exitStatus) {
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
                        "\n\nSee message logs for Docker logs.")
                  .arg(containerExitCode));
              qCritical() << logs;
            }
            logsInvocation->deleteLater();
            PipelineSettings settings;
            if (settings.dockerRemove() && !m_containerId.isEmpty()) {
              remove(m_containerId);
            }
          });
}

void DockerPipelineExecutor::checkContainerStatus()
{
  auto inspectInvocation = docker::inspect(m_containerId);
  connect(inspectInvocation, &docker::DockerInspectInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(inspectInvocation, &docker::DockerInspectInvocation::finished,
          inspectInvocation, [this, inspectInvocation](
                               int exitCode, QProcess::ExitStatus exitStatus) {
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

void DockerPipelineExecutor::operatorStarted(Operator* op)
{
  op->setState(OperatorState::Running);
  emit op->transformingStarted();

  auto pythonOp = qobject_cast<OperatorPython*>(op);
  if (pythonOp != nullptr) {
    pythonOp->createChildDataSource();
  }
}

void DockerPipelineExecutor::operatorFinished(Operator* op)
{
  QDir temp(m_temporaryDir->path());
  auto operatorIndex = pipeline()->dataSource()->operators().indexOf(op);
  QDir operatorPath(temp.filePath(QString::number(operatorIndex)));
  // See it we have any child data source updates
  if (operatorPath.exists()) {
    QMap<QString, vtkSmartPointer<vtkDataObject>> childOutput;
    foreach (const QFileInfo& fileInfo,
             operatorPath.entryInfoList(QDir::NoDotAndDotDot)) {

      auto name = fileInfo.baseName();
      EmdFormat emdFile;
      vtkNew<vtkImageData> childData;
      if (emdFile.read(fileInfo.filePath().toLatin1().data(), childData)) {
        childOutput[name] = childData;
        emit pipeline()->finished();
      } else {
        displayError(
          "Read Error",
          QString("Unable to load child data at: %1").arg(fileInfo.filePath()));
        break;
      }
    }

    auto pythonOp = qobject_cast<OperatorPython*>(op);
    Q_ASSERT(pythonOp != nullptr);
    pythonOp->updateChildDataSource(childOutput);
  }

  op->setState(OperatorState::Complete);
  emit op->transformingDone(TransformResult::Complete);
}

void DockerPipelineExecutor::operatorError(Operator* op, const QString& error)
{
  op->setState(OperatorState::Error);
  emit op->transformingDone(TransformResult::Error);

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

void DockerPipelineExecutor::operatorProgressData(
  Operator* op, vtkSmartPointer<vtkDataObject> data)
{
  auto pythonOperator = qobject_cast<OperatorPython*>(op);
  if (pythonOperator != nullptr) {
    emit pythonOperator->childDataSourceUpdated(data);
  }
}

void DockerPipelineExecutor::pipelineStarted()
{
  qDebug("Pipeline started in docker container!");
}

void DockerPipelineExecutor::reset()
{
  // Cancel status checks
  m_statusCheckTimer->stop();

  // Stop the progress reader
  m_progressReader->stop();

  // Clean up temp directory
  m_temporaryDir.reset(nullptr);

  PipelineSettings settings;
  if (settings.dockerRemove() && !m_containerId.isEmpty()) {
    // Remove the container
    remove(m_containerId, true);
  }

  m_containerId = QString();
}

void DockerPipelineExecutor::displayError(const QString& title,
                                          const QString& msg)
{
  QMessageBox::critical(tomviz::mainWidget(), title, msg);
  qCritical() << msg;
}

ProgressReader::ProgressReader(const QString& path,
                               const QList<Operator*>& operators)
  : m_path(path), m_operators(operators)
{
  connect(this, &ProgressReader::progressMessage, this,
          &ProgressReader::progressReady);
}

void ProgressReader::progressReady(const QString& progressMessage)
{
  if (progressMessage.isEmpty()) {
    return;
  }

  auto progressDoc = QJsonDocument::fromJson(progressMessage.toLatin1());
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
    auto op = m_operators[opIndex];

    if (type == "started") {
      emit operatorStarted(op);
    } else if (type == "finished") {
      emit operatorFinished(op);
    } else if (type == "error") {
      auto error = progressObj["error"].toString();
      emit operatorError(op, error);
    } else if (type == "progress.maximum") {
      auto value = progressObj["value"].toInt();
      emit operatorProgressMaximum(op, value);
    } else if (type == "progress.step") {
      auto value = progressObj["value"].toInt();
      emit operatorProgressStep(op, value);
    } else if (type == "progress.message") {
      auto value = progressObj["value"].toString();
      emit operatorProgressMessage(op, value);
    } else if (type == "progress.data") {
      auto value = progressObj["value"].toString();
      emit operatorProgressData(op, readProgressData(value));
    } else {
      qCritical() << QString("Unrecognized message type: %1").arg(type);
    }
  }
  // Overall pipeline progress
  else {
    if (type == "started") {
      emit pipelineStarted();
    } else if (type == "finished") {
      emit pipelineFinished();
    } else {
      qCritical() << QString("Unrecognized message type: %1").arg(type);
    }
  }
}

vtkSmartPointer<vtkDataObject> ProgressReader::readProgressData(
  const QString& path)
{
  EmdFormat emdFile;
  auto data = vtkSmartPointer<vtkImageData>::New();

  auto hostPath = QFileInfo(m_path).absoluteDir().filePath(path);

  if (!emdFile.read(hostPath.toLatin1().data(), data)) {
    qCritical() << QString("Unable to load progress data at: %1").arg(path);
  }

  return data;
}

FilesProgressReader::FilesProgressReader(const QString& path,
                                         const QList<Operator*>& operators)
  : ProgressReader(path, operators), m_pathWatcher(new QFileSystemWatcher())
{
  QDir dir(m_path);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  connect(m_pathWatcher.data(), &QFileSystemWatcher::directoryChanged, this,
          &FilesProgressReader::checkForProgressFiles);
}

void FilesProgressReader::start()
{
  m_pathWatcher->addPath(m_path);
}

void FilesProgressReader::stop()
{
  m_pathWatcher->removePath(m_path);
}

void FilesProgressReader::checkForProgressFiles()
{
  QDir progressDir(m_path);
  foreach (const QString fileName,
           progressDir.entryList(QDir::Files, QDir::Name)) {
    auto progressFilePath = progressDir.filePath(fileName);
    QFile progressFile(progressFilePath);
    if (!progressFile.exists()) {
      continue;
    }

    if (!progressFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qCritical() << "Unable to read progress file: " << progressFilePath;
      continue;
    }

    auto msg = progressFile.readLine();
    progressFile.close();
    if (!msg.isEmpty()) {
      emit progressMessage(msg);

      progressFile.remove();
    } else {
      QTimer::singleShot(0, this, &FilesProgressReader::checkForProgressFiles);
    }
  }
}

LocalSocketProgressReader::LocalSocketProgressReader(
  const QString& path, const QList<Operator*>& operators)
  : ProgressReader(path, operators), m_localServer(new QLocalServer())
{

  auto server = m_localServer.data();
  connect(server, &QLocalServer::newConnection, server, [this]() {
    auto connection = m_localServer->nextPendingConnection();
    m_progressConnection.reset(connection);

    if (m_progressConnection) {
      connect(connection, &QIODevice::readyRead, this,
              &LocalSocketProgressReader::readProgress);
      connect(connection, static_cast<void (QLocalSocket::*)(
                            QLocalSocket::LocalSocketError socketError)>(
                            &QLocalSocket::error),
              [this](QLocalSocket::LocalSocketError socketError) {
                if (socketError != QLocalSocket::PeerClosedError) {
                  qCritical()
                    << QString("Socket connection error: %1").arg(socketError);
                }
              });
    }
  });
}

void LocalSocketProgressReader::start()
{
  m_localServer->listen(m_path);
}

void LocalSocketProgressReader::stop()
{
  m_localServer->close();
}

void LocalSocketProgressReader::readProgress()
{
  auto message = m_progressConnection->readLine();

  if (message.isEmpty()) {
    return;
  }

  emit progressMessage(message);

  // If we have more data schedule ourselves again.
  if (m_progressConnection->bytesAvailable() > 0) {
    QTimer::singleShot(0, this, &LocalSocketProgressReader::readProgress);
  }
}

} // namespace tomviz
