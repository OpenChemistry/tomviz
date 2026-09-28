/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ExternalNodeExecutor.h"

#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "NodeFactory.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "PortData.h"
#include "PortDataMetadata.h"
#include "PythonEnvironmentCheck.h"
#include "ProgressReader.h"
#include "ThreadUtils.h"
#include "SourceNode.h"

#include "Tvh5Format.h"
#include "data/LabelMapData.h"
#include "data/VolumeData.h"

#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace tomviz {
namespace pipeline {

namespace {

constexpr int kShimSourceId = 1;
constexpr int kShimTargetId = 2;

// Hard timeout for the should_auto_execute subprocess. Polls fire on a
// timer, so unlike execute() a hung check would stall auto-execution
// forever; kill it and answer false instead.
constexpr int kCheckTimeoutMs = 10 * 60 * 1000;

bool useSocketProgress()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
  return false;
#else
  return true;
#endif
}

/// Write @a node's user-state bag as the `--node-state` sidecar the
/// CLI reads: {"nodes": {"<nodeId>": {...}}}.
bool writeNodeStateFile(Node* node, int nodeId, const QString& path)
{
  QJsonObject nodes;
  nodes[QString::number(nodeId)] =
    QJsonObject::fromVariantMap(node->userState());
  QJsonObject root;
  root[QStringLiteral("nodes")] = nodes;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "ExternalNodeExecutor: failed to write node-state file"
               << path;
    return false;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  return true;
}

/// Read the node_state.json the CLI writes back and install the entry
/// for @a nodeId as @a node's user-state bag. Missing file or entry is
/// a no-op (e.g. the run failed before writing it, or the env's tomviz
/// package predates the sidecar).
void applyNodeStateFile(Node* node, int nodeId, const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  QJsonValue entry = doc.object()
                       .value(QStringLiteral("nodes"))
                       .toObject()
                       .value(QString::number(nodeId));
  if (entry.isObject()) {
    node->setUserState(entry.toObject().toVariantMap());
  }
}

/// Read the node_parameters.json the CLI writes when the child's kernel
/// changed parameters through `self.set_parameter` (same
/// {"nodes": {"<nodeId>": {...}}} shape as the state sidecar, written
/// only when something changed) and install the entry for @a nodeId
/// through Node::applyParameterUpdates — quietly, no staleness. Missing
/// file or entry is a no-op: nothing changed, or the env's package
/// predates the file.
void applyNodeParametersFile(Node* node, int nodeId, const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  QJsonValue entry = doc.object()
                       .value(QStringLiteral("nodes"))
                       .toObject()
                       .value(QString::number(nodeId));
  if (entry.isObject() && !entry.toObject().isEmpty()) {
    node->applyParameterUpdates(entry.toObject().toVariantMap());
  }
}

} // namespace

ExternalNodeExecutor::ExternalNodeExecutor(QObject* parent)
  : NodeExecutor(parent)
{
}

ExternalNodeExecutor::ExternalNodeExecutor(const QString& envPath,
                                           QObject* parent)
  : NodeExecutor(parent), m_envPath(envPath)
{
}

ExternalNodeExecutor::~ExternalNodeExecutor() = default;

QString ExternalNodeExecutor::typeString()
{
  return QStringLiteral("external");
}

QString ExternalNodeExecutor::type() const
{
  return typeString();
}

QString ExternalNodeExecutor::envPath() const
{
  return m_envPath;
}

void ExternalNodeExecutor::setEnvPath(const QString& path)
{
  m_envPath = path;
}

QJsonObject ExternalNodeExecutor::serialize() const
{
  QJsonObject json;
  json[QStringLiteral("envPath")] = m_envPath;
  return json;
}

bool ExternalNodeExecutor::deserialize(const QJsonObject& json)
{
  m_envPath = json.value(QStringLiteral("envPath")).toString();
  return true;
}

