/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ExternalPythonExecutor.h"
#include "DataSource.h"
#include "EmdFormat.h"
#include "Operator.h"
#include "OperatorPython.h"
#include "Pipeline.h"
#include "PipelineWorker.h"

namespace tomviz {

ExternalPythonExecutor::ExternalPythonExecutor(Pipeline* pipeline)
  : ExternalPipelineExecutor(pipeline)
{
}

ExternalPythonExecutor::~ExternalPythonExecutor() = default;

Pipeline::Future* ExternalPythonExecutor::execute(vtkDataObject* data,
                                                  QList<Operator*> operators,
                                                  int start, int end)
{

  auto future = ExternalPipelineExecutor::execute(data, operators, start, end);

  // We are now ready to run the pipeline
  QStringList args = executorArgs(start);

  PipelineSettings settings;
  auto pythonExecutable = settings.externalPythonExecutablePath();

  // Find the tomviz-pipeline executable
  auto pythonExecutableFile = QFileInfo(pythonExecutable);

  if (!pythonExecutableFile.exists()) {
    displayError("External Python Error",
                 QString("The external python executable doesn't exist: %1\n")
                   .arg(pythonExecutable));

    return Pipeline::emptyFuture();
  }

  auto baseDir = pythonExecutableFile.dir();
  auto tomvizPipelineExecutable =
    QFileInfo(baseDir.filePath("tomviz-pipeline"));
  if (!tomvizPipelineExecutable.exists()) {
    displayError(
      "External Python Error",
      QString("Unable to find tomviz-pipeline executable, please ensure "
              "tomviz package has been installed in python environment."
              "Click the Help button for more details on setting up your "
              "Python environment."));

    return Pipeline::emptyFuture();
  }

  m_process.reset(new QProcess(this));

  connect(m_process.data(), &QProcess::errorOccurred, this,
          &ExternalPythonExecutor::error);
  connect(
    m_process.data(),
    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
    [this](int exitCode, QProcess::ExitStatus exitStatus) {

      QString standardOut(this->m_process->readAllStandardOutput());
      QString standardErr(this->m_process->readAllStandardError());

      if (exitStatus == QProcess::CrashExit) {
        displayError("External Python Error",
                     QString("The external python process crash: %1\n\n "
                             "stderr:\n%2 \n\n stdout:\n%3 \n")
                       .arg(commandLine(this->m_process.data()))
                       .arg(standardErr)
                       .arg(standardOut));

      } else if (exitCode != 0) {
        displayError(
          "External Python Error",
          QString("The external python return a non-zero exist code: %1\n\n "
                  "command: %2 \n\n stderr:\n%3 \n\n stdout:\n%4 \n")
            .arg(exitCode)
            .arg(commandLine(this->m_process.data()))
            .arg(standardErr)
            .arg(standardOut));
      }

      qDebug().noquote() << standardErr;
      qDebug().noquote() << standardOut;
    });

  // We have to get the process environment and unset TOMVIZ_APPLICATION and
  // set that as the process environment for the process, otherwise the
  // python package will think its running in the applicaton.
  auto processEnv = QProcessEnvironment::systemEnvironment();
  processEnv.remove("TOMVIZ_APPLICATION");
  m_process->setProcessEnvironment(processEnv);

  m_process->start(tomvizPipelineExecutable.filePath(), args);

  return future;
}

void ExternalPythonExecutor::cancel(std::function<void()> canceled)
{

  reset();

  m_process->kill();

  canceled();
}

bool ExternalPythonExecutor::cancel(Operator* op)
{
  Q_UNUSED(op);

  // Stop the progress reader
  m_progressReader->stop();

  m_process->kill();

  // Clean update state.
  reset();

  // We can't cancel an individual operator so we return false, so the caller
  // knows
  return false;
}

bool ExternalPythonExecutor::isRunning()
{
  return !m_process.isNull() && m_process->state() != QProcess::NotRunning;
}

void ExternalPythonExecutor::error(QProcess::ProcessError error)
{
  auto process = qobject_cast<QProcess*>(sender());
  auto invocation = commandLine(process);

  displayError("Execution Error",
               QString("An error occurred executing '%1', '%2'")
                 .arg(invocation)
                 .arg(error));
}

void ExternalPythonExecutor::pipelineStarted()
{
  qDebug("Pipeline started in external python!");
}

void ExternalPythonExecutor::reset()
{
  ExternalPipelineExecutor::reset();

  m_process->waitForFinished();
  m_process.reset();
}

QString ExternalPythonExecutor::executorWorkingDir()
{
  return workingDir();
}

QString ExternalPythonExecutor::commandLine(QProcess* process)
{
  return QString("%1 %2")
    .arg(process->program())
    .arg(process->arguments().join(" "));
}

} // namespace tomviz
