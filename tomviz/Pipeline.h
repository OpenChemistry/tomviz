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

#include <QScopedPointer>
#include <functional>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace tomviz {
class DataSource;
class Operator;

class Pipeline : public QObject
{
  Q_OBJECT

public:
  class ImageFuture;

  Pipeline(DataSource* dataSource, QObject* parent = nullptr);
  ~Pipeline();

  // Pause the automatic execution of the pipeline
  void pause();

  // Resume the automatic execution of the pipeline, will execution the
  // existing pipeline. If execute is true the entire pipeline will be executed.
  void resume(bool execute = true);

  // Cancel execution of the pipeline. canceled is a optional callback
  // that will be called when the pipeline has been successfully canceled.
  void cancel(std::function<void()> canceled = nullptr);

  /// Return true if the pipeline is currently being executed.
  bool isRunning();

  ImageFuture* getCopyOfImagePriorTo(Operator* op);

public slots:
  void execute();
  void execute(DataSource* start, bool runLast);
  void execute(DataSource* start);

protected slots:
  void executePipelineBranch(DataSource* dataSource, Operator* start = nullptr);

  /// The pipeline worker is finished with this branch.
  void pipelineBranchFinished(bool result);

  /// The pipeline worker has been canceled
  void pipelineBranchCanceled();

signals:
  /// This signal is when the execution of the pipeline starts.
  void started();
  /// This signal is fired the execution of the pipeline finishes.
  void finished();

private:
  class PInternals;
  const QScopedPointer<PInternals> Internals;

  DataSource* findTransformedDataSource(DataSource* dataSource);
  Operator* findTransformedDataSourceOperator(DataSource* dataSource);
  void addDataSource(DataSource* dataSource);
};

/// Return from getCopyOfImagePriorTo for caller to track async operation.
class Pipeline::ImageFuture : public QObject
{
  Q_OBJECT

public:
  friend class Pipeline;

  vtkSmartPointer<vtkImageData> result() { return m_imageData; }
  Operator* op() { return m_operator; }

signals:
  void finished(bool result);
  void canceled();

private:
  ImageFuture(Operator* op, vtkSmartPointer<vtkImageData> m_imageData,
              PipelineWorker::Future* future = nullptr,
              QObject* parent = nullptr);
  ~ImageFuture() override;
  Operator* m_operator;
  vtkSmartPointer<vtkImageData> m_imageData;
  PipelineWorker::Future* m_future;
};
}

#endif // tomvizPipeline_h