QString ExternalNodeExecutor::findCliExecutable() const
{
  // Resolved the same way the editor's environment check does, so a
  // path that pointed at <env>/bin or the interpreter still runs.
  return PythonEnvironmentCheck::findCliExecutable(
    PythonEnvironmentCheck::resolveEnvironmentRoot(m_envPath));
}

QString ExternalNodeExecutor::writeShimTvh5(Node* target,
                                            const QTemporaryDir& dir,
                                            int& targetNodeId) const
{
  if (!target) {
    return QString();
  }

  // Synthetic SourceNode mirroring the target's connected inputs +
  // a fresh clone of the target wired to it.
  Pipeline shim;

  auto* shimSource = new SourceNode();
  shim.addNode(shimSource);
  shim.setNodeId(shimSource, kShimSourceId);
  shimSource->setLabel(QStringLiteral("ExternalShimSource"));

  // Override the input's active scalar with the target's previous
  // output's active so apply_to_each_array's merge target preserves
  // the user's selection across re-runs.
  QString preferredActive;
  for (auto* outPort : target->outputPorts()) {
    if (!outPort->hasData() || !isVolumeType(outPort->data().type())) {
      continue;
    }
    try {
      auto prevVol = outPort->data().value<VolumeDataPtr>();
      if (prevVol && prevVol->imageData()) {
        if (auto* scalars =
              prevVol->imageData()->GetPointData()->GetScalars()) {
          if (auto* name = scalars->GetName()) {
            preferredActive = QString::fromUtf8(name);
            break;
          }
        }
      }
    } catch (const std::bad_any_cast&) {
    }
  }

  for (auto* input : target->inputPorts()) {
    if (!input->link() || !input->hasData()) {
      qWarning() << "ExternalNodeExecutor: input port" << input->name()
                 << "has no data; cannot run externally.";
      return QString();
    }
    PortData payload = input->data();
    // Deep copy on override — upstream data must stay untouched.
    if (!preferredActive.isEmpty() && isVolumeType(payload.type())) {
      try {
        auto inputVol = payload.value<VolumeDataPtr>();
        if (inputVol && inputVol->imageData() &&
            inputVol->imageData()->GetPointData()->HasArray(
              preferredActive.toUtf8().constData())) {
          vtkNew<vtkImageData> copy;
          copy->DeepCopy(inputVol->imageData());
          copy->GetPointData()->SetActiveScalars(
            preferredActive.toUtf8().constData());
          auto overriddenVol = makeVolumeData(copy.Get(), payload.type());
          payload = PortData(std::any(overriddenVol), payload.type());
        }
      } catch (const std::bad_any_cast&) {
      }
    }
    shimSource->addOutput(input->name(), payload.type());
    shimSource->setOutputData(input->name(), payload);
  }

  QString typeName = NodeFactory::typeName(target);
  if (typeName.isEmpty()) {
    qWarning() << "ExternalNodeExecutor: target node has no registered type; "
                  "cannot externalize.";
    return QString();
  }
  Node* targetClone = NodeFactory::create(typeName);
  if (!targetClone) {
    qWarning() << "ExternalNodeExecutor: NodeFactory could not create"
               << typeName;
    return QString();
  }
  // Strip the "executor" block so the subprocess doesn't recurse into
  // another ExternalNodeExecutor, and "autoExecute" — timers are the
  // parent app's business, not the child's.
  QJsonObject cloneJson = target->serialize();
  cloneJson.remove(QStringLiteral("executor"));
  cloneJson.remove(QStringLiteral("autoExecute"));
  targetClone->deserialize(cloneJson);
  shim.addNode(targetClone);
  shim.setNodeId(targetClone, kShimTargetId);
  targetNodeId = kShimTargetId;

  for (auto* input : target->inputPorts()) {
    auto* srcPort = shimSource->outputPort(input->name());
    auto* dstPort = targetClone->inputPort(input->name());
    if (!srcPort || !dstPort) {
      qWarning() << "ExternalNodeExecutor: missing port pairing for"
                 << input->name();
      return QString();
    }
    shim.createLink(srcPort, dstPort);
  }

  QString shimPath = QDir(dir.path()).filePath(QStringLiteral("shim.tvh5"));
  if (!Tvh5Format::write(shimPath.toStdString(), &shim)) {
    qWarning() << "ExternalNodeExecutor: failed to write shim tvh5 at"
               << shimPath;
    return QString();
  }
  return shimPath;
}

