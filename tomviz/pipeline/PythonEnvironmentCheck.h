/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonEnvironmentCheck_h
#define tomvizPipelinePythonEnvironmentCheck_h

#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QString>

class QProcess;

namespace tomviz {
namespace pipeline {

/// Outcome of validating a user-selected Python environment for
/// external node execution.
struct PythonEnvironmentInfo
{
  enum class Status
  {
    /// Nothing to check (empty path).
    NoPath,
    /// The path is not a Python environment (no interpreter found).
    NotAnEnvironment,
    /// A Python environment without the tomviz-pipeline CLI.
    CliMissing,
    /// tomviz-pipeline is present but could not be run (moved env,
    /// broken install, timeout) or reported nothing usable.
    CliBroken,
    /// tomviz-pipeline runs, but is older than the app requires.
    VersionTooOld,
    /// tomviz-pipeline runs, but is a newer major version than the
    /// app supports.
    VersionTooNew,
    /// Environment usable for external execution.
    Ok,
  };

  Status status = Status::NoPath;
  /// Environment root the check settled on. Set whenever the path led
  /// to an environment (also for CliMissing / version failures), even
  /// when the user pointed at its bin directory or interpreter.
  QString envPath;
  /// Absolute path of the tomviz-pipeline CLI, when found.
  QString cliPath;
  /// tomviz-pipeline version reported by the environment, when known.
  QString version;
  /// One- or two-sentence diagnosis suitable for showing to the user.
  QString message;

  bool ok() const { return status == Status::Ok; }
};

/// Validates that a directory is a Python environment carrying a
/// compatible tomviz-pipeline, so a bad selection is reported in the
/// editor instead of as an execution failure. Checks, in order:
///
///   1. is the path a Python environment (has an interpreter)?
///   2. is the tomviz-pipeline CLI installed in it?
///   3. does `tomviz-pipeline --version` run, and is the version in
///      [TOMVIZ_PIPELINE_MIN_VERSION, next major)?
///
/// Steps 1–2 are filesystem lookups; step 3 spawns the CLI with the
/// same scrubbed environment ExternalNodeExecutor uses, so what is
/// verified is exactly what execution will run. check() is the
/// blocking form; start()/finished() the asynchronous one for UIs.
class PythonEnvironmentCheck : public QObject
{
  Q_OBJECT

public:
  using Info = PythonEnvironmentInfo;

  /// Default wait for `tomviz-pipeline --version` before giving up.
  static constexpr int kDefaultTimeoutMs = 60 * 1000;

  explicit PythonEnvironmentCheck(QObject* parent = nullptr);
  ~PythonEnvironmentCheck() override;

  /// The tomviz-pipeline version this build depends on
  /// (tomviz/python/tomviz/_pipeline_requirement.py, via CMake).
  static QString requiredVersion();

  /// pip-style specifier for the compatible range of @a required,
  /// e.g. "tomviz-pipeline>=3.1.3,<4".
  static QString requirementSpec(const QString& required = requiredVersion());

  /// Parse the leading "major.minor[.patch]" of a version string
  /// (pre-release / dev / post suffixes are ignored). Returns false if
  /// there is no such prefix.
  static bool parseVersion(const QString& text, int& major, int& minor,
                           int& patch);

  /// True if @a found is >= @a required and < the next major release
  /// of @a required. An unparsable @a found is incompatible; an
  /// unparsable @a required disables the check (returns true).
  static bool isCompatibleVersion(const QString& found,
                                  const QString& required);

  /// Normalize a user-picked path to an environment root: accepts the
  /// root itself, its bin/ (Scripts\ on Windows) directory, or the
  /// python executable inside it. Returns an empty string when no
  /// interpreter can be found.
  static QString resolveEnvironmentRoot(const QString& path);

  /// Absolute path of the tomviz-pipeline CLI in @a envRoot, or empty.
  static QString findCliExecutable(const QString& envRoot);

  /// The environment ExternalNodeExecutor spawns the CLI with: the
  /// app's own Python configuration scrubbed so the child resolves
  /// its own interpreter and packages.
  static QProcessEnvironment childProcessEnvironment();

  /// Blocking check: runs all three steps, waiting up to @a timeoutMs
  /// for the CLI. Safe to call from a worker thread.
  static Info check(const QString& path, int timeoutMs = kDefaultTimeoutMs,
                    const QString& required = requiredVersion());

  /// Asynchronous check: emits finished() on this object's thread
  /// (always asynchronously, even when no subprocess is needed). A
  /// new start() supersedes a pending one, whose result is dropped.
  void start(const QString& path, int timeoutMs = kDefaultTimeoutMs,
             const QString& required = requiredVersion());

  /// Drop a pending check; its finished() will not be emitted.
  void abort();

  bool isRunning() const;

signals:
  void finished(const tomviz::pipeline::PythonEnvironmentInfo& info);

private:
  /// Steps 1–2 only. On success the status is Ok with cliPath set and
  /// version empty — the caller still has to run step 3.
  static Info inspect(const QString& path, const QString& required);

  /// Raw outcome of running `tomviz-pipeline --version`, collected
  /// identically by the blocking and asynchronous paths.
  struct CliRunResult
  {
    bool started = false;
    bool finishedInTime = false;
    bool normalExit = false;
    int exitCode = -1;
    QString stdOut;
    QString stdErr;
    /// QProcess's description of a failure to start, when !started.
    QString startError;
  };

  /// Step 3: turn the outcome of `tomviz-pipeline --version` into the
  /// final verdict.
  static Info classify(Info info, const CliRunResult& run, int timeoutMs,
                       const QString& required);

  void deliver(const Info& info, int generation);

  QPointer<QProcess> m_process;
  int m_generation = 0;
};

} // namespace pipeline
} // namespace tomviz

Q_DECLARE_METATYPE(tomviz::pipeline::PythonEnvironmentInfo)

#endif
