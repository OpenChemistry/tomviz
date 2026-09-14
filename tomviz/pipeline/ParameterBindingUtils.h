/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineParameterBindingUtils_h
#define tomvizPipelineParameterBindingUtils_h

#include <QMap>
#include <QString>

class QJsonObject;
class QWidget;

namespace tomviz {
namespace pipeline {

class Node;

/// Declares a live link between a parameter's editor control and a
/// property on a sink node connected to the same pipeline branch.
/// The binding is resolved at widget-open time and torn down with
/// the widget; nothing about it is persisted.
struct ParameterBinding
{
  QString sinkType;     // e.g. "SliceSink"
  QString sinkProperty; // e.g. "slice"
};

/// Walk an operator JSON description's "parameters" array and return
/// the bindings declared via each parameter's optional "bindToSink"
/// object. Parameters without the hint are simply omitted.
QMap<QString, ParameterBinding> parseParameterBindings(
  const QJsonObject& description);

/// For each binding, find a matching sink reachable through @a node's
/// port topology and wire two-way connections between the named
/// parameter's control under @a widget and the sink's property.
/// Connections are parented to the control — Qt auto-disconnects when
/// it is destroyed, so calling again after the controls are rebuilt
/// is safe. Controls that do not exist yet are skipped.
void wireParameterBindings(Node* node, QWidget* widget,
                           const QMap<QString, ParameterBinding>& bindings);

} // namespace pipeline
} // namespace tomviz

#endif
