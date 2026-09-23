/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeEditDialog_h
#define tomvizPipelineNodeEditDialog_h

#include "DeferredLinkInfo.h"

#include <QDialog>
#include <QPointer>

class QDialogButtonBox;
class QCloseEvent;
class QShowEvent;

namespace tomviz {
namespace pipeline {

class EditNodeWidget;
class Node;
class Pipeline;

/// A dialog for configuring node parameters with Apply/OK/Cancel.
///
/// Two modes:
///   - **Insertion mode**: The node has already been added and spliced into
///     the pipeline (eagerly, so the preview shows its final position).
///     Apply/OK executes the pipeline and commits. Cancel removes the node
///     and restores the original links and node states captured in
///     DeferredLinkInfo, so it is a true no-op.
///   - **Edit mode**: The node already exists. Apply/OK re-applies
///     parameters and executes. Cancel just closes the dialog.
///
/// Editor gating (e.g. waiting for input data) is handled inside the
/// EditNodeWidget; this dialog just gates Apply/OK on canApply() and
/// the pipeline's executing state.
class NodeEditDialog : public QDialog
{
  Q_OBJECT

public:
  /// Edit an existing node (edit mode).
  NodeEditDialog(Node* node, Pipeline* pipeline, QWidget* parent = nullptr);

  /// Insert a new node (insertion mode). For source-shaped nodes the
  /// DeferredLinkInfo can be empty.
  NodeEditDialog(Node* node, Pipeline* pipeline,
                 const DeferredLinkInfo& deferred,
                 QWidget* parent = nullptr);

  ~NodeEditDialog() override;

  Node* node() const;

  /// Overridden so that every cancel path -- the Cancel button, the Escape
  /// key, and the window close button -- funnels through here and rolls back
  /// an in-progress insertion.
  void reject() override;

signals:
  /// Emitted after Apply/OK completes an insertion.
  void insertionCompleted(Node* node);

  /// Emitted after Cancel aborts an insertion.
  void insertionCanceled();

private slots:
  void onApply();
  void onOkay();

protected:
  void showEvent(QShowEvent* event) override;
  /// Close is refused while an apply is in progress: applying can
  /// spin a nested event loop, and destroying the dialog under that
  /// loop would unwind through freed objects.
  void closeEvent(QCloseEvent* event) override;

private:
  bool m_applying = false;
  void init();
  void refreshButtonEnablement();
  void saveGeometry();
  void restoreGeometry();

  void completeInsertion();
  /// Hand the Python editor's script and description to a
  /// CustomOperatorEditDialog draft in the user directory.
  void saveAsCustomOperator();

  QPointer<Node> m_node;
  Pipeline* m_pipeline;
  EditNodeWidget* m_editWidget = nullptr;
  QDialogButtonBox* m_buttonBox = nullptr;
  DeferredLinkInfo m_deferred;
  bool m_isNewInsertion;
  bool m_insertionCompleted = false;
};

} // namespace pipeline
} // namespace tomviz

#endif
