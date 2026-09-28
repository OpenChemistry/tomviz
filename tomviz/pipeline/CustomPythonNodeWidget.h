/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineCustomPythonNodeWidget_h
#define tomvizPipelineCustomPythonNodeWidget_h

#include <QMap>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace tomviz {
namespace pipeline {

class Node;
class Pipeline;

/// Base class for custom parameter widgets that replace the
/// auto-generated parameter UI for specific Python nodes (sources or
/// transforms). Concrete subclasses are registered with
/// :func:`registerCustomNodeWidget` keyed on the JSON description's
/// ``widget`` field.
class CustomPythonNodeWidget : public QWidget
{
  Q_OBJECT

public:
  CustomPythonNodeWidget(QWidget* parent = nullptr);
  ~CustomPythonNodeWidget() override;

  virtual void getValues(QMap<QString, QVariant>& map) = 0;
  virtual void setValues(const QMap<QString, QVariant>& map) = 0;

  /// Optional context: the node whose editor hosts this widget and the
  /// pipeline it belongs to. The editor factory calls this right after
  /// construction, before setValues(). Widgets that interact with the
  /// wider pipeline (e.g. to preview against an upstream port's sink,
  /// or to enumerate other ports) override this; the default ignores
  /// it. Both pointers outlive the widget for the editor's lifetime,
  /// but @a node's links can change while the editor is open, so
  /// consumers should re-resolve ports rather than cache them blindly.
  virtual void setNodeContext(Node* node, Pipeline* pipeline)
  {
    Q_UNUSED(node)
    Q_UNUSED(pipeline)
  }

  /// Keep a copy of the current script (including edits) in case the
  /// custom widget needs to use it (e.g. for running test Python code).
  virtual void setScript(const QString& script) { m_script = script; }

  /// The node's JSON description as it currently stands in the editor's
  /// Definition tab — which is not necessarily the one the node is
  /// running, since the user may be part-way through editing it.
  ///
  /// Called once at construction and again on every edit that changes
  /// the parameter declarations. Subclasses that build any of their UI
  /// from the description (e.g. by embedding a NodePropertiesWidget
  /// alongside their bespoke controls) should override this and
  /// re-render, so descriptor edits are reflected the same way they are
  /// in the auto-generated form. The default just stores it.
  virtual void setJSONDescription(const QString& json)
  {
    m_jsonDescription = json;
  }

  /// Called when the operator is applied (e.g. to persist settings).
  virtual void writeSettings() {}

protected:
  QString m_script;
  QString m_jsonDescription;
};

} // namespace pipeline
} // namespace tomviz

#endif
