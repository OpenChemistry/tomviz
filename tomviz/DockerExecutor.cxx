/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DockerExecutor.h"

#include "DockerUtilities.h"
#include "ProgressDialog.h"
#include "Utilities.h"

namespace tomviz {

DockerPipelineExecutor::DockerPipelineExecutor(Pipeline* pipeline)
  : ExternalPipelineExecutor(pipeline), m_statusCheckTimer(new QTimer(this))
{
  m_statusCheckTimer->setInterval(5000);
  connect(m_statusCheckTimer, &QTimer::timeout, this,
          &DockerPipelineExecutor::checkContainerStatus);
}

DockerPipelineExecutor::~DockerPipelineExecutor() = default;

docker::DockerRunInvocation* DockerPipelineExecutor::run(
  const QString& image, const QStringList& args,
  const QMap<QString, QString>& bindMounts)
{
  auto runInvocation = docker::run(image, QString(), args, bindMounts);
  connect(runInvocation, &docker::DockerRunInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(runInvocation, &docker::DockerRunInvocation::finished, runInvocation,
          [this, runInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
            Q_UNUSED(exitStatus)
            if (exitCode) {
              displayError("Docker Error",
                           QString("Docker run failed with: %1\n\n%2")
                             .arg(exitCode)
                             .arg(runInvocation->stdErr()));
              return;
            } else {
              m_containerId = runInvocation->containerId();
              // Start to monitor the status of the container
              m_statusCheckTimer->start();
            }
            runInvocation->deleteLater();
          });

  return runInvocation;
}

void DockerPipelineExecutor::remove(const QString& containerId, bool force)
{
  auto removeInvocation = docker::remove(containerId, force);
  connect(removeInvocation, &docker::DockerRunInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(
    removeInvocation, &docker::DockerRunInvocation::finished, removeInvocation,
    [this, removeInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
      Q_UNUSED(exitStatus)
      if (exitCode) {
        displayError("Docker Error",
                     QString("Docker remove failed with: %1\n\n%2")
                       .arg(exitCode)
                       .arg(removeInvocation->stdErr()));
      }
      removeInvocation->deleteLater();
    });
}

docker::DockerStopInvocation* DockerPipelineExecutor::stop(
  const QString& containerId)
{
  auto stopInvocation = docker::stop(containerId, 0);
  connect(stopInvocation, &docker::DockerStopInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(
    stopInvocation, &docker::DockerStopInvocation::finished, stopInvocation,
    [this, stopInvocation](int exitCode, QProcess::ExitStatus exitStatus) {
      Q_UNUSED(exitStatus)
      if (exitCode) {
        displayError("Docker Error",
                     QString("Docker stop failed with: %1\n\n%2")
                       .arg(exitCode)
                       .arg(stopInvocation->stdErr()));
        stopInvocation->deleteLater();
        return;
      }

      PipelineSettings settings;
      if (settings.dockerRemove() && !m_containerId.isEmpty()) {
        // Remove the container
        remove(m_containerId, true);
      }

      stopInvocation->deleteLater();
    });

  return stopInvocation;
}

Pipeline::Future* DockerPipelineExecutor::execute(vtkDataObject* data,
                                                  QList<Operator*> operators,
                                                  int start, int end)
{

  auto future = ExternalPipelineExecutor::execute(data, operators, start, end);

  // We are now ready to run the pipeline
  auto args = executorArgs(start);
  QMap<QString, QString> bindMounts;
  bindMounts[m_temporaryDir->path()] = CONTAINER_MOUNT;

  PipelineSettings settings;
  QString image = settings.dockerImage();
  auto startContainer = [this, image, args, bindMounts]() {
    auto msg = QString("Starting docker container.");
    auto progress = new ProgressDialog("Docker run", msg, tomviz::mainWidget());
    progress->show();
    auto runInvocation = run(image, args, bindMounts);
    connect(runInvocation, &docker::DockerPullInvocation::finished,
            runInvocation,
            [progress](int exitCode, QProcess::ExitStatus exitStatus) {
              Q_UNUSED(exitCode)
              Q_UNUSED(exitStatus)
              progress->hide();
              progress->deleteLater();
            });
  };

  // Pull the latest version of the image, if haven't already
  if (settings.dockerPull() && m_pullImage) {
    auto msg = QString("Pulling docker image: %1").arg(image);
    auto progress =
      new ProgressDialog("Docker Pull", msg, tomviz::mainWidget());
    progress->show();
    m_pullImage = false;
    auto pullInvocation = docker::pull(image);
    connect(pullInvocation, &docker::DockerPullInvocation::error, this,
            &DockerPipelineExecutor::error);
    connect(pullInvocation, &docker::DockerPullInvocation::finished,
            pullInvocation,
            [this, image, args, bindMounts, pullInvocation, progress,
             startContainer](int exitCode, QProcess::ExitStatus exitStatus) {
              Q_UNUSED(exitStatus)
              progress->hide();
              progress->deleteLater();
              if (exitCode) {
                displayError("Docker Error",
                             QString("Docker pull failed with: %1\n\n%2")
                               .arg(exitCode)
                               .arg(pullInvocation->stdErr()));
              } else {
                startContainer();
              }
              pullInvocation->deleteLater();
            });
  } else {
    startContainer();
  }

  return future;
}

void DockerPipelineExecutor::cancel(std::function<void()> canceled)
{
  // Call reset to stop progress updates, status checking and clean
  // update state.
  reset();
  auto stopInvocation = stop(m_containerId);
  connect(stopInvocation, &docker::DockerStopInvocation::finished,
          stopInvocation,
          [this, canceled](int exitCode, QProcess::ExitStatus exitStatus) {
            Q_UNUSED(exitStatus)
            if (!exitCode) {
              canceled();
            }
          });
}

bool DockerPipelineExecutor::cancel(Operator* op)
{
  Q_UNUSED(op);

  if (m_containerId.isEmpty()) {
    return false;
  }

  // Cancel status checks
  m_statusCheckTimer->stop();

  // Stop the progress reader
  m_progressReader->stop();

  // Simply stop the container.
  stop(m_containerId);

  // Clean update state.
  reset();

  // We can't cancel an individual operator so we return false, so the caller
  // knows
  return false;
}

bool DockerPipelineExecutor::isRunning()
{
  return !m_containerId.isEmpty();
}

void DockerPipelineExecutor::error(QProcess::ProcessError error)
{
  auto invocation = qobject_cast<docker::DockerInvocation*>(sender());
  displayError("Execution Error",
               QString("An error occurred executing '%1', '%2'")
                 .arg(invocation->commandLine())
                 .arg(error));
}

void DockerPipelineExecutor::containerError(int containerExitCode)
{
  auto logsInvocation = docker::logs(m_containerId);
  connect(logsInvocation, &docker::DockerRunInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(logsInvocation, &docker::DockerRunInvocation::finished,
          logsInvocation, [this, logsInvocation, containerExitCode](
                            int exitCode, QProcess::ExitStatus exitStatus) {
            Q_UNUSED(exitStatus)
            if (exitCode) {
              displayError("Docker Error",
                           QString("Docker logs failed with: %1\n\n%2")
                             .arg(exitCode)
                             .arg(logsInvocation->stdErr()));
              return;
            } else {
              auto logs = logsInvocation->logs();
              displayError(
                "Pipeline Error",
                QString("Docker container exited with non-zero exit code: %1."
                        "\n\nSee message logs for Docker logs.")
                  .arg(containerExitCode));
              qCritical() << logs;
            }
            logsInvocation->deleteLater();
            PipelineSettings settings;
            if (settings.dockerRemove() && !m_containerId.isEmpty()) {
              remove(m_containerId);
            }
          });
}

void DockerPipelineExecutor::checkContainerStatus()
{
  auto inspectInvocation = docker::inspect(m_containerId);
  connect(inspectInvocation, &docker::DockerInspectInvocation::error, this,
          &DockerPipelineExecutor::error);
  connect(inspectInvocation, &docker::DockerInspectInvocation::finished,
          inspectInvocation, [this, inspectInvocation](
                               int exitCode, QProcess::ExitStatus exitStatus) {
            Q_UNUSED(exitStatus)
            if (exitCode) {
              displayError("Docker Error",
                           QString("Docker inspect failed with: %1\n\n%2")
                             .arg(exitCode)
                             .arg(inspectInvocation->stdErr()));
              return;
            } else {
              // Check we haven't exited with an error.
              auto status = inspectInvocation->status();
              if (status == "exited") {
                if (inspectInvocation->exitCode()) {
                  containerError(inspectInvocation->exitCode());
                }
                // Cancel the status checks we are done.
                m_statusCheckTimer->stop();
              }
            }
            inspectInvocation->deleteLater();
          });
}

void DockerPipelineExecutor::pipelineStarted()
{
  qDebug("Pipeline started in docker container!");
}

void DockerPipelineExecutor::reset()
{
  // Cancel status checks
  m_statusCheckTimer->stop();

  ExternalPipelineExecutor::reset();

  PipelineSettings settings;
  if (settings.dockerRemove() && !m_containerId.isEmpty()) {
    // Remove the container
    remove(m_containerId, true);
  }

  m_containerId = QString();
}

QString DockerPipelineExecutor::executorWorkingDir()
{
  return CONTAINER_MOUNT;
}

} // namespace tomviz
