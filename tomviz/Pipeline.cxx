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
#include "DataSource.h"
#include "Operator.h"
#include "PipelineWorker.h"

#include <QObject>
#include <QTimer>
#include <vtkTrivialProducer.h>

namespace tomviz {

class Pipeline::PInternals
{
public:
  DataSource* Data;
  PipelineWorker* Worker;
  PipelineWorker::Future* Future;
  bool Paused = false;
};

Pipeline::Pipeline(DataSource* dataSource, QObject* parent)
  : QObject(parent), Internals(new Pipeline::PInternals())
{
  this->Internals->Data = dataSource;
  this->Internals->Worker = new PipelineWorker(this);
  this->Internals->Data->setParent(this);

  this->addDataSource(dataSource);
}

Pipeline::~Pipeline()
{
}

void Pipeline::execute()
{
  emit this->started();
  this->executePipelineBranch(this->Internals->Data);
}

void Pipeline::execute(DataSource* start, bool last)
{
  emit this->started();
  Operator* lastOp = nullptr;

  if (last) {
    lastOp = start->operators().last();
  }
  this->executePipelineBranch(start, lastOp);
}

void Pipeline::execute(DataSource* start)
{
  emit this->started();
  this->executePipelineBranch(start);
}

void Pipeline::executePipelineBranch(DataSource* dataSource, Operator* start)
{
  if (this->Internals->Paused) {
    return;
  }

  auto operators = dataSource->operators();
  if (operators.isEmpty()) {
    return;
  }

  // Cancel any running operators. TODO in the future we should be able to add
  // operators to end of a running pipeline.
  if (this->Internals->Future != nullptr &&
      this->Internals->Future->isRunning()) {
    this->Internals->Future->cancel();
  }

  vtkDataObject* data = nullptr;

  if (start != nullptr) {
    // Use the transform DataSource as the starting point, if we have one.
    auto transformDataSource = this->findTransformedDataSource(dataSource);
    if (transformDataSource != nullptr) {
      data = transformDataSource->copyData();
    }

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

  // Use the original
  if (data == nullptr) {
    data = dataSource->copyData();
  }

  this->Internals->Future = this->Internals->Worker->run(data, operators);
  connect(this->Internals->Future, &PipelineWorker::Future::finished, this,
          &Pipeline::pipelineBranchFinished);
  connect(this->Internals->Future, &PipelineWorker::Future::canceled, this,
          &Pipeline::pipelineBranchCanceled);
}

void Pipeline::pipelineBranchFinished(bool result)
{
  PipelineWorker::Future* future =
    qobject_cast<PipelineWorker::Future*>(sender());
  if (result) {

    auto lastOp = future->operators().last();

    // We only add the transformed child data source if the last operator
    // doesn't already have an explicit child data source i.e.
    // hasChildDataSource
    // is true.
    if (!lastOp->hasChildDataSource()) {
      DataSource* newChildDataSource = nullptr;
      if (lastOp->childDataSource() == nullptr) {
        newChildDataSource = new DataSource("Output");
        newChildDataSource->setParent(this);
        this->addDataSource(newChildDataSource);
        lastOp->setChildDataSource(newChildDataSource);
      }

      lastOp->childDataSource()->setData(future->result());
      lastOp->childDataSource()->dataModified();

      if (newChildDataSource != nullptr) {
        emit lastOp->newChildDataSource(newChildDataSource);
      }
    }

    // Do we have another branch to execute
    if (lastOp->childDataSource() != nullptr) {
      this->execute(lastOp->childDataSource());
    }
    // The pipeline execution is finished
    else {
      emit this->finished();
    }

    future->deleteLater();
    if (this->Internals->Future == future) {
      this->Internals->Future = nullptr;
    }
  } else {
    future->result()->Delete();
  }
}

void Pipeline::pipelineBranchCanceled()
{
  PipelineWorker::Future* future =
    qobject_cast<PipelineWorker::Future*>(sender());
  future->result()->Delete();
  future->deleteLater();
  if (this->Internals->Future == future) {
    this->Internals->Future = nullptr;
  }
}

void Pipeline::pause()
{
  this->Internals->Paused = true;
}

void Pipeline::resume(bool execute)
{
  this->Internals->Paused = false;
  if (execute) {
    this->execute();
  }
}

void Pipeline::cancel(std::function<void()> canceled)
{
  if (this->Internals->Future) {
    if (canceled) {
      connect(this->Internals->Future, &PipelineWorker::Future::canceled,
              canceled);
    }
    this->Internals->Future->cancel();
  }
}

bool Pipeline::isRunning()
{
  return this->Internals->Future != nullptr &&
         this->Internals->Future->isRunning();
}

DataSource* Pipeline::findTransformedDataSource(DataSource* dataSource)
{
  auto op = this->findTransformedDataSourceOperator(dataSource);
  if (op != nullptr) {
    return op->childDataSource();
  }

  return nullptr;
}

Operator* Pipeline::findTransformedDataSourceOperator(DataSource* dataSource)
{
  auto operators = dataSource->operators();
  for (auto itr = operators.rbegin(); itr != operators.rend(); ++itr) {
    auto op = *itr;
    // hasChildDataSource is only set by operators that explicitly produce child
    // data sources
    // such as a reconstruction operator. As part of the pipeline execution we
    // do not
    // set that flag, so we currently use it to tell the difference between
    // "explicit"
    // child data sources and those used to represent the transform data source.
    if (!op->hasChildDataSource() && op->childDataSource() != nullptr) {
      return op;
    }
  }

  return nullptr;
}

void Pipeline::addDataSource(DataSource* dataSource)
{
  connect(dataSource, &DataSource::operatorAdded,
          [this](Operator* op) { this->execute(op->dataSource(), true); });
  // Wire up transformModified to execute pipeline
  connect(dataSource, &DataSource::operatorAdded, [this](Operator* op) {
    // Extract out source and execute all.
    connect(op, &Operator::transformModified, this,
            [this]() { this->execute(); });

    // We need to ensure we move add datasource to the end of the branch
    auto operators = op->dataSource()->operators();
    if (operators.size() > 1) {
      auto transformedDataSourceOp =
        this->findTransformedDataSourceOperator(op->dataSource());
      if (transformedDataSourceOp != nullptr) {
        auto transformedDataSource = transformedDataSourceOp->childDataSource();
        transformedDataSourceOp->setChildDataSource(nullptr);
        op->setChildDataSource(transformedDataSource);
        // Delay emitting signal until next event loop
        QTimer::singleShot(
          0, [=] { emit op->dataSourceMoved(transformedDataSource); });
      }
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
    if (this->Internals->Future != nullptr &&
        this->Internals->Future->isRunning()) {
      // If we can't safely cancel the execution then trigger the rerun of the
      // pipeline.
      if (!this->Internals->Future->cancel(op)) {
        this->execute(op->dataSource());
      }
    } else {
      // Trigger the pipeline to run
      this->execute(op->dataSource());
    }
  });
}

Pipeline::ImageFuture::ImageFuture(Operator* op,
                                   vtkSmartPointer<vtkImageData> imageData,
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
  ImageFuture* imageFuture;
  Q_ASSERT(op->dataSource() != nullptr);

  auto dataSource = op->dataSource();
  auto dataObject = dataSource->copyData();
  vtkSmartPointer<vtkImageData> result(vtkImageData::SafeDownCast(dataObject));
  auto operators = dataSource->operators();
  if (operators.size() > 1) {
    auto index = operators.indexOf(op);
    // Only run operators if we have some to run
    if (index > 0) {
      auto future =
        this->Internals->Worker->run(result, operators.mid(0, index));

      imageFuture = new ImageFuture(op, result, future);

      return imageFuture;
    }
  }

  imageFuture = new ImageFuture(op, result);
  // Delay emitting signal until next event loop
  QTimer::singleShot(0, [=] { emit imageFuture->finished(true); });

  return imageFuture;
}

} // tomviz namespace
