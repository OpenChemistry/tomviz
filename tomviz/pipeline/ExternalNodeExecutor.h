/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineExternalNodeExecutor_h
#define tomvizPipelineExternalNodeExecutor_h

#include "NodeExecutor.h"
#include "PortData.h"

#include <atomic>

#include <QMap>
#include <QPointer>
#include <QScopedPointer>
#include <QString>

class QProcess;
class QTemporaryDir;

namespace tomviz {
namespace pipeline {

class Node;
class ProgressReader;

/// NodeExecutor that runs the node in a separate Python interpreter via
/// the `tomviz-pipeline` CLI. The transport is a one-node "shim"
/// pipeline written as `.tvh5` (so input port payloads are bundled);
/// the CLI is asked to write its result as another `.tvh5` whose
/// dataRefs we read back into the parent-side node's output ports.
///
/// Per-call subprocess spawn — no persistent worker. Progress
/// (`progress.maximum`/`step`/`message`) is forwarded onto the
/// parent-side Node's existing progress signals, so UI plumbing does
/// not need to know external execution is happening.
class ExternalNodeExecutor : public NodeExecutor
{
  Q_OBJECT

public:
  explicit ExternalNodeExecutor(QObject* parent = nullptr);
  explicit ExternalNodeExecutor(const QString& envPath,
                                QObject* parent = nullptr);
  ~ExternalNodeExecutor() override;

  bool execute(Node* node) override;
  void cancel(Node* node) override;
  void complete(Node* node) override;

  /// Auto-execute poll: spawns `tomviz-pipeline --check-auto-execute`
  /// in the configured env so the node's should_auto_execute hook runs
  /// with the same imports execute() would have. The node's user-state
  /// bag rides a JSON sidecar in both directions. Blocks (bounded by a
  /// kill timeout) — call from a worker thread. Any failure — missing
  /// CLI, an env whose tomviz package predates the check mode, a
  /// timeout — answers false.
  bool shouldAutoExecute(Node* node) override;

  QString type() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QString envPath() const;
  void setEnvPath(const QString& path);

  /// Stable type string written into the node's serialized "executor"
  /// block. Exposed so the NodeExecutorFactory registration can use
  /// the same constant.
  static QString typeString();

private:
  /// Build the shim pipeline (single SourceNode + a clone of @a target
  /// wired to it) and write it to `<dir>/shim.tvh5`. Returns the
  /// absolute path on success, an empty string on failure.
  QString writeShimTvh5(Node* target, const QTemporaryDir& dir,
                        int& targetNodeId) const;

  /// Minimal state file for the auto-execute poll: just a clone of
  /// @a target (no inputs, no payloads — the hook receives neither),
  /// so polling never serializes input volumes to disk. Returns the
  /// absolute path on success, an empty string on failure.
  QString writeCheckStateFile(Node* target, const QTemporaryDir& dir,
                              int& targetNodeId) const;

  /// Read `output_state.tvh5` produced by the subprocess and copy each
  /// output port payload onto the matching port of @a target.
  bool populateOutputs(Node* target, int targetNodeId,
                       const QString& outputPath) const;

  /// Apply a live-preview tvh5 from the subprocess as an intermediate
  /// update on the parent-side @a target. Routes through
  /// Node::setIntermediateOutputs so volume ports keep their existing
  /// vtkImageData identity.
  void handleIntermediate(Node* target, int targetNodeId,
                          const QString& tvh5Path) const;

  /// Decode the per-port payloads from a tvh5 file written by the
  /// subprocess. Shared by populateOutputs (final result) and
  /// handleIntermediate (live preview).
  QMap<QString, PortData> decodeTvh5Outputs(Node* target, int targetNodeId,
                                            const QString& tvh5Path) const;

  /// Locate the `tomviz-pipeline` script in the configured environment
  /// (see PythonEnvironmentCheck::resolveEnvironmentRoot). Returns an
  /// empty string if missing.
  QString findCliExecutable() const;

  /// Forward a control message ("cancel" / "complete") to the
  /// subprocess via the progress channel. Posted across threads to
  /// the reader's owning thread.
  void sendControlSignal(const QString& signal);

  QString m_envPath;
  QPointer<QProcess> m_process;
  QPointer<ProgressReader> m_reader;
  /// Set after populateOutputs runs. The intermediate-data lambda
  /// checks this and drops late-firing progress.data events that
  /// would otherwise overwrite the final result.
  std::atomic<bool> m_outputsFinalized{ false };
};

} // namespace pipeline
} // namespace tomviz

#endif
