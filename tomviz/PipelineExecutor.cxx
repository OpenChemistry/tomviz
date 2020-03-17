/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Pipeline.h"

#include "ActiveObjects.h"
#include "DataExchangeFormat.h"
#include "DataSource.h"
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

class ExternalPipelineFuture : public Pipeline::Future
{
public:
  ExternalPipelineFuture(QList<Operator*> operators, QObject* parent = nullptr);
};

ExternalPipelineFuture::ExternalPipelineFuture(QList<Operator*> operators,
                                               QObject* parent)
  : Pipeline::Future(operators, parent)
{
}

const char* ExternalPipelineExecutor::ORIGINAL_FILENAME = "original";
const char* ExternalPipelineExecutor::TRANSFORM_FILENAME = "transformed.emd";
const char* ExternalPipelineExecutor::STATE_FILENAME = "state.tvsm";
const char* ExternalPipelineExecutor::CONTAINER_MOUNT = "/tomviz";
const char* ExternalPipelineExecutor::PROGRESS_PATH = "progress";

Pipeline::Future* ExternalPipelineExecutor::execute(vtkDataObject* data,
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
  fileNames.append(QDir(executorWorkingDir()).filePath(origFileName));
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
  QFile stateFile(QDir(workingDir()).filePath(STATE_FILENAME));
  if (!stateFile.open(QIODevice::WriteOnly)) {
    displayError("Write Error", "Couldn't open state file for write.");
    return Pipeline::emptyFuture();
  }
  stateFile.write(QJsonDocument(state).toJson());
  stateFile.close();

  // Write data to EMD or DataExchange
  auto dataFilePath = QDir(workingDir()).filePath(origFileName);
  if (origFileName.endsWith("emd")) {
    auto imageData = vtkImageData::SafeDownCast(data);
    if (!EmdFormat::write(dataFilePath.toLatin1().data(), imageData)) {
      displayError("Write Error",
                   QString("Unable to write data at: %1").arg(dataFilePath));
      return Pipeline::emptyFuture();
    }
  } else {
    DataExchangeFormat dxfFile;
    if (!dxfFile.write(dataFilePath.toLatin1().data(),
        pipeline()->dataSource())) {
      displayError("Write Error",
                   QString("Unable to write data at: %1").arg(dataFilePath));
      return Pipeline::emptyFuture();
    }
  }

  // Start reading progress updates
  auto progressPath = QDir(workingDir()).filePath(PROGRESS_PATH);

// On Windows and MacOS we have to use files to pass progress updates rather
// than a local socket which we can use on Linux. Looks like docker on MacOS
// may support sharing local sockets as some point, see
// https://github.com/docker/for-mac/issues/483
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
  m_progressMode = "files";
  m_progressReader.reset(new FilesProgressReader(progressPath, operators));
#else
  m_progressMode = "socket";
  m_progressReader.reset(
    new LocalSocketProgressReader(progressPath, operators));
#endif

  auto future = new ExternalPipelineFuture(operators);
  m_progressReader->start();
  connect(m_progressReader.data(), &ProgressReader::operatorStarted, this,
          &ExternalPipelineExecutor::operatorStarted);
  connect(m_progressReader.data(), &ProgressReader::operatorFinished, this,
          &ExternalPipelineExecutor::operatorFinished);
  connect(m_progressReader.data(), &ProgressReader::operatorError, this,
          &ExternalPipelineExecutor::operatorError);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressMaximum,
          this, &ExternalPipelineExecutor::operatorProgressMaximum);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressStep, this,
          &ExternalPipelineExecutor::operatorProgressStep);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressMessage,
          this, &ExternalPipelineExecutor::operatorProgressMessage);
  connect(m_progressReader.data(), &ProgressReader::operatorProgressData, this,
          &ExternalPipelineExecutor::operatorProgressData);
  connect(m_progressReader.data(), &ProgressReader::pipelineStarted, this,
          &ExternalPipelineExecutor::pipelineStarted);
  connect(m_progressReader.data(), &ProgressReader::pipelineFinished, this,
          [this, future]() {
            auto transformedFilePath =
              QDir(workingDir()).filePath(TRANSFORM_FILENAME);
            vtkSmartPointer<vtkDataObject> transformedData =
              vtkImageData::New();
            vtkImageData* transformedImageData =
              vtkImageData::SafeDownCast(transformedData.Get());
            // Make sure we don't ask the user about subsampling
            QVariantMap options = { { "askForSubsample", false } };
            if (EmdFormat::read(transformedFilePath.toLatin1().data(),
                                transformedImageData, options)) {
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
          &ExternalPipelineExecutor::reset);

  // We need to hook up the transformCanceled signal to each of the operators,
  // so we can stop the container if the user cancels any of them.
  for (Operator* op : operators) {
    connect(op, &Operator::transformCanceled, future,
            [this]() { this->cancel(nullptr); });
  }

  return future;
}

ExternalPipelineExecutor::~ExternalPipelineExecutor() = default;

ExternalPipelineExecutor::ExternalPipelineExecutor(Pipeline* pipeline)
  : PipelineExecutor(pipeline)
{
}

void ExternalPipelineExecutor::displayError(const QString& title,
                                            const QString& msg)
{
  QMessageBox::critical(tomviz::mainWidget(), title, msg);
  qCritical() << msg;
}

QString ExternalPipelineExecutor::workingDir()
{
  return m_temporaryDir->path();
}

QStringList ExternalPipelineExecutor::executorArgs(int start)
{
  auto baseDir = QDir(executorWorkingDir());
  auto stateFilePath = baseDir.filePath(STATE_FILENAME);
  auto outputPath = baseDir.filePath(TRANSFORM_FILENAME);
  auto progressPath = baseDir.filePath(PROGRESS_PATH);

  QStringList args;
  args << "-s";
  args << stateFilePath;
  args << "-i";
  args << QString::number(start);
  args << "-o";
  args << outputPath;
  args << "-p";
  args << m_progressMode;
  args << "-u";
  args << progressPath;

  return args;
}

void ExternalPipelineExecutor::pipelineStarted()
{
}

void ExternalPipelineExecutor::operatorStarted(Operator* op)
{
  op->setState(OperatorState::Running);
  emit op->transformingStarted();

  auto pythonOp = qobject_cast<OperatorPython*>(op);
  if (pythonOp != nullptr) {
    pythonOp->createChildDataSource();
  }
}

void ExternalPipelineExecutor::operatorFinished(Operator* op)
{
  QDir temp(m_temporaryDir->path());
  auto operatorIndex = pipeline()->dataSource()->operators().indexOf(op);
  QDir operatorPath(temp.filePath(QString::number(operatorIndex)));
  // See it we have any child data source updates
  if (operatorPath.exists()) {
    QMap<QString, vtkSmartPointer<vtkDataObject>> childOutput;

    // We are looking for EMD files
    foreach (const QFileInfo& fileInfo,
             operatorPath.entryInfoList(QDir::Files)) {

      auto name = fileInfo.baseName();
      vtkNew<vtkImageData> childData;
      // Make sure we don't ask the user about subsampling
      QVariantMap options = { { "askForSubsample", false } };
      if (EmdFormat::read(fileInfo.filePath().toLatin1().data(), childData,
                          options)) {
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

void ExternalPipelineExecutor::operatorError(Operator* op, const QString& error)
{
  op->setState(OperatorState::Error);
  emit op->transformingDone(TransformResult::Error);

  qCritical() << error;
}

void ExternalPipelineExecutor::operatorProgressMaximum(Operator* op, int max)
{
  op->setTotalProgressSteps(max);
}

void ExternalPipelineExecutor::operatorProgressStep(Operator* op, int step)
{
  op->setProgressStep(step);
}

void ExternalPipelineExecutor::operatorProgressMessage(Operator* op,
                                                       const QString& msg)
{
  op->setProgressMessage(msg);
}

void ExternalPipelineExecutor::operatorProgressData(
  Operator* op, vtkSmartPointer<vtkDataObject> data)
{
  auto pythonOperator = qobject_cast<OperatorPython*>(op);
  if (pythonOperator != nullptr) {
    emit pythonOperator->childDataSourceUpdated(data);
  }
}

void ExternalPipelineExecutor::reset()
{
  // Stop the progress reader
  m_progressReader->stop();

  // Clean up temp directory
  m_temporaryDir.reset(nullptr);
}

QString ExternalPipelineExecutor::originalFileName()
{
  QString ext = ".emd";
  auto* dataSource = pipeline()->dataSource();
  if (dataSource->darkData() && dataSource->whiteData()) {
    // Let's write out to data exchange
    ext = ".h5";
  }

  return ORIGINAL_FILENAME + ext;
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
  auto data = vtkSmartPointer<vtkImageData>::New();

  auto hostPath = QFileInfo(m_path).absoluteDir().filePath(path);

  // Make sure we don't ask the user about subsampling
  QVariantMap options = { { "askForSubsample", false } };
  if (!EmdFormat::read(hostPath.toLatin1().data(), data, options)) {
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
