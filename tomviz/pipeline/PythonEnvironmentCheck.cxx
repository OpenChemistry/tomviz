/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonEnvironmentCheck.h"

#include "tomvizConfig.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace tomviz {
namespace pipeline {

namespace {

using Info = PythonEnvironmentInfo;
using Status = PythonEnvironmentInfo::Status;

#if defined(Q_OS_WIN)
const char* kBinDirName = "Scripts";
const char* kCliRelPath = "Scripts/tomviz-pipeline.exe";
#else
const char* kBinDirName = "bin";
const char* kCliRelPath = "bin/tomviz-pipeline";
#endif

bool isExecutableFile(const QString& path)
{
  QFileInfo info(path);
  return info.isFile() && info.isExecutable();
}

/// Does @a root hold a Python interpreter where an environment keeps it?
bool hasInterpreter(const QDir& root)
{
#if defined(Q_OS_WIN)
  // conda: <env>\python.exe; venv: <env>\Scripts\python.exe
  return isExecutableFile(root.filePath(QStringLiteral("python.exe"))) ||
         isExecutableFile(root.filePath(QStringLiteral("Scripts/python.exe")));
#else
  return isExecutableFile(root.filePath(QStringLiteral("bin/python"))) ||
         isExecutableFile(root.filePath(QStringLiteral("bin/python3")));
#endif
}

QString interpreterHint()
{
#if defined(Q_OS_WIN)
  return QStringLiteral("python.exe or Scripts\\python.exe");
#else
  return QStringLiteral("bin/python");
#endif
}

bool looksLikeInterpreter(const QString& fileName)
{
  QString name = fileName.toLower();
#if defined(Q_OS_WIN)
  return name == QLatin1String("python.exe") ||
         name == QLatin1String("pythonw.exe");
#else
  // python, python3, python3.12, ...
  return name == QLatin1String("python") ||
         (name.startsWith(QLatin1String("python")) &&
          !name.contains(QLatin1Char('-')) && name.size() <= 12);
#endif
}

/// Was tomviz-pipeline installed into @a envRoot by conda? A conda
/// package leaves a record in <env>/conda-meta; pip (into a conda env
/// or a venv) leaves none. The distinction matters for the fix hint:
/// pip-upgrading a conda-managed package works once, but conda's own
/// records then disagree and its next update can roll it back.
bool condaManagedPipeline(const QString& envRoot)
{
  QDir meta(QDir(envRoot).filePath(QStringLiteral("conda-meta")));
  return meta.exists() &&
         !meta.entryList({ QStringLiteral("tomviz-pipeline-*.json") },
                         QDir::Files)
            .isEmpty();
}

/// The command that installs, upgrades or downgrades tomviz-pipeline
/// into the compatible range, with the tool that manages it there.
QString installCommand(const QString& envRoot, const QString& spec,
                       bool upgrade)
{
  if (condaManagedPipeline(envRoot)) {
    return QStringLiteral("conda install -c conda-forge \"%1\"").arg(spec);
  }
  if (upgrade) {
    return QStringLiteral("pip install -U \"%1\"").arg(spec);
  }
  return QStringLiteral("pip install \"%1\"").arg(spec);
}

/// "<problem>\n\nTo fix: activate the environment, then run:\n<command>"
QString withFix(const QString& problem, const QString& command)
{
  return problem + QStringLiteral("\n\n") +
         PythonEnvironmentCheck::tr(
           "To fix: activate the environment, then run:\n%1")
           .arg(command);
}

QString lastLine(const QString& text, int maxChars = 240)
{
  QStringList lines = text.trimmed().split(QLatin1Char('\n'),
                                           Qt::SkipEmptyParts);
  if (lines.isEmpty()) {
    return QString();
  }
  QString line = lines.last().trimmed();
  if (line.size() > maxChars) {
    line = line.left(maxChars) + QStringLiteral("...");
  }
  return line;
}

} // namespace

// ---- construction ----------------------------------------------------------

PythonEnvironmentCheck::PythonEnvironmentCheck(QObject* parent)
  : QObject(parent)
{
  qRegisterMetaType<PythonEnvironmentInfo>("tomviz::pipeline::"
                                           "PythonEnvironmentInfo");
}

PythonEnvironmentCheck::~PythonEnvironmentCheck()
{
  abort();
}

// ---- version rule -----------------------------------------------------------

QString PythonEnvironmentCheck::requiredVersion()
{
  return QStringLiteral(TOMVIZ_PIPELINE_MIN_VERSION);
}

QString PythonEnvironmentCheck::requirementSpec(const QString& required)
{
  int major = 0, minor = 0, patch = 0;
  if (!parseVersion(required, major, minor, patch)) {
    return QStringLiteral("tomviz-pipeline>=%1").arg(required);
  }
  Q_UNUSED(minor)
  return QStringLiteral("tomviz-pipeline>=%1,<%2")
    .arg(required)
    .arg(major + 1);
}

bool PythonEnvironmentCheck::parseVersion(const QString& text, int& major,
                                          int& minor, int& patch)
{
  static const QRegularExpression re(
    QStringLiteral("^\\s*v?(\\d+)\\.(\\d+)(?:\\.(\\d+))?"));
  QRegularExpressionMatch m = re.match(text);
  if (!m.hasMatch()) {
    return false;
  }
  major = m.captured(1).toInt();
  minor = m.captured(2).toInt();
  patch = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
  return true;
}

bool PythonEnvironmentCheck::isCompatibleVersion(const QString& found,
                                                 const QString& required)
{
  int reqMajor = 0, reqMinor = 0, reqPatch = 0;
  if (!parseVersion(required, reqMajor, reqMinor, reqPatch)) {
    qWarning() << "PythonEnvironmentCheck: unparsable required version"
               << required << "- skipping the version check.";
    return true;
  }
  int major = 0, minor = 0, patch = 0;
  if (!parseVersion(found, major, minor, patch)) {
    return false;
  }
  // >= required and < the next major. Minor releases of the library
  // keep the CLI contract and degrade gracefully in both directions,
  // so a newer minor in the environment is fine; a new major is the
  // signal that something really may have broken.
  if (major != reqMajor) {
    return false;
  }
  return minor > reqMinor || (minor == reqMinor && patch >= reqPatch);
}

// ---- filesystem lookups -----------------------------------------------------

QString PythonEnvironmentCheck::resolveEnvironmentRoot(const QString& path)
{
  QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }
  QFileInfo info(trimmed);
  QDir candidate;
  if (info.isFile()) {
    // The interpreter itself: <env>/bin/python, <env>\python.exe or
    // <env>\Scripts\python.exe.
    if (!looksLikeInterpreter(info.fileName())) {
      return QString();
    }
    candidate = info.dir();
  } else if (info.isDir()) {
    candidate = QDir(info.absoluteFilePath());
  } else {
    return QString();
  }

