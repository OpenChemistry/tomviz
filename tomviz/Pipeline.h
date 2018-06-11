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
#ifndef tomvizPipeline_h
#define tomvizPipeline_h

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

class pqSettings;

namespace tomviz {
class DataSource;
class Operator;
class Pipeline;
class PipelineExecutor;

namespace docker {
class DockerStopInvocation;
class DockerRunInvocation;
}

class Pipeline : public QObject
{
  Q_OBJECT
public:
  enum ExecutionMode
  {
    Threaded,
    Docker
  };
  Q_ENUM(ExecutionMode)

  class Future;

  Pipeline(DataSource* dataSource, QObject* parent = nullptr);
  ~Pipeline() override;

  // Pause the automatic execution of the pipeline
  void pause();

  // Returns true if pipeline is currently paused, false otherwise.
  bool paused() const;

  // Returns true if edit dialogs of operators in the pipeline are open
  bool editingOperators() const { return m_editingOperators > 0; }

  // Resume the automatic execution of the pipeline, will execution the
  // existing pipeline. If execute is true the entire pipeline will be executed.
  void resume(bool execute = true);

  // Resume the automatic execution of the pipeline, will execute the
  // existing pipeline starting at the given data source.
  void resume(DataSource* at);

  // Cancel execution of the pipeline. canceled is a optional callback
  // that will be called when the pipeline has been successfully canceled.
  void cancel(std::function<void()> canceled = nullptr);

  /// Return true if the pipeline is currently being executed.
  bool isRunning();

  /// Add default modules to this pipeline.
  void addDefaultModules(DataSource* dataSource);

  /// The data source a the root of the pipeline.
  DataSource* dataSource() { return m_data; }

  /// Returns that transformed data source associated with a given
  /// data source. If no data source is provided the pipeline's root data source
  /// will be used.
  DataSource* transformedDataSource(DataSource* dataSource = nullptr);

  /// Set the execution mode to use when executing the pipeline.
  void setExecutionMode(ExecutionMode executor);

  ExecutionMode executionMode() { return m_executionMode; };
  PipelineExecutor* executor() { return m_executor.data(); };

public slots:
  /// Execute the entire pipeline, starting at the root data source. Note the
  /// returned Future instance needs to be cleaned up. deleteWhenFinished() can
  /// be called to ensure its cleanup when the pipeline execution is finished.
  Future* execute();
  /// Execute the pipeline, starting at the given data source. Note the
  /// returned Future instance needs to be cleaned up. deleteWhenFinished() can
  /// be called to ensure its cleanup when the pipeline execution is finished.
  Future* execute(DataSource* dataSource);
  /// Execute the pipeline, starting at the operator provided. Note the
  /// returned Future instance needs to be cleaned up. deleteWhenFinished() can
  /// be called to ensure its cleanup when the pipeline execution is finished.
  Future* execute(DataSource* dataSource, Operator* start);
  /// Execute the pipeline, starting at 'start' end just before 'end'. If end is
  /// nullptr the execution will run to the end of the pipeline. Note the
  /// returned Future instance needs to be cleaned up. deleteWhenFinished() can
  /// be called to ensure its cleanup when the pipeline execution is finished.
  Future* execute(DataSource* dataSource, Operator* start, Operator* end);

  /// The user has started/finished editing an operator
  void startedEditingOp(Operator* op);
  void finishedEditingOp(Operator* op);

signals:
  /// This signal is when the execution of the pipeline starts.
  void started();

  /// This signal is fired the execution of the pipeline finishes.
  void finished();

  /// This signal is fired when an operator is added.  The second argument
  /// is the datasource that should be moved to become its output in the
  /// pipeline view (or null if there isn't one).
  void operatorAdded(Operator* op, DataSource* outputDS = nullptr);

private slots:
  void branchFinished();

private:
  DataSource* findTransformedDataSource(DataSource* dataSource);
  Operator* findTransformedDataSourceOperator(DataSource* dataSource);
  void addDataSource(DataSource* dataSource);
  bool beingEdited(DataSource* dataSource) const;
  bool isModified(DataSource* dataSource, Operator** firstModified) const;
  Future* emptyFuture();

  DataSource* m_data;
  bool m_paused = false;
  bool m_operatorsDeleted = false;
  QScopedPointer<PipelineExecutor> m_executor;
  ExecutionMode m_executionMode = Threaded;
  int m_editingOperators = 0;
};

/// Return from getCopyOfImagePriorTo for caller to track async operation.
class Pipeline::Future : public QObject
{
  Q_OBJECT

public:
  friend class ThreadPipelineExecutor;

  Future(vtkImageData* result, QObject* parent = nullptr) : QObject(parent)
  {
    m_imageData = result;
  };
  Future(QObject* parent = nullptr) : QObject(parent){};
  virtual ~Future() override{};

  vtkSmartPointer<vtkImageData> result() { return m_imageData; }
  void setResult(vtkSmartPointer<vtkImageData> result) { m_imageData = result; }
  void setResult(vtkImageData* result) { m_imageData = result; }
  QList<Operator*> operators() { return m_operators; }
  void setOperators(QList<Operator*> operators) { m_operators = operators; }
  void deleteWhenFinished();

signals:
  void finished();
  void canceled();

private:
  vtkSmartPointer<vtkImageData> m_imageData;
  QList<Operator*> m_operators;
};

class PipelineSettings
{
public:
  PipelineSettings();
  Pipeline::ExecutionMode executionMode();
  QString dockerImage();
  bool dockerPull();
  bool dockerRemove();

  void setExecutionMode(Pipeline::ExecutionMode executor);
  void setExecutionMode(const QString& executor);
  void setDockerImage(const QString& image);
  void setDockerPull(bool pull);
  void setDockerRemove(bool remove);

private:
  pqSettings* m_settings;
};

} // namespace tomviz

#endif // tomvizPipeline_h
