/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizThreadedExecutor_h
#define tomvizThreadedExecutor_h

#include <QObject>

#include "PipelineExecutor.h"

namespace tomviz {
class DataSource;
class Operator;
class Pipeline;

///
/// The default pipeline executor, providing execution of pipelines in a
/// background thread in order to retain interactivity in the user interface.
///
class ThreadPipelineExecutor : public PipelineExecutor
{
  Q_OBJECT

public:
  ThreadPipelineExecutor(Pipeline* pipeline);
  Pipeline::Future* execute(vtkDataObject* data, QList<Operator*> operators,
                            int start = 0, int end = -1) override;
  void cancel(std::function<void()> canceled) override;
  bool cancel(Operator* op) override;
  bool isRunning() override;

private:
  PipelineWorker* m_worker;
  QPointer<PipelineWorker::Future> m_future;
};

} // namespace tomviz

#endif // tomvizThreadedExecutor_h