QString ExternalNodeExecutor::writeCheckStateFile(Node* target,
                                                  const QTemporaryDir& dir,
                                                  int& targetNodeId) const
{
  if (!target) {
    return QString();
  }

  QString typeName = NodeFactory::typeName(target);
  if (typeName.isEmpty()) {
    qWarning() << "ExternalNodeExecutor: target node has no registered "
                  "type; cannot poll should_auto_execute.";
    return QString();
  }
  Node* clone = NodeFactory::create(typeName);
  if (!clone) {
    qWarning() << "ExternalNodeExecutor: NodeFactory could not create"
               << typeName;
    return QString();
  }

  Pipeline check;
  QJsonObject cloneJson = target->serialize();
  cloneJson.remove(QStringLiteral("executor"));
  cloneJson.remove(QStringLiteral("autoExecute"));
  clone->deserialize(cloneJson);
  // Drop pending per-port metadata stashed by deserialize — the check
  // never touches port data, so nothing should look resumable.
  for (auto* port : clone->outputPorts()) {
    port->clearPendingData();
  }
  check.addNode(clone);
  check.setNodeId(clone, kShimTargetId);
  targetNodeId = kShimTargetId;

  QString path = QDir(dir.path()).filePath(QStringLiteral("check.tvh5"));
  if (!Tvh5Format::write(path.toStdString(), &check)) {
    qWarning() << "ExternalNodeExecutor: failed to write check tvh5 at"
               << path;
    return QString();
  }
  return path;
}

QMap<QString, PortData> ExternalNodeExecutor::decodeTvh5Outputs(
  Node* target, int targetNodeId, const QString& tvh5Path) const
{
  QMap<QString, PortData> outputs;
  if (!QFileInfo::exists(tvh5Path)) {
    return outputs;
  }

  // Transient pipeline with just the target's clone, pinned at the
  // same id used in the tvh5 so populatePayloadData matches it.
  Pipeline transient;
  QString typeName = NodeFactory::typeName(target);
  Node* clone = NodeFactory::create(typeName);
  if (!clone) {
    return outputs;
  }
  QJsonObject cloneJson = target->serialize();
  cloneJson.remove(QStringLiteral("executor"));
  clone->deserialize(cloneJson);
  // Drop pending per-port metadata stashed by deserialize — the file
  // payload is the source of truth here, not the target's state
  // snapshot from serialize().
  for (auto* port : clone->outputPorts()) {
    port->clearPendingData();
  }
  transient.addNode(clone);
  transient.setNodeId(clone, targetNodeId);

  QJsonObject state = Tvh5Format::readState(tvh5Path.toStdString());
  if (state.isEmpty()) {
    return outputs;
  }
  QJsonObject pipelineJson =
    state.value(QStringLiteral("pipeline")).toObject();
  Tvh5Format::populatePayloadData(&transient, pipelineJson,
                                  tvh5Path.toStdString());

  for (auto* port : clone->outputPorts()) {
    if (port->hasData()) {
      outputs.insert(port->name(), port->data());
    }
  }
  return outputs;
}