  // <env>/bin (or <env>\Scripts) rather than <env>. Tested first: a
  // venv's Scripts\ holds python.exe itself and would otherwise pass
  // as a root of its own.
  if (candidate.dirName() == QLatin1String(kBinDirName)) {
    QDir parent = candidate;
    if (parent.cdUp() && hasInterpreter(parent)) {
      return QDir::cleanPath(parent.absolutePath());
    }
  }
  if (hasInterpreter(candidate)) {
    return QDir::cleanPath(candidate.absolutePath());
  }
  return QString();
}

QString PythonEnvironmentCheck::findCliExecutable(const QString& envRoot)
{
  if (envRoot.isEmpty()) {
    return QString();
  }
  QFileInfo info(QDir(envRoot).filePath(QLatin1String(kCliRelPath)));
  if (!info.exists() || !info.isExecutable()) {
    return QString();
  }
  return info.absoluteFilePath();
}

QProcessEnvironment PythonEnvironmentCheck::childProcessEnvironment()
{
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.remove(QStringLiteral("TOMVIZ_APPLICATION"));
  env.remove(QStringLiteral("PYTHONHOME"));
  env.remove(QStringLiteral("PYTHONPATH"));
  env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("ON"));
  return env;
}

PythonEnvironmentCheck::Info PythonEnvironmentCheck::inspect(
  const QString& path, const QString& required)
{
  Info info;
  QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    info.status = Status::NoPath;
    return info;
  }

  info.envPath = resolveEnvironmentRoot(trimmed);
  if (info.envPath.isEmpty()) {
    info.status = Status::NotAnEnvironment;
    info.message =
      tr("The selected path is not a Python environment: no %1 found "
         "inside it.")
        .arg(interpreterHint());
    return info;
  }

  info.cliPath = findCliExecutable(info.envPath);
  if (info.cliPath.isEmpty()) {
    info.status = Status::CliMissing;
    info.message =
      withFix(tr("tomviz-pipeline is not installed in this environment."),
              installCommand(info.envPath, requirementSpec(required),
                             false));
    return info;
  }

  info.status = Status::Ok;
  return info;
}

