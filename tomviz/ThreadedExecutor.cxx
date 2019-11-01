/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ThreadedExecutor.h"

namespace tomviz {

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

} // namespace tomviz