bool ExternalNodeExecutor::populateOutputs(Node* target, int targetNodeId,
                                           const QString& outputPath) const
{
  QMap<QString, PortData> outputs =
    decodeTvh5Outputs(target, targetNodeId, outputPath);
  if (outputs.isEmpty()) {
    qWarning() << "ExternalNodeExecutor: no output ports were populated from"
               << outputPath;
    return false;
  }
  // Must happen before the data reaches the ports: setting port data
  // drives the sinks, so a label map that arrives without its
  // segmentation colormap is rendered through the upstream volume's
  // colormap - miscolored, and with a GPU lookup table VTK has to
  // clamp. TransformNode::execute does the same for internally-executed
  // nodes; keep the two paths in step.
  inheritOutputMetadata(target, target->collectInputs(), outputs);

  // Route through Node::applyOutputs so volume payloads reuse the
  // existing VolumeData instance instead of replacing it. That preserves
  // the color map across re-executions — without this, every external
  // re-run would swap in a fresh VolumeData carrying a default
  // ColorMap and the user's choice would be lost.
  target->applyOutputs(outputs);
  return true;
}

void ExternalNodeExecutor::handleIntermediate(Node* target, int targetNodeId,
                                              const QString& tvh5Path) const
{
  // Same decode path as populateOutputs but routed through
  // setIntermediateOutputs to preserve VolumeOutputPort identity.
  QMap<QString, PortData> updates =
    decodeTvh5Outputs(target, targetNodeId, tvh5Path);
  if (!updates.isEmpty()) {
    // Intermediates are rendered as they arrive, so they need the
    // metadata step first too (see populateOutputs).
    inheritOutputMetadata(target, target->collectInputs(), updates);
    target->setIntermediateOutputs(updates);
  }
}