// ---- verdict ----------------------------------------------------------------

PythonEnvironmentCheck::Info PythonEnvironmentCheck::classify(
  Info info, const CliRunResult& run, int timeoutMs, const QString& required)
{
  const QString spec = requirementSpec(required);
  const QString install = installCommand(info.envPath, spec, false);
  // The package is there but doesn't work; the child's own error is
  // the best clue, and a plain install fills in missing requirements
  // without disturbing what a conda env manages itself.
  const QString brokenFix =
    tr("Check the error above. If a dependency is missing, activate the "
       "environment, then run:\n%1")
      .arg(install);

  if (!run.started) {
    info.status = Status::CliBroken;
    info.message =
      tr("tomviz-pipeline could not be started in this environment (%1).\n\n"
         "The environment may have been moved or deleted. Recreate it, or "
         "select a different one.")
        .arg(run.startError.trimmed());
    return info;
  }
  if (!run.finishedInTime) {
    info.status = Status::CliBroken;
    info.message = tr("tomviz-pipeline did not respond within %1 s in this "
                      "environment.")
                     .arg(timeoutMs / 1000);
    return info;
  }
  if (!run.normalExit || run.exitCode != 0) {
    // tomviz-pipeline < 3.1 (the in-tree package that shipped with
    // tomviz 3.0) has no --version option at all, so click rejects the
    // flag with exit code 2. That is the common "environment predates
    // this tomviz" case, not a broken install.
    if (run.exitCode == 2 && run.stdErr.contains(QLatin1String("--version")) &&
        run.stdErr.contains(QLatin1String("such option"), Qt::CaseInsensitive)) {
      info.status = Status::VersionTooOld;
      info.message =
        withFix(tr("tomviz-pipeline in this environment predates %1 and "
                   "is too old.")
                  .arg(required),
                installCommand(info.envPath, spec, true));
      return info;
    }
    info.status = Status::CliBroken;
    QString detail = lastLine(run.stdErr);
    if (!detail.isEmpty()) {
      detail.prepend(QStringLiteral(":\n"));
    }
    info.message = tr("tomviz-pipeline is installed in this environment "
                      "but failed to run (exit code %1)%2\n\n%3")
                     .arg(run.exitCode)
                     .arg(detail, brokenFix);
    return info;
  }

  // click prints "tomviz-pipeline, version 3.1.3".
  static const QRegularExpression versionRe(
    QStringLiteral("version\\s+(\\S+)"));
  static const QRegularExpression bareRe(
    QStringLiteral("(\\d+\\.\\d+(?:\\.\\d+)?\\S*)"));
  QRegularExpressionMatch m = versionRe.match(run.stdOut);
  if (!m.hasMatch()) {
    m = bareRe.match(run.stdOut);
  }
  if (!m.hasMatch()) {
    info.status = Status::CliBroken;
    info.message = tr("tomviz-pipeline is installed in this environment "
                      "but reported no version (output: '%1').\n\n%2")
                     .arg(lastLine(run.stdOut), brokenFix);
    return info;
  }
  info.version = m.captured(1);

  int major = 0, minor = 0, patch = 0;
  if (!parseVersion(info.version, major, minor, patch)) {
    info.status = Status::CliBroken;
    info.message = tr("tomviz-pipeline in this environment reported an "
                      "unrecognized version '%1'.\n\n%2")
                     .arg(info.version, brokenFix);
    return info;
  }

  if (isCompatibleVersion(info.version, required)) {
    info.status = Status::Ok;
    info.message = tr("This environment is compatible: "
                      "tomviz-pipeline %1")
                     .arg(info.version);
    return info;
  }

  int reqMajor = 0, reqMinor = 0, reqPatch = 0;
  parseVersion(required, reqMajor, reqMinor, reqPatch);
  bool older = major < reqMajor || (major == reqMajor && minor < reqMinor) ||
               (major == reqMajor && minor == reqMinor && patch < reqPatch);
  if (older) {
    info.status = Status::VersionTooOld;
    info.message =
      withFix(tr("tomviz-pipeline %1 is installed in this environment, but "
                 "is too old.")
                .arg(info.version),
              installCommand(info.envPath, spec, true));
  } else {
    info.status = Status::VersionTooNew;
    info.message =
      withFix(tr("tomviz-pipeline %1 is installed in this environment, but "
                 "is newer than supported.")
                .arg(info.version),
              install);
  }
  return info;
}

