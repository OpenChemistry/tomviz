/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineExecutor_h
#define tomvizPipelineExecutor_h

#include <QObject>

#include "PipelineWorker.h"
#include "Pipeline.h"

#include <QFile>
#include <QFileSystemWatcher>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QPointer>
#include <QScopedPointer>
#include <QSettings>
#include <QTemporaryDir>

#include <functional>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace tomviz {
class DataSource;
class Operator;
class Pipeline;

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
  virtual bool cancel(Operator* op);
  virtual bool isRunning() = 0;

protected:
  Pipeline* pipeline();
};

class ProgressReader;

class ExternalPipelineExecutor : public PipelineExecutor
{
  Q_OBJECT

public:
  ExternalPipelineExecutor(Pipeline* pipeline);
  ~ExternalPipelineExecutor();
  Pipeline::Future* execute(vtkDataObject* data, QList<Operator*> operators,
                            int start = 0, int end = -1) override;

  static const char* ORIGINAL_FILENAME;
  static const char* TRANSFORM_FILENAME;
  static const char* STATE_FILENAME;
  static const char* CONTAINER_MOUNT;
  static const char* PROGRESS_PATH;

protected:
  // The working directory that tomviz will write and read data from
  virtual QString workingDir();
  // The working directory that will be passed to the executor
  virtual QString executorWorkingDir() = 0;
  virtual void operatorStarted(Operator* op);
  virtual void operatorFinished(Operator* op);
  virtual void operatorError(Operator* op, const QString& error);
  virtual void operatorProgressMaximum(Operator* op, int max);
  virtual void operatorProgressStep(Operator* op, int step);
  virtual void operatorProgressMessage(Operator* op, const QString& msg);
  virtual void operatorProgressData(Operator* op, vtkSmartPointer<vtkDataObject> data);
  virtual void pipelineStarted();
  virtual void reset();

  QString originalFileName();
  void displayError(const QString& title, const QString& msg);
  QStringList executorArgs(int start);

  QScopedPointer<QTemporaryDir> m_temporaryDir;
  QScopedPointer<ProgressReader> m_progressReader;
  QString m_progressMode;
};

class ProgressReader : public QObject
{
  Q_OBJECT

public:
  ProgressReader(const QString& path, const QList<Operator*>& operators);

  virtual void start() = 0;
  virtual void stop() = 0;
  vtkSmartPointer<vtkDataObject> readProgressData(const QString& path);

signals:
  void progressMessage(const QString& msg);
  void operatorStarted(Operator* op);
  void operatorFinished(Operator* op);
  void operatorError(Operator* op, const QString& error);
  void operatorCanceled(Operator* op);
  void operatorProgressMaximum(Operator* op, int max);
  void operatorProgressStep(Operator* op, int step);
  void operatorProgressMessage(Operator* op, const QString& msg);
  void operatorProgressData(Operator* op, vtkSmartPointer<vtkDataObject> data);
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