bool ExternalNodeExecutor::execute(Node* node)
{
  if (!node) {
    return false;
  }

  // Defensive: the shim builder needs the factory warmed up, and
  // pipelines that haven't been saved/loaded yet wouldn't have it.
  NodeFactory::registerBuiltins();

  // Mirror TransformNode::execute's state transitions so the UI sees
  // consistent Running/Failed/Canceled/Idle regardless of executor.
  node->resetExecutionFlags();
  node->resetProgress();
  node->setExecState(NodeExecState::Running);
  m_outputsFinalized.store(false);

  QString cli = findCliExecutable();
  if (cli.isEmpty()) {
    // check() needs no subprocess here: the CLI is missing, so it
    // stops at the filesystem steps and says which one failed.
    QString msg =
      m_envPath.isEmpty()
        ? QStringLiteral("No external Python environment selected. Choose "
                         "one containing tomviz-pipeline in the Execution "
                         "tab.")
        : PythonEnvironmentCheck::check(m_envPath).message;
    qWarning() << "ExternalNodeExecutor:" << msg;
    node->setProgressMessage(msg);
    node->setExecState(NodeExecState::Failed);
    return false;
  }

  QTemporaryDir tmpDir;
  if (!tmpDir.isValid()) {
    qWarning() << "ExternalNodeExecutor: failed to create temp dir.";
    node->setExecState(NodeExecState::Failed);
    return false;
  }

  int targetNodeId = -1;
  QString shimPath = writeShimTvh5(node, tmpDir, targetNodeId);
  if (shimPath.isEmpty()) {
    node->setExecState(NodeExecState::Failed);
    return false;
  }

  QString outDir = QDir(tmpDir.path()).filePath(QStringLiteral("out"));
  QDir().mkpath(outDir);
  QString outputStatePath =
    QDir(outDir).filePath(QStringLiteral("output_state.tvh5"));

  // Unix socket on Linux, polled file dir on macOS/Windows.
  QString progressPath;
  ProgressReader* reader = nullptr;
  if (useSocketProgress()) {
    progressPath = QDir(tmpDir.path()).filePath(QStringLiteral("progress.sock"));
    reader = new LocalSocketProgressReader(progressPath);
  } else {
    progressPath = QDir(tmpDir.path()).filePath(QStringLiteral("progress"));
    QDir().mkpath(progressPath);
    reader = new FilesProgressReader(progressPath);
  }
  QScopedPointer<ProgressReader> readerHolder(reader);
  m_reader = reader;

  // Forward subprocess progress messages onto the target node's
  // progress signals. The id filter skips the shim source's events.
  QObject::connect(reader, &ProgressReader::nodeProgressMaximum, node,
                   [node, targetNodeId](int id, int v) {
                     if (id == targetNodeId) {
                       node->setTotalProgressSteps(v);
                     }
                   });
  QObject::connect(reader, &ProgressReader::nodeProgressStep, node,
                   [node, targetNodeId](int id, int v) {
                     if (id == targetNodeId) {
                       node->setProgressStep(v);
                     }
                   });
  QObject::connect(reader, &ProgressReader::nodeProgressMessage, node,
                   [node, targetNodeId](int id, const QString& msg) {
                     if (id == targetNodeId) {
                       node->setProgressMessage(msg);
                     }
                   });
  QObject::connect(reader, &ProgressReader::nodeError, node,
                   [targetNodeId](int id, const QString& err) {
                     if (id == targetNodeId) {
                       qWarning() << "ExternalNodeExecutor: node" << id
                                  << "reported error:" << err;
                     }
                   });
  // Live preview: subprocess writes a small tvh5 alongside the
  // progress channel and sends its basename here. Resolve relative
  // to the temp working dir and apply via setIntermediateOutputs.
  QString workingDir = tmpDir.path();
  QObject::connect(
    reader, &ProgressReader::nodeProgressData, node,
    [this, node, targetNodeId, workingDir](int id, const QString& filename) {
      if (id != targetNodeId || filename.isEmpty()) {
        return;
      }
      // Drop tail-end intermediates that get delivered after we've
      // installed the final result. Qt's queued-connection events
      // already in the receiver's queue still fire after disconnect().
      if (m_outputsFinalized.load()) {
        return;
      }
      QString tvh5Path = QDir(workingDir).filePath(filename);
      handleIntermediate(node, targetNodeId, tvh5Path);
    });

  reader->start();

  // Created on the calling thread so its socket notifiers are driven
  // by our nested QEventLoop below.
  QProcess process;
  m_process = &process;

  process.setProcessEnvironment(
    PythonEnvironmentCheck::childProcessEnvironment());

  QStringList args;
  args << QStringLiteral("-s") << shimPath
       << QStringLiteral("-o") << outDir
       << QStringLiteral("--output-format") << QStringLiteral("state")
       << QStringLiteral("-p")
       << (useSocketProgress() ? QStringLiteral("socket")
                               : QStringLiteral("files"))
       << QStringLiteral("-u") << progressPath;

  // Round-trip the node's user-state bag through the child. Only when
  // the node actually uses the feature — the flag is unknown to older
  // tomviz packages, and passing it unconditionally would break every
  // existing external env.
  const bool passNodeState =
    node->autoExecuteEnabled() || !node->userState().isEmpty();
  if (passNodeState) {
    QString nodeStateInPath =
      QDir(tmpDir.path()).filePath(QStringLiteral("node_state_in.json"));
    if (writeNodeStateFile(node, kShimTargetId, nodeStateInPath)) {
      args << QStringLiteral("--node-state") << nodeStateInPath;
    }
  }

  QEventLoop loop;
  bool finishedCleanly = false;
  int exitCode = -1;
  QProcess::ExitStatus exitStatus = QProcess::NormalExit;
  QObject::connect(
    &process,
    QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), &loop,
    [&](int code, QProcess::ExitStatus status) {
      exitCode = code;
      exitStatus = status;
      finishedCleanly = true;
      loop.quit();
    });
  QObject::connect(&process, &QProcess::errorOccurred, &loop,
                   [&, node](QProcess::ProcessError err) {
                     // "Crashed" after a cancel-driven kill() is expected.
                     if (!node->isCanceled()) {
                       qWarning()
                         << "ExternalNodeExecutor: QProcess error" << err
                         << process.errorString();
                     }
                     loop.quit();
                   });

  // Forward the child's stdout/stderr to the messages box.
  QObject::connect(&process, &QProcess::readyReadStandardOutput, &process,
                   [&]() {
                     auto out = QString::fromUtf8(process.readAllStandardOutput());
                     if (!out.isEmpty()) {
                       qDebug().noquote() << out.trimmed();
                     }
                   });
  QObject::connect(&process, &QProcess::readyReadStandardError, &process,
                   [&]() {
                     auto err = QString::fromUtf8(process.readAllStandardError());
                     if (!err.isEmpty()) {
                       qDebug().noquote() << err.trimmed();
                     }
                   });

  process.start(cli, args);
  if (!process.waitForStarted(-1)) {
    qWarning() << "ExternalNodeExecutor: failed to start" << cli
               << process.errorString();
    reader->stop();
    m_process.clear();
    m_reader.clear();
    node->setExecState(NodeExecState::Failed);
    return false;
  }

  loop.exec();

  // Drain output between the last readyReadStandard* and finished
  // signals so a fast-failing subprocess's traceback isn't lost.
  auto leftoverOut = QString::fromUtf8(process.readAllStandardOutput());
  if (!leftoverOut.isEmpty()) {
    qDebug().noquote() << leftoverOut.trimmed();
  }
  auto leftoverErr = QString::fromUtf8(process.readAllStandardError());
  if (!leftoverErr.isEmpty()) {
    qDebug().noquote() << leftoverErr.trimmed();
  }

  reader->stop();
  // Stop new intermediates from queuing. Already-queued ones are
  // gated by m_outputsFinalized in the lambda below.
  QObject::disconnect(reader, nullptr, node, nullptr);
  m_process.clear();
  m_reader.clear();

  bool failed = !finishedCleanly || exitStatus != QProcess::NormalExit ||
                exitCode != 0;

  // Pick up state mutations regardless of how the run ended: the CLI
  // writes node_state.json after its runs, so a failed node execution
  // can still have recorded state. Missing file is a no-op.
  if (passNodeState) {
    applyNodeStateFile(node, kShimTargetId,
                       QDir(outDir).filePath(
                         QStringLiteral("node_state.json")));
  }
  applyNodeParametersFile(
    node, kShimTargetId,
    QDir(outDir).filePath(QStringLiteral("node_parameters.json")));

  if (node->isCanceled()) {
    node->setExecState(NodeExecState::Canceled);
    return false;
  }

  if (failed) {
    qWarning() << "ExternalNodeExecutor: subprocess failed (exit=" << exitCode
               << ", status=" << exitStatus << ").";
    // Only a failed run pays for an environment check: a stale or
    // incompatible tomviz-pipeline is a likely cause the exit code
    // alone doesn't name.
    PythonEnvironmentInfo info = PythonEnvironmentCheck::check(m_envPath);
    if (!info.ok()) {
      qWarning() << "ExternalNodeExecutor:" << info.message;
      node->setProgressMessage(info.message);
    }
    node->setExecState(NodeExecState::Failed);
    return false;
  }

  // Apply outputs on the node's thread (port mutation safety, parity
  // with setIntermediateData). The flag is set inside the lambda — on
  // the node's thread, with the worker blocked — so already-queued
  // intermediate lambdas observe it before they run.
  bool populated = false;
  auto applyOutputs = [this, node, targetNodeId, &outputStatePath,
                       &populated]() {
    populated = populateOutputs(node, targetNodeId, outputStatePath);
    m_outputsFinalized.store(true);
  };
  runOnThread(node, applyOutputs);
  if (!populated) {
    node->setExecState(NodeExecState::Failed);
    return false;
  }
  // Mirror TransformNode::execute. Flipping to Current also lets the
  // next markStale() cascade downstream — markStale early-returns
  // if already Stale.
  node->markCurrent();
  node->setExecState(NodeExecState::Idle);
  return true;
}

