/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonNodeBackend_h
#define tomvizPipelinePythonNodeBackend_h

#include "ParameterBindingUtils.h"
#include "PortData.h"
#include "PortType.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>

namespace tomviz {
namespace pipeline {

class Node;
class OutputPort;

/// Non-QObject helper that owns all of a schema-v2 Python node's
/// state — JSON description, Python script, parsed input/output
/// declarations, and current parameter values — and runs the user's
/// Python class.
///
/// PythonSource and PythonTransform each own one PythonNodeBackend and
/// delegate to it. The backend is intentionally agnostic about whether
/// it's serving a source or a transform: the source shell calls
/// runSource() (which dispatches to the user's Node.produce()), the
/// transform shell calls runTransform() (which dispatches to
/// Node.transform(inputs, ...)). All other plumbing — parsing,
/// serialization, parameter management, Python interpreter setup — is
/// shared.
class PythonNodeBackend
{
public:
  /// Callbacks supplied by the host shell so the backend can create
  /// ports without needing direct access to the Node's protected
  /// addInputPort / addOutputPort. The signatures match
  /// TransformNode::addInput / addOutput so the shells forward
  /// directly. A null AddInputFn skips inputs (used by the source
  /// shell, which has none); a null AddOutputFn skips outputs.
  using AddInputFn = std::function<void(const QString&, PortType)>;
  using AddOutputFn = std::function<OutputPort*(const QString&, PortType)>;

  PythonNodeBackend();
  ~PythonNodeBackend() = default;

  // ---- description / script -----------------------------------------
  /// Set the operator JSON description string. Re-parses defaults from
  /// the description but does NOT touch the host's ports — call
  /// applyDescription() after this when the host is ready to receive
  /// port-creation calls.
  void setJSONDescription(const QString& json);
  QString jsonDescription() const;

  /// Replace the description on a node that is already live, as the
  /// user does from the editor's Definition tab.
  ///
  /// Unlike setJSONDescription(), which re-seeds every parameter from
  /// its declared default, this keeps the values the user has already
  /// set wherever the new description still declares them at the same
  /// type. Ports are deliberately untouched: the caller is expected to
  /// have rejected any description whose port declarations moved (see
  /// validateNodeDefinition), because nothing here can remove a port or
  /// the links hanging off one.
  ///
  /// Returns the names of parameters whose values could not be carried
  /// over and fell back to the new defaults.
  QStringList reconfigure(const QString& json);

  /// Set the Python script source code.
  void setScript(const QString& script);
  QString scriptSource() const;

  /// Apply the parsed description to the host's ports. Calls @a
  /// addInput once per "inputs" entry and @a addOutput once per
  /// "outputs" entry, then applies per-output persistency (defaults
  /// to false / transient when "persistent" is omitted, per the
  /// schema-v2 convention).
  void applyDescription(AddInputFn addInput, AddOutputFn addOutput);

  // ---- parsed metadata ----------------------------------------------
  QString operatorName() const;
  QString defaultLabel() const;
  QString descriptionText() const;
  QString helpText() const;
  QString customWidgetID() const;
  /// Schema-v2 ``supportsCancel`` flag (default false). Shells use this
  /// to drive Node::setSupportsCancel.
  bool supportsCancel() const;
  /// Schema-v2 ``supportsComplete`` flag (default false). Shells use
  /// this to drive Node::setSupportsCompletion.
  bool supportsComplete() const;
  /// The description's "inheritColorMap" (default true).
  bool inheritsColorMap() const;
  /// Schema-v2 description shape: true if the JSON declared a
  /// non-empty ``inputs`` array. Used by the factory routing to
  /// validate that a transform-shape description is paired with the
  /// transform shell (and vice versa for sources).
  bool isTransformShape() const;
  /// Optional ``tomviz_pipeline_env`` field carried by older operator
  /// descriptions. The shells synthesize an ExternalNodeExecutor at
  /// deserialize time when set and no explicit executor was loaded.
  /// Empty when not declared.
  QString externalPythonEnvPath() const;
  /// Optional ``externalOnly`` flag (default false). Marks operators
  /// whose dependencies can never be imported in the application
  /// environment: shells install an ExternalNodeExecutor on fresh
  /// nodes, and the editor disables the Internal executor choice.
  bool externalOnly() const;

  // ---- parameters ---------------------------------------------------
  /// Accessors copy / write under a lock: a running kernel may write
  /// back parameters from the pipeline worker thread (see
  /// applyParameterUpdates) while the GUI reads them.
  void setParameter(const QString& name, const QVariant& value);
  QVariant parameter(const QString& name) const;
  QMap<QString, QVariant> parameters() const;

