/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDockerUtilties_h
#define tomvizDockerUtilties_h

// Collection of utilities for interacting with docker.

#include <QList>
#include <QMap>
#include <QPointer>
#include <QProcess>
#include <QStringList>

namespace tomviz {

namespace docker {

class DockerInvocation : public QObject
{
  Q_OBJECT

public:
  DockerInvocation(){};
  DockerInvocation(const QString& command, const QStringList& args);

  QString commandLine() const;
  DockerInvocation* run();
  QString stdOut() { return m_stdOutReceived; }
  QString stdErr() { return m_stdErrReceived; }

signals:
  void error(QProcess::ProcessError error);
  void finished(int exitCode, QProcess::ExitStatus exitStatus);

  void stdOutReceived(const QString& s);
  void stdErrReceived(const QString& s);

protected:
  void init(const QString& command, const QStringList& args);

private slots:
  void onStdOutReceived();
  void onStdErrReceived();

private:
  QString m_command;
  QStringList m_args;
  QString m_stdOutReceived;
  QString m_stdErrReceived;
  QProcess* m_process;
};

class DockerRunInvocation : public DockerInvocation
{
  Q_OBJECT
public:
  DockerRunInvocation(const QString& image, const QString& entryPoint,
                      const QStringList& containerArgs,
                      const QMap<QString, QString>& bindMounts);

  DockerRunInvocation* run();
  QString containerId();

private:
  QString m_image;
  QString m_entryPoint;
  QStringList m_containerArgs;
  QMap<QString, QString> m_bindMounts;
};

class DockerPullInvocation : public DockerInvocation
{
  Q_OBJECT
public:
  DockerPullInvocation(const QString& image);

  DockerPullInvocation* run();

private:
  QString m_image;
};

class DockerLogsInvocation : public DockerInvocation
{
  Q_OBJECT
public:
  DockerLogsInvocation(const QString& containerId, bool follow = false);

  DockerLogsInvocation* run();
  QString logs();

private:
  QString m_containerId;
  QString m_logs;
};

class DockerStopInvocation : public DockerInvocation
{
  Q_OBJECT
public:
  DockerStopInvocation(const QString& containerId, int timeout);

  DockerStopInvocation* run();

private:
  QString m_containerId;
  int m_timeout;
};

class DockerInspectInvocation : public DockerInvocation
{
  Q_OBJECT
public:
  DockerInspectInvocation(const QString& containerId);

  DockerInspectInvocation* run();
  QString status();
  int exitCode();

private:
  QString m_containerId;
  int m_inspectResult;
  QString m_status;
  int m_exitCode = -1;
};

class DockerRemoveInvocation : public DockerInvocation
{
  Q_OBJECT
public:
  DockerRemoveInvocation(const QString& containerId, bool force = false);

  DockerRemoveInvocation* run();

private:
  QString m_containerId;
};

DockerRunInvocation* run(
  const QString& image, const QString& entryPoint = QString(),
  const QStringList& containerArgs = QStringList(),
  const QMap<QString, QString>& bindMounts = QMap<QString, QString>());

DockerPullInvocation* pull(const QString& image);
DockerStopInvocation* stop(const QString& containerId, int wait = 10);
DockerRemoveInvocation* remove(const QString& containerId, bool force = false);
DockerLogsInvocation* logs(const QString& containerId, bool follow = false);
DockerInspectInvocation* inspect(const QString& containerId);

} // namespace docker
} // namespace tomviz

#endif