bool ExternalNodeExecutor::shouldAutoExecute(Node* node)
{
  if (!node) {
    return false;
  }

  NodeFactory::registerBuiltins();

  QString cli = findCliExecutable();
  if (cli.isEmpty()) {
    qWarning() << "ExternalNodeExecutor: cannot poll should_auto_execute —"
               << "tomviz-pipeline was not found in" << m_envPath;
    return false;
  }

  QTemporaryDir tmpDir;
  if (!tmpDir.isValid()) {
    qWarning() << "ExternalNodeExecutor: failed to create temp dir for "
                  "the should_auto_execute poll.";
    return false;
  }

  int targetNodeId = -1;
  QString statePath = writeCheckStateFile(node, tmpDir, targetNodeId);
  if (statePath.isEmpty()) {
    return false;
  }

  QString nodeStateInPath =
    QDir(tmpDir.path()).filePath(QStringLiteral("node_state_in.json"));
  if (!writeNodeStateFile(node, targetNodeId, nodeStateInPath)) {
    return false;
  }

  QString outDir = QDir(tmpDir.path()).filePath(QStringLiteral("out"));
  QDir().mkpath(outDir);

  // Deliberately local (not m_process/m_reader): a check may not
  // interfere with the members an in-flight execute() is using.
  QProcess process;
  process.setProcessEnvironment(
    PythonEnvironmentCheck::childProcessEnvironment());

  QStringList args;
  args << QStringLiteral("-s") << statePath
       << QStringLiteral("-o") << outDir
       << QStringLiteral("--check-auto-execute")
       << QString::number(targetNodeId)
       << QStringLiteral("--node-state") << nodeStateInPath;

  process.start(cli, args);
  if (!process.waitForStarted(30000)) {
    qWarning() << "ExternalNodeExecutor: failed to start" << cli
               << process.errorString();
    return false;
  }
  if (!process.waitForFinished(kCheckTimeoutMs)) {
    qWarning() << "ExternalNodeExecutor: should_auto_execute poll timed "
                  "out; killing the subprocess.";
    process.kill();
    process.waitForFinished(5000);
    return false;
  }

  auto childErr = QString::fromUtf8(process.readAllStandardError());
  if (!childErr.isEmpty()) {
    qDebug().noquote() << childErr.trimmed();
  }

  // State mutations made by the hook count even when it answered "no";
  // so do parameter write-backs.
  applyNodeStateFile(node, targetNodeId,
                     QDir(outDir).filePath(
                       QStringLiteral("node_state.json")));
  applyNodeParametersFile(
    node, targetNodeId,
    QDir(outDir).filePath(QStringLiteral("node_parameters.json")));

  if (process.exitStatus() != QProcess::NormalExit ||
      process.exitCode() != 0) {
    qWarning() << "ExternalNodeExecutor: should_auto_execute poll failed "
                  "(exit code"
               << process.exitCode()
               << "). If the environment's tomviz package predates "
                  "--check-auto-execute, update it.";
    return false;
  }

  QFile resultFile(
    QDir(outDir).filePath(QStringLiteral("auto_execute.json")));
  if (!resultFile.open(QIODevice::ReadOnly)) {
    qWarning() << "ExternalNodeExecutor: should_auto_execute poll wrote "
                  "no auto_execute.json.";
    return false;
  }
  QJsonDocument doc = QJsonDocument::fromJson(resultFile.readAll());
  return doc.object().value(QStringLiteral("shouldExecute")).toBool(false);
}

void ExternalNodeExecutor::cancel(Node* /*node*/)
{
  sendControlSignal(QStringLiteral("cancel"));
}

void ExternalNodeExecutor::complete(Node* /*node*/)
{
  sendControlSignal(QStringLiteral("complete"));
}

void ExternalNodeExecutor::sendControlSignal(const QString& signal)
{
  // Soft signal — operator polls and observes at next checkpoint.
  // No kill (matches in-process semantics). Posted to the reader's
  // own thread because socket/file ops belong there.
  auto* r = m_reader.data();
  if (!r) {
    return;
  }
  QMetaObject::invokeMethod(
    r, [r, signal]() { r->sendSignal(signal); }, Qt::QueuedConnection);
}

} // namespace pipeline
} // namespace tomviz