// ---- blocking check ---------------------------------------------------------

PythonEnvironmentCheck::Info PythonEnvironmentCheck::check(
  const QString& path, int timeoutMs, const QString& required)
{
  Info info = inspect(path, required);
  if (!info.ok()) {
    return info;
  }

  QProcess process;
  process.setProcessEnvironment(childProcessEnvironment());
  process.start(info.cliPath, { QStringLiteral("--version") });
  CliRunResult run;
  if (!process.waitForStarted(timeoutMs)) {
    run.startError = process.errorString();
    return classify(info, run, timeoutMs, required);
  }
  run.started = true;
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished(5000);
    return classify(info, run, timeoutMs, required);
  }
  run.finishedInTime = true;
  run.normalExit = process.exitStatus() == QProcess::NormalExit;
  run.exitCode = process.exitCode();
  run.stdOut = QString::fromUtf8(process.readAllStandardOutput());
  run.stdErr = QString::fromUtf8(process.readAllStandardError());
  return classify(info, run, timeoutMs, required);
}

// ---- asynchronous check -----------------------------------------------------

void PythonEnvironmentCheck::start(const QString& path, int timeoutMs,
                                   const QString& required)
{
  abort();
  const int generation = ++m_generation;

  Info info = inspect(path, required);
  if (!info.ok()) {
    deliver(info, generation);
    return;
  }

  auto* process = new QProcess(this);
  m_process = process;
  process->setProcessEnvironment(childProcessEnvironment());

  // Shared between the timeout and the finished handler so a kill on
  // timeout is reported as such rather than as a crash.
  auto timedOut = std::make_shared<bool>(false);

  connect(process, &QProcess::errorOccurred, this,
          [this, process, info, generation, timeoutMs, required](
            QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart ||
                generation != m_generation) {
              return;
            }
            CliRunResult run;
            run.startError = process->errorString();
            Info result = classify(info, run, timeoutMs, required);
            m_process.clear();
            process->deleteLater();
            deliver(result, generation);
          });

  connect(process, &QProcess::finished, this,
          [this, process, info, generation, timeoutMs, required, timedOut](
            int exitCode, QProcess::ExitStatus status) {
            if (generation != m_generation) {
              return;
            }
            CliRunResult run;
            run.started = true;
            run.finishedInTime = !*timedOut;
            run.normalExit = status == QProcess::NormalExit;
            run.exitCode = exitCode;
            run.stdOut = QString::fromUtf8(process->readAllStandardOutput());
            run.stdErr = QString::fromUtf8(process->readAllStandardError());
            Info result = classify(info, run, timeoutMs, required);
            m_process.clear();
            process->deleteLater();
            deliver(result, generation);
          });

  QTimer::singleShot(timeoutMs, process, [process, timedOut]() {
    if (process->state() != QProcess::NotRunning) {
      *timedOut = true;
      process->kill();
    }
  });

  process->start(info.cliPath, { QStringLiteral("--version") });
}

void PythonEnvironmentCheck::abort()
{
  ++m_generation;
  if (!m_process) {
    return;
  }
  QProcess* process = m_process;
  m_process.clear();
  process->disconnect();
  if (process->state() != QProcess::NotRunning) {
    // Detach so this object's destruction doesn't tear down a live
    // QProcess (which warns); it cleans itself up once the kill lands.
    process->setParent(nullptr);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    process->kill();
  } else {
    process->deleteLater();
  }
}

bool PythonEnvironmentCheck::isRunning() const
{
  return m_process && m_process->state() != QProcess::NotRunning;
}

void PythonEnvironmentCheck::deliver(const Info& info, int generation)
{
  // Queued so callers always see finished() asynchronously; the
  // generation guard drops results of checks superseded meanwhile.
  QMetaObject::invokeMethod(
    this,
    [this, info, generation]() {
      if (generation == m_generation) {
        emit finished(info);
      }
    },
    Qt::QueuedConnection);
}

} // namespace pipeline
} // namespace tomviz
