/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineWorker_h
#define tomvizPipelineWorker_h

#include <QList>
#include <QObject>
#include <QRunnable>

class vtkDataObject;

namespace tomviz {

class Operator;

/// Responsible for running Operator in a separate thread. Backed by the
/// QThreadPool. Operators are run in sequence, one at a time.
class PipelineWorker : public QObject
{
  Q_OBJECT

public:
  class Future;
  PipelineWorker(QObject* parent = nullptr);
  Future* run(vtkDataObject* data, Operator* op);
  Future* run(vtkDataObject* data, QList<Operator*> ops);

private:
  class RunnableOperator;
  class Run;
  class ConfigureThreadPool
  {
  public:
    ConfigureThreadPool();
  };

  ConfigureThreadPool configure;
};

class PipelineWorker::Future : public QObject
{
  Q_OBJECT
  friend class PipelineWorker::Run;

public:
  /// Clear all Operators from the queue and attempts to cancel the
  /// running Operator.
  void cancel();
  /// Returns true if the operator was successfully removed from the queue
  /// before
  /// it was run, false otherwise.
  bool cancel(Operator* op);
  /// Returns true if we are currently running the operator pipeline, false
  /// otherwise.
  bool isRunning();

  vtkDataObject* result();

  /// If the execution of the pipeline is still in progress then add this
  /// operator to it. Return true is
  bool addOperator(Operator* op);

  QList<Operator*> operators();

  ~Future();

signals:
  void canceled();
  void finished(bool result);
  void progressRangeChanged(int minimum, int maximum);
  void progressTextChanged(const QString& progressText);
  void progressValueChanged(int progressValue);

private:
  Future(Run* run, QObject* parent = nullptr);

  Run* m_run;
};
} // namespace tomviz

#endif
