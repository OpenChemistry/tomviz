/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSaveDataDialog_h
#define tomvizSaveDataDialog_h

#include "PortDataWriter.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Ui {
class SaveDataDialog;
}

namespace tomviz {

namespace pipeline {
class Node;
class OutputPort;
class Pipeline;
} // namespace pipeline

/// Batch export of pipeline output-port payloads into a directory, one
/// file per scalar array. The user picks the destination, one file
/// format per port-type group, and which ports are in scope, then
/// reviews the resolved filenames before anything is written.
class SaveDataDialog : public QDialog
{
  Q_OBJECT

public:
  /// Which output ports the dialog offers.
  enum class Scope
  {
    /// Ports of nodes whose only downstream nodes are sinks.
    LeafNodes,
    /// Every port currently holding data, in memory or on disk. Transient
    /// ports count while their data is still in memory: persistence
    /// decides how long data is kept, not whether it may be saved.
    AllPorts
  };

  /// One file to be written.
  struct Entry
  {
    pipeline::OutputPort* port = nullptr;
    /// Scalar arrays to write into this file. A payload that isn't
    /// array-bearing carries a single empty name.
    QStringList arrayNames;
    /// Absolute path, already disambiguated against the other entries.
    QString path;
  };

  /// Everything one port contributes, shown as a single row.
  struct PortPlan
  {
    pipeline::OutputPort* port = nullptr;
    QList<Entry> entries;
    /// The filename, or — when the port produces several files — the
    /// shared name with the varying part braced:
    /// `node_port_{arrayA|arrayB}.ext`.
    QString displayName;
  };

  /// Export from anywhere in @a pipeline; the user picks the scope.
  explicit SaveDataDialog(pipeline::Pipeline* pipeline,
                          QWidget* parent = nullptr);

  /// Export just @a node's own ports. A single node has no useful
  /// notion of leaves, so the scope is pinned to AllPorts and the
  /// choice is disabled.
  explicit SaveDataDialog(pipeline::Node* node, QWidget* parent = nullptr);

  /// Export a single output port, with the scope pinned as above.
  explicit SaveDataDialog(pipeline::OutputPort* port,
                          QWidget* parent = nullptr);

  ~SaveDataDialog() override;

  /// The entries the user left checked, in display order.
  QList<Entry> selectedEntries() const;

  /// Write @a entries, prompting once about any files that already
  /// exist and reporting failures at the end. Returns the number of
  /// files successfully written.
  static int writeEntries(const QList<Entry>& entries, QWidget* parent);

  /// Output ports in @a scope that tomviz has a writer for. Shared with
  /// SaveDataReaction, which uses it to decide whether the action should
  /// be enabled at all.
  static QList<pipeline::OutputPort*> candidatePorts(
    pipeline::Pipeline* pipeline, Scope scope);

  /// The same filtering restricted to one node's own output ports.
  static QList<pipeline::OutputPort*> candidatePorts(pipeline::Node* node,
                                                     Scope scope);

  /// The same filtering narrowed to a single port. Returns an empty
  /// list when the port doesn't qualify: nothing to write, or no writer
  /// for its type.
  static QList<pipeline::OutputPort*> candidatePorts(
    pipeline::OutputPort* port, Scope scope);

  /// Ports in the same scope that could be written but no longer can:
  /// transient ports whose data has been released from memory. The
  /// dialog names them so the user knows why they are missing.
  static QList<pipeline::OutputPort*> releasedPorts(
    pipeline::Pipeline* pipeline, Scope scope);
  static QList<pipeline::OutputPort*> releasedPorts(pipeline::Node* node,
                                                    Scope scope);
  static QList<pipeline::OutputPort*> releasedPorts(
    pipeline::OutputPort* port, Scope scope);

  /// Resolve the files each port in @a ports produces under @a directory.
  /// A format that can hold every array of a volume gets one file named
  /// `{node}_{port}.{extension}`; one that can't gets one file per array,
  /// named `{node}_{port}_{array}.{extension}`. Node labels aren't unique,
  /// so labels shared by more than one node gain a 1-based numeric suffix;
  /// every component is sanitized, and names that still collide afterwards
  /// get a numeric suffix of their own.
  ///
  /// @a arrayNames supplies each port's scalar array names (see
  /// PortDataWriter::arrayNames) — passed in rather than read from the
  /// ports so callers can cache it across replans. @a formats maps a
  /// port-type group (see PortDataWriter::formatGroup) to the format its
  /// ports should be written in; ports whose group is absent are skipped.
  static QList<PortPlan> planPorts(
    const QList<pipeline::OutputPort*>& ports,
    const QHash<pipeline::OutputPort*, QStringList>& arrayNames,
    const QString& directory,
    const QHash<pipeline::PortType, PortFormat>& formats);

private:
  Q_DISABLE_COPY(SaveDataDialog)

  /// Shared body of the constructors.
  void init();
  /// Pin the scope for the node- and port-restricted modes.
  void restrictScope();
  /// The ports the current restriction and scope admit.
  QList<pipeline::OutputPort*> currentCandidates() const;
  /// releasedPorts() for the current restriction and scope.
  QList<pipeline::OutputPort*> currentReleased() const;
  void browseForDirectory();
  /// Recompute the file list from the current scope, formats and
  /// destination, and repopulate the tree.
  void rebuildPlan();
  void updateSummary();
  void setAllChecked(bool checked);
  void rememberCheckState();
  void restoreSettings();
  void saveSettings() const;
  Scope currentScope() const;
  PortFormat formatFor(pipeline::PortType type) const;

  QScopedPointer<Ui::SaveDataDialog> m_ui;
  /// Exactly one of these is set, narrowest first: a single port, a
  /// single node, or the whole graph.
  pipeline::OutputPort* m_port = nullptr;
  pipeline::Node* m_node = nullptr;
  pipeline::Pipeline* m_pipeline = nullptr;
  /// One per tree row, in display order.
  QList<PortPlan> m_plans;
  /// Scalar-array names per port. Filling this materializes the payload,
  /// so it is cached across rebuilds to avoid re-reading on-disk data
  /// every time a format or scope changes.
  QHash<pipeline::OutputPort*, QStringList> m_arrayNames;
  /// Ports the user has explicitly unchecked, so a rebuild doesn't
  /// silently re-select them.
  QSet<pipeline::OutputPort*> m_unchecked;
};

} // namespace tomviz

#endif
