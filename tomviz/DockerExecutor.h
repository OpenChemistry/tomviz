/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDockerExecutor_h
#define tomvizDockerExecutor_h

#include <QObject>

#include "PipelineExecutor.h"

namespace tomviz {
class DataSource;
class Operator;
class Pipeline;

namespace docker {
class DockerStopInvocation;
class DockerRunInvocation;
}


///
/// Executor that orchestrates the execution of pipelines in Docker containers,
/// providing a pristine container-based pipeline environment.
///
class DockerPipelineExecutor : public ExternalPipelineExecutor
{
  Q_OBJECT

public:
  DockerPipelineExecutor(Pipeline* pipeline);
  ~DockerPipelineExecutor();
  Pipeline::Future* execute(vtkDataObject* data, QList<Operator*> operators,
                            int start = 0, int end = -1) override;
  void cancel(std::function<void()> canceled) override;
  bool cancel(Operator* op) override;
  bool isRunning() override;

protected:
  QString executorWorkingDir() override;
  void pipelineStarted() override;
  void reset() override;

private slots:
  docker::DockerRunInvocation* run(const QString& image,
                                   const QStringList& args,
                                   const QMap<QString, QString>& bindMounts);
  void remove(const QString& containerId, bool force = false);
  docker::DockerStopInvocation* stop(const QString& containerId);
  void containerError(int exitCode);
  void error(QProcess::ProcessError error);

private:
  bool m_pullImage = true;
  QString m_containerId;

  QTimer* m_statusCheckTimer;

  void checkContainerStatus();
  void followLogs();
};

} // namespace tomviz

#endif // tomvizDockerExecutor_h
