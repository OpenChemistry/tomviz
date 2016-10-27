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
#include "PipelineWorker.h"
#include "Operator.h"

#include <QObject>
#include <QQueue>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>

#include <vtkDataObject.h>

namespace tomviz {

class PipelineWorker::RunnableOperator : public QObject, public QRunnable
{
  Q_OBJECT

public:
  RunnableOperator(Operator* op, vtkDataObject* input,
                   QObject* parent = nullptr);

  /// Returns the data the operator operates on
  vtkDataObject* data() { return m_data; };
  Operator* op() { return m_operator; };
  void run() override;
  void cancel();

signals:
  void complete(bool result);

private:
  Operator* m_operator;
  vtkDataObject* m_data;
  Q_DISABLE_COPY(RunnableOperator)
};

class PipelineWorker::Run : public QObject
{
  Q_OBJECT

public:
  Run(vtkDataObject* data, QList<Operator*> operators);

  /// Clear all Operators from the queue and attempts to cancel the
  /// running Operator.
  void cancel();
  /// Returns true if the operator was successfully removed from the queue
  /// before it was run, false otherwise.
  bool cancel(Operator* op);
  /// Returns true if we are currently running the operator pipeline, false
  /// otherwise.
  bool isRunning();

  /// If the execution of the pipeline is still in progress then add this
  /// operator to it. Return true is
  bool addOperator(Operator* op);

  /// Returns the data object being used for this run.
  vtkDataObject* data() { return m_data; };

  /// Start the pipeline execution
  Future* start();

public slots:
  void operatorComplete(bool result);

  // Start the next operator in the queue
  void startNextOperator();

signals:
  void finished(bool result);
  void canceled();

private:
  RunnableOperator* m_running = nullptr;
  vtkDataObject* m_data;
  QQueue<RunnableOperator*> m_runnableOperators;
  QList<RunnableOperator*> m_complete;
  bool m_canceled = false;
};

#include "PipelineWorker.moc"

PipelineWorker::RunnableOperator::RunnableOperator(Operator* op,
                                                   vtkDataObject* data,
                                                   QObject* parent)
  : QObject(parent), m_operator(op), m_data(data)
{
  this->setAutoDelete(false);
}

void PipelineWorker::RunnableOperator::run()
{

  bool result = m_operator->transform(this->m_data);
  emit complete(result);
}

void PipelineWorker::RunnableOperator::cancel()
{
  this->m_operator->cancelTransform();
}

PipelineWorker::ConfigureThreadPool::ConfigureThreadPool()
{
  auto threads = QThread::idealThreadCount();
  if (threads < 1) {
    threads = 1;
  } else {
    // Use half the threads we have available.
    threads = threads / 2;
  }
  QThreadPool::globalInstance()->setMaxThreadCount(threads);
}

PipelineWorker::Run::Run(vtkDataObject* data, QList<Operator*> operators)
  : m_data(data)
{
  foreach (auto op, operators) {
    m_runnableOperators.enqueue(new RunnableOperator(op, this->m_data, this));
  }
}

PipelineWorker::Future* PipelineWorker::Run::start()
{
  auto future = new PipelineWorker::Future(this);
  connect(this, SIGNAL(finished(bool)), future, SIGNAL(finished(bool)));
  connect(this, SIGNAL(canceled()), future, SIGNAL(canceled()));

  QTimer::singleShot(0, this, SLOT(startNextOperator()));

  return future;
}

void PipelineWorker::Run::startNextOperator()
{

  if (!this->m_runnableOperators.isEmpty()) {
    this->m_running = this->m_runnableOperators.dequeue();
    connect(this->m_running, SIGNAL(complete(bool)), this,
            SLOT(operatorComplete(bool)));
    QThreadPool::globalInstance()->start(this->m_running);
  }
}

void PipelineWorker::Run::operatorComplete(bool result)
{
  auto runnableOperator = qobject_cast<RunnableOperator*>(this->sender());

  this->m_complete.append(runnableOperator);

  if (!result) {
    emit finished(result);
  } else if (!this->m_runnableOperators.isEmpty()) {
    this->startNextOperator();
  } else {
    if (!this->m_canceled && !runnableOperator->op()->isCanceled()) {
      emit finished(result);
    } else {
      emit canceled();
    }
  }

  runnableOperator->deleteLater();
}

void PipelineWorker::Run::cancel()
{
  // Try to cancel the currently running operator
  if (this->m_running != nullptr) {
    QThreadPool::globalInstance()->cancel(this->m_running);
    this->m_running->cancel();
    this->m_running = nullptr;
  }
  this->m_canceled = true;
  emit canceled();
}

bool PipelineWorker::Run::cancel(Operator* op)
{

  // If the operator is currently running we just have to cancel the execution
  // of the whole pipeline.
  if (this->m_running->op() == op) {
    this->cancel();
    return false;
  }

  foreach (auto runnable, this->m_runnableOperators) {
    if (runnable->op() == op) {
      this->m_runnableOperators.removeAll(runnable);
      return true;
    }
  }

  return false;
}

bool PipelineWorker::Run::isRunning()
{
  return this->m_running != nullptr && !this->m_canceled;
}

bool PipelineWorker::Run::addOperator(Operator* op)
{
  if (!this->isRunning()) {
    return false;
  }

  this->m_runnableOperators.enqueue(
    new RunnableOperator(op, this->m_data, this));

  return true;
}

PipelineWorker::Future* PipelineWorker::run(vtkDataObject* data, Operator* op)
{
  QList<Operator*> ops;
  ops << op;

  return this->run(data, ops);
}

PipelineWorker::Future* PipelineWorker::run(vtkDataObject* data,
                                            QList<Operator*> operators)
{
  // Set all the operators in the queued state
  foreach(Operator *op, operators) {
    op->resetState();
  }

  Run* run = new Run(data, operators);

  return run->start();
}

PipelineWorker::Future::Future(Run* run, QObject* parent)
  : QObject(parent), m_run(run)
{
}

PipelineWorker::Future::~Future()
{
  this->m_run->deleteLater();
}

void PipelineWorker::Future::cancel()
{
  this->m_run->cancel();
}

bool PipelineWorker::Future::cancel(Operator* op)
{
  return this->m_run->cancel(op);
}

bool PipelineWorker::Future::isRunning()
{
  return this->m_run->isRunning();
}

vtkDataObject* PipelineWorker::Future::result()
{
  return this->m_run->data();
}

bool PipelineWorker::Future::addOperator(Operator* op)
{
  return this->m_run->addOperator(op);
}

PipelineWorker::PipelineWorker(QObject* parent) : QObject(parent)
{
}
}
