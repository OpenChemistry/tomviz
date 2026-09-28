/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLegacyPythonTransform_h
#define tomvizPipelineLegacyPythonTransform_h

#include "CustomNodeWidgetRegistry.h"
#include "ParameterBindingUtils.h"
#include "TransformNode.h"

#include <QJsonArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace tomviz {
namespace pipeline {

/// A TransformNode that loads and executes an existing tomviz Python operator
/// described by a JSON description and Python script file pair.
///
/// This enables the ~59 existing Python operators (e.g. AddConstant.py/.json)
/// to run unchanged within the new pipeline framework. Python execution uses
/// direct pybind11/CPython — no dependency on the old tomvizlib.
class LegacyPythonTransform : public TransformNode
{
  Q_OBJECT

public:
  LegacyPythonTransform(QObject* parent = nullptr);
  ~LegacyPythonTransform() override = default;

  /// The description's "inheritColorMap" (default true).
  bool inheritsColorMap() const override { return m_inheritColorMap; }

  /// Load from JSON description string
  void setJSONDescription(const QString& json);
  QString jsonDescription() const;

  /// Replace the description on a node that already exists, as the user
  /// does from the editor's Definition tab. Re-derives parameters and
  /// flags but creates no ports, and leaves the label and executor
  /// alone — the editor's own Name field and Execution tab own those,
  /// and are applied alongside this. Returns the names of parameters
  /// whose values could not be carried over.
  QStringList reconfigureDescription(const QString& json);

  /// Set the Python script source code
  void setScript(const QString& script);
  QString scriptSource() const;

  /// Parameter access (populated from JSON defaults, overridable)
  void setParameter(const QString& name, const QVariant& value);
  QVariant parameter(const QString& name) const;
  QMap<QString, QVariant> parameters() const;

  /// The operator name from the JSON description
  QString operatorName() const;

  /// The custom widget ID from the JSON "widget" field (empty if none).
  QString customWidgetID() const;

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  /// @a createPorts and @a applyLabel are false when re-parsing a
  /// description on a node that already exists: port creation here is
  /// purely additive (it would duplicate the `results` and `dataset`
  /// ports), and the label belongs to the editor's Name field once the
  /// node is live.
  void parseJSON(bool createPorts = true, bool applyLabel = true);

  QString m_jsonDescription;
  QString m_script;
  QString m_operatorName;
  QMap<QString, QVariant> m_parameters;
  // Declared parameter type (per the operator JSON description's
  // `parameters[*].type`) — needed at deserialize time because Qt6
  // collapses every JSON number into QVariant<double>, so we'd
  // otherwise pass `axis: 2` to Python as 2.0 and break operators that
  // index with it.
  QMap<QString, QString> m_parameterTypes;
  // For each enumeration parameter, the options array from the JSON
  // description — captured at parseJSON time so deserialize can
  // resolve a saved option index back to its value.
  QMap<QString, QJsonArray> m_enumOptions;
  // Parameter bindings declared via the JSON `bindToSink` hint.
  // Resolved at widget-open time, never persisted.
  QMap<QString, ParameterBinding> m_parameterBindings;
  QString m_customWidgetID;
  QStringList m_resultNames;
  bool m_inheritColorMap = true;
  QStringList m_resultTypes;
  QStringList m_datasetInputNames;
  QString m_primaryOutputName = QStringLiteral("volume");
  QString m_childName;  // Non-empty when JSON declares a "children" entry
};

} // namespace pipeline
} // namespace tomviz

#endif