  /// Install values the kernel changed through `self.set_parameter`
  /// (harvested after produce / transform / should_auto_execute, or
  /// read back from an external run). Only entries whose value
  /// actually differs are written; returns those, so the host shell
  /// can emit Node::parametersUpdated. Never marks anything stale —
  /// see Node::applyParameterUpdates for the rationale.
  QMap<QString, QVariant> applyParameterUpdates(
    const QMap<QString, QVariant>& updates);

  /// Bindings declared via the JSON `bindToSink` hint, parsed at
  /// description time. Resolved to live signal/slot wiring at
  /// widget-open time by ParameterBindingUtils::wireParameterBindings.
  QMap<QString, ParameterBinding> parameterBindings() const;

  // ---- serialize / deserialize --------------------------------------
  /// Append the backend's serialized fields (description, script,
  /// arguments) to @a base and return the merged object. Designed to
  /// be called from a shell's serialize() override after it has called
  /// the Node-base serializer.
  QJsonObject serializeInto(QJsonObject base) const;

  /// Apply schema-v2 fields ("description", "script", "arguments") onto
  /// this backend. Description is applied first (so applyDescription()
  /// can run with the freshly-parsed defaults), then arguments
  /// override the defaults. The shell typically calls this before
  /// Node::deserialize so per-port state can land on the just-created
  /// ports.
  void applySerializedFields(const QJsonObject& json,
                             AddInputFn addInput, AddOutputFn addOutput);

  // ---- execution ----------------------------------------------------
  /// Run the user's Python class as a transform. Loads the script,
  /// finds a single subclass of tomviz.nodes.TransformNode, builds an
  /// inputs dict from @a inputs and a kwargs dict from the current
  /// parameters, and calls instance.transform(inputs, **kwargs).
  /// Returns the output port → PortData map.
  QMap<QString, PortData> runTransform(
    Node* host, const QMap<QString, PortData>& inputs);

  /// Run the user's Python class as a source. Loads the script, finds
  /// a single subclass of tomviz.nodes.SourceNode, and calls
  /// instance.produce(**kwargs). Returns the output port → PortData
  /// map.
  QMap<QString, PortData> runSource(Node* host);

  /// Run the user's should_auto_execute hook (the auto-execute poll)
  /// and return its answer. Same instantiation as runSource /
  /// runTransform — the wrapper and the host's user-state bag are
  /// injected, and state mutations made by the hook are harvested back
  /// onto the host — but no inputs are converted and no outputs are
  /// produced. Any error answers false.
  bool runShouldAutoExecute(Node* host);

  // ---- input/output declarations (parsed from description) ----------
  /// Names of input ports declared in the description, in declaration
  /// order. Mirror of what was passed to addInput in
  /// applyDescription(); useful for the shells to validate
  /// source-vs-transform consistency at load time.
  QStringList inputNames() const;
  QStringList outputNames() const;
  QString primaryOutputName() const;

private:
  /// Reparse m_jsonDescription into the parameter/spec/metadata
  /// members. The caller must hold m_parametersMutex.
  void parseDescriptionLocked();
  QMap<QString, PortData> runImpl(Node* host,
                                  const QMap<QString, PortData>& inputs,
                                  bool isSource);

  // Raw inputs.
  QString m_jsonDescription;
  QString m_script;

  // Parsed metadata.
  QString m_operatorName;
  QString m_defaultLabel;
  QString m_description;
  QString m_help;
  QString m_customWidgetID;
  bool m_supportsCancel = false;
  bool m_supportsComplete = false;
  bool m_inheritColorMap = true;
  QString m_externalPythonEnvPath;
  bool m_externalOnly = false;

  struct PortSpec
  {
    QString name;
    PortType type = PortType::None;
    bool persistent = false; // outputs only; ignored for inputs
    bool persistentSpecified =
      false; // whether the operator JSON explicitly carried "persistent"
  };
  QList<PortSpec> m_inputs;
  QList<PortSpec> m_outputs;

  // Parameters: current values plus per-name declared type and (for
  // enum parameters) the options array. Same shape as the legacy
  // bookkeeping in LegacyPythonTransform; needed for state-file
  // round-tripping under Qt6's lossy QJsonValue::toVariant.
  QMap<QString, QVariant> m_parameters;
  QMap<QString, QString> m_parameterTypes;
  QMap<QString, QJsonArray> m_enumOptions;
  // name -> the description's parameter entry, verbatim. Handed to the
  // kernel instance as `_parameter_spec` so `self.set_parameter` can
  // validate names and coerce values against the declared types.
  QMap<QString, QJsonObject> m_parameterSpecs;
  QMap<QString, ParameterBinding> m_parameterBindings;
  /// Guards m_parameters: executions write back kernel updates from a
  /// worker thread while the GUI reads the current values.
  mutable QMutex m_parametersMutex;
};

} // namespace pipeline
} // namespace tomviz

#endif
