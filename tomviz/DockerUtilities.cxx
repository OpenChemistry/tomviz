/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DockerUtilities.h"
//#include <QListSocket>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace tomviz {

namespace docker {

DockerInvocation::DockerInvocation(const QString& command,
                                   const QStringList& args)
  : m_command(command), m_args(args)
{
}

QString DockerInvocation::commandLine() const
{
  return QString("docker %1 %2").arg(m_command).arg(m_args.join(" "));
}

DockerInvocation* DockerInvocation::run()
{
  m_process = new QProcess(this);

  // We emit the signals rather than connect signal to signal so we get the
  // right sender.
  connect(m_process, &QProcess::errorOccurred,
          [this](QProcess::ProcessError error) { emit this->error(error); });

  connect(m_process, static_cast<void (QProcess::*)(
                       int exitCode, QProcess::ExitStatus exitStatus)>(
                       &QProcess::finished),
          [this](int exitCode, QProcess::ExitStatus exitStatus) {
            emit this->finished(exitCode, exitStatus);
          });
  m_process->setProgram("docker");
  QStringList dockerArgs;
  dockerArgs.append(m_command);
  dockerArgs.append(m_args);

  m_process->setArguments(dockerArgs);

  // Start in the next event loop so signals can be wired up.
  QTimer::singleShot(0, [this]() { m_process->start(); });

  return this;
}

QString DockerInvocation::stdOut()
{
  if (m_process->state() == QProcess::NotRunning) {
    if (m_stdOut.isNull()) {
      m_stdOut = m_process->readAllStandardOutput();
    }

    return m_stdOut;
  }

  return QString();
}

QString DockerInvocation::stdErr()
{
  if (m_process->state() == QProcess::NotRunning) {
    if (m_stdErr.isNull()) {
      m_stdErr = m_process->readAllStandardError();
    }

    return m_stdErr;
  }

  return QString();
}

void DockerInvocation::init(const QString& command, const QStringList& args)
{
  m_command = command;
  m_args = args;
}

DockerRunInvocation::DockerRunInvocation(
  const QString& image, const QString& entryPoint,
  const QStringList& containerArgs, const QMap<QString, QString>& bindMounts)
  : m_image(image), m_entryPoint(entryPoint), m_containerArgs(containerArgs),
    m_bindMounts(bindMounts)
{

  QStringList args;
  args.append("-d");
  if (!entryPoint.isNull()) {
    args.append("--entrypoint");
    args.append(entryPoint);
  }

  if (!bindMounts.isEmpty()) {
    for (QMap<QString, QString>::const_iterator iter = bindMounts.begin();
         iter != bindMounts.end(); ++iter) {
      args.append("-v");
      args.append(QString("%1:%2").arg(iter.key()).arg(iter.value()));
    }
  }

  args.append(image);
  args.append(containerArgs);

  init("run", args);
}

DockerRunInvocation* DockerRunInvocation::run()
{
  return qobject_cast<DockerRunInvocation*>(DockerInvocation::run());
}

QString DockerRunInvocation::containerId()
{
  return stdOut().trimmed();
}

DockerPullInvocation::DockerPullInvocation(const QString& image)
  : m_image(image)
{
  QStringList args;
  args.append(image);

  init("pull", args);
}

DockerPullInvocation* DockerPullInvocation::run()
{
  return qobject_cast<DockerPullInvocation*>(DockerInvocation::run());
}

DockerLogsInvocation::DockerLogsInvocation(const QString& containerId)
  : m_containerId(containerId)
{
  QStringList args;
  args.append(containerId);

  init("logs", args);
}

DockerLogsInvocation* DockerLogsInvocation::run()
{
  return qobject_cast<DockerLogsInvocation*>(DockerInvocation::run());
}

QString DockerLogsInvocation::logs()
{
  return stdErr() + stdOut();
}

DockerStopInvocation::DockerStopInvocation(const QString& containerId,
                                           int timeout = 10)
  : m_containerId(containerId), m_timeout(timeout)
{
  QStringList args;
  args.append(containerId);
  args.append("-t");
  args.append(QString::number(timeout));

  init("stop", args);
}

DockerStopInvocation* DockerStopInvocation::run()
{
  return qobject_cast<DockerStopInvocation*>(DockerInvocation::run());
}

DockerInspectInvocation::DockerInspectInvocation(const QString& containerId)
  : m_containerId(containerId)
{
  QStringList args;
  args.append(containerId);

  init("inspect", args);
}

QString DockerInspectInvocation::status()
{
  if (m_status.isNull()) {
    auto document = QJsonDocument::fromJson(stdOut().toLatin1());
    auto inspect = document.array()[0].toObject();
    auto state = inspect["State"].toObject();
    auto status = state["Status"];

    if (status.isString()) {
      m_status = status.toString();
    }
  }

  return m_status;
}

int DockerInspectInvocation::exitCode()
{
  if (m_exitCode < 0) {
    auto document = QJsonDocument::fromJson(stdOut().toLatin1());
    auto inspect = document.array()[0].toObject();
    auto state = inspect["State"].toObject();
    auto exitCode = state["ExitCode"];

    if (exitCode.isDouble()) {
      m_exitCode = exitCode.toInt();
    }
  }

  return m_exitCode;
}

DockerInspectInvocation* DockerInspectInvocation::run()
{
  return qobject_cast<DockerInspectInvocation*>(DockerInvocation::run());
}

DockerRemoveInvocation::DockerRemoveInvocation(const QString& containerId,
                                               bool force)
  : m_containerId(containerId)
{
  QStringList args;
  args.append(containerId);
  if (force) {
    args.append("-f");
  }

  init("rm", args);
}

DockerRemoveInvocation* DockerRemoveInvocation::run()
{
  return qobject_cast<DockerRemoveInvocation*>(DockerInvocation::run());
}

DockerRunInvocation* run(const QString& image, const QString& entryPoint,
                         const QStringList& containerArgs,
                         const QMap<QString, QString>& bindMounts)
{
  auto invocation =
    new DockerRunInvocation(image, entryPoint, containerArgs, bindMounts);

  return invocation->run();
}

DockerPullInvocation* pull(const QString& image)
{
  auto invocation = new DockerPullInvocation(image);

  return invocation->run();
}

DockerStopInvocation* stop(const QString& containerId, int wait)
{
  auto invocation = new DockerStopInvocation(containerId, wait);
  return invocation->run();
}

DockerRemoveInvocation* remove(const QString& containerId, bool force)
{
  auto invocation = new DockerRemoveInvocation(containerId, force);
  return invocation->run();
}

DockerLogsInvocation* logs(const QString& containerId)
{
  auto invocation = new DockerLogsInvocation(containerId);
  return invocation->run();
}

DockerInspectInvocation* inspect(const QString& containerId)
{
  auto invocation = new DockerInspectInvocation(containerId);
  return invocation->run();
}
}

} // namespace tomviz
