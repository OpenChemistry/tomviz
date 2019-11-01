/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizExternalPythonExecutor_h
#define tomvizExternalPythonExecutor_h

#include "PipelineExecutor.h"

#include <QObject>
#include <QProcess>
#include <QScopedPointer>

namespace tomviz {

class DataSource;
class Operator;
class Pipeline;

class ExternalPythonExecutor : public ExternalPipelineExecutor
{
  Q_OBJECT

public:
  ExternalPythonExecutor(Pipeline* pipeline);
  ~ExternalPythonExecutor();
  Pipeline::Future* execute(vtkDataObject* data, QList<Operator*> operators,
                            int start = 0, int end = -1) override;
  void cancel(std::function<void()> canceled) override;
  bool cancel(Operator* op) override;
  bool isRunning() override;

protected:
  QString executorWorkingDir() override;

private slots:
  void error(QProcess::ProcessError error);

private:
  void pipelineStarted() override;
  void reset() override;
  QString commandLine(QProcess* process);

  QScopedPointer<QProcess> m_process;
};

} // namespace tomviz

#endif // tomvizExternalPythonExecutor_h
