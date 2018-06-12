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
#ifndef tomvizPipelineExecutor_h
#define tomvizPipelineExecutor_h

#include <QObject>

#include "PipelineWorker.h"

#include <QFile>
#include <QFileSystemWatcher>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QScopedPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>

#include <functional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace tomviz {
class DataSource;
class Operator;
class Pipeline;

namespace docker {
class DockerStopInvocation;
class DockerRunInvocation;
}

class PipelineExecutor : public QObject
{
  Q_OBJECT

public:
  PipelineExecutor(Pipeline* pipeline);

  /// Execute list of operators on a give data source. start is the index into
  /// the operator list indicating where the execution should start.
  virtual Pipeline::Future* execute(vtkDataObject* data,
                                    QList<Operator*> operators, int start = 0,
                                    int end = -1) = 0;
  virtual void cancel(std::function<void()> canceled) = 0;
  bool cancel(Operator* op);
  virtual bool isRunning() = 0;

protected:
  Pipeline* pipeline();
};

class ThreadPipelineExecutor : public PipelineExecutor
{
  Q_OBJECT

public:
  ThreadPipelineExecutor(Pipeline* pipeline);
  Pipeline::Future* execute(vtkDataObject* data, QList<Operator*> operators,
                            int start = 0, int end = -1);
  void cancel(std::function<void()> canceled);
  bool cancel(Operator* op);
  bool isRunning();

private:
  PipelineWorker* m_worker;
  QPointer<PipelineWorker::Future> m_future;
};

class ProgressReader;

class DockerPipelineExecutor : public PipelineExecutor
{
  Q_OBJECT

public:
  DockerPipelineExecutor(Pipeline* pipeline);
  ~DockerPipelineExecutor();
  Pipeline::Future* execute(vtkDataObject* data, QList<Operator*> operators,
                            int start = 0, int end = -1);
  void cancel(std::function<void()> canceled);
  bool cancel(Operator* op);
  bool isRunning();

private slots:
  void error(QProcess::ProcessError error);
  docker::DockerRunInvocation* run(const QString& image,
                                   const QStringList& args,
                                   const QMap<QString, QString>& bindMounts);
  void remove(const QString& containerId);
  docker::DockerStopInvocation* stop(const QString& containerId);
  void containerError(int exitCode);

private:
  QScopedPointer<QTemporaryDir> m_temporaryDir;
  bool m_pullImage = true;
  QString m_containerId;
  QScopedPointer<ProgressReader> m_progressReader;

  QTimer* m_statusCheckTimer;

  void checkContainerStatus();
  void operatorStarted(Operator* op);
  void operatorFinished(Operator* op);
  void operatorError(Operator* op, const QString& error);
  void operatorCanceled(Operator* op);
  void operatorProgressMaximum(Operator* op, int max);
  void operatorProgressStep(Operator* op, int step);
  void operatorProgressMessage(Operator* op, const QString& msg);
  void pipelineStarted();
  void pipelineFinished();
  void displayError(const QString& title, const QString& msg);
};

class ProgressReader : public QObject
{
  Q_OBJECT

public:
  ProgressReader(const QString& path, const QList<Operator*>& operators);

  virtual void start() = 0;
  virtual void stop() = 0;

signals:
  void progressMessage(const QString& msg);
  void operatorStarted(Operator* op);
  void operatorFinished(Operator* op);
  void operatorError(Operator* op, const QString& error);
  void operatorCanceled(Operator* op);
  void operatorProgressMaximum(Operator* op, int max);
  void operatorProgressStep(Operator* op, int step);
  void operatorProgressMessage(Operator* op, const QString& msg);
  void pipelineStarted();
  void pipelineFinished();

private slots:
  void progressReady(const QString& msg);

protected:
  QString m_path;
  QList<Operator*> m_operators;
};

class FilesProgressReader : public ProgressReader
{
  Q_OBJECT

public:
  FilesProgressReader(const QString& path, const QList<Operator*>& operators);

  void start();
  void stop();

private:
  QScopedPointer<QFileSystemWatcher> m_pathWatcher;

  void checkForProgressFiles();
};

class LocalSocketProgressReader : public ProgressReader
{
  Q_OBJECT

public:
  LocalSocketProgressReader(const QString& path,
                            const QList<Operator*>& operators);

  void start();
  void stop();

private:
  QScopedPointer<QLocalServer> m_localServer;
  QScopedPointer<QLocalSocket> m_progressConnection;

  void readProgress();
};

} // namespace tomviz

#endif // tomvizPipelineExecutor_h
