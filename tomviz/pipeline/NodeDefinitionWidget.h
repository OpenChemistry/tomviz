/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineNodeDefinitionWidget_h
#define tomvizPipelineNodeDefinitionWidget_h

#include "NodeDefinitionValidator.h"

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QTimer;

namespace tomviz {
namespace pipeline {

class NodeDefinitionFormWidget;

/// Editor for a Python node's JSON description, shown as the
/// "Definition" tab of PythonNodeEditorWidget.
///
/// Two interchangeable views over one buffer: a generated form and the
/// raw JSON text, switched by the "Raw" toggle. The QTextEdit is the
/// single source of truth — a form edit serializes straight back into it
/// — so validation and everything downstream have exactly one input
/// regardless of which view produced it.
///
/// Validation runs on a short debounce after every keystroke rather than
/// on Apply, so the editor can report what a change will do (and refuse
/// the ones a live node can't survive) while it is being written. The
/// host gates its Apply/OK button on isValid(). Nothing here is written
/// to the node until Apply/OK, so cancelling the dialog discards the
/// edit — the same way parameter values already behave.
class NodeDefinitionWidget : public QWidget
{
  Q_OBJECT

public:
  /// For a live node: the identity and port checks apply.
  NodeDefinitionWidget(const QString& json, NodeShape shape,
                       DefinitionSchema schema, QWidget* parent = nullptr);
  /// @a target picks the validation rules; see DefinitionTarget. Shape and
  /// schema seed the form either way.
  NodeDefinitionWidget(const QString& json, NodeShape shape,
                       DefinitionSchema schema, DefinitionTarget target,
                       QWidget* parent = nullptr);

  /// Current editor text, whether or not it validates.
  QString definitionText() const;

  bool isValid() const;

  /// Validate immediately instead of waiting out the debounce. The host
  /// calls this before committing, so a keystroke typed a moment before
  /// Apply can't slip past validation or leave the Parameters tab
  /// rendering a description that is no longer on screen.
  void flushPendingValidation();

  /// Adopt @a json as the baseline that later edits are validated
  /// against. Called by the host once the description has actually been
  /// committed to the node.
  void markApplied(const QString& json);

signals:
  /// Emitted whenever isValid() changes.
  void validityChanged(bool valid);

  /// Emitted (debounced) when the text validates and its parameter
  /// declarations differ from those the host last rendered. The host
  /// rebuilds its Parameters tab from @a json so the form on screen
  /// always matches the description being edited.
  void parameterSchemaChanged(const QString& json);

private:
  void revalidate();
  void renderIssues(const DefinitionValidation& validation);
  /// Switch views. Leaving raw re-syncs the form from the text; if the
  /// text isn't valid JSON the form can't represent it, so the switch is
  /// refused and the user stays in the raw editor.
  void setRawMode(bool raw);
  /// Push the form's serialization into the text buffer, which is what
  /// actually triggers revalidation.
  void onFormChanged();

  NodeShape m_shape;
  DefinitionSchema m_schema;
  DefinitionTarget m_target = DefinitionTarget::LiveNode;
  /// The description the node is actually running — the baseline every
  /// candidate is validated and diffed against.
  QString m_appliedJson;
  /// The "parameters" array behind the currently-rendered form, so a
  /// keystroke that leaves it alone doesn't tear the form down.
  QJsonArray m_renderedParameters;
  bool m_valid = true;
  /// Guards the form → text → form path from re-entering itself.
  bool m_syncing = false;

  QStackedWidget* m_stack = nullptr;
  NodeDefinitionFormWidget* m_form = nullptr;
  QTextEdit* m_editor = nullptr;
  QPushButton* m_rawButton = nullptr;
  QLabel* m_issueLabel = nullptr;
  QTimer* m_debounce = nullptr;
};

} // namespace pipeline
} // namespace tomviz

#endif
