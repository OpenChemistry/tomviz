/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonSource_h
#define tomvizPipelinePythonSource_h

#include "PythonNodeBackend.h"
#include "SourceNode.h"

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVariant>

namespace tomviz {
namespace pipeline {

class EditNodeWidget;
class Pipeline;

/// A schema-v2 Python source node. Owns a PythonNodeBackend that
/// handles JSON parsing, parameter management, and execution; this
/// shell only wires the backend into the SourceNode lifecycle.
///
/// The user-facing Python class inherits from
/// tomviz.nodes.SourceNode and implements
/// ``produce(self, **params) -> dict``.
class PythonSource : public SourceNode
{
  Q_OBJECT

public:
  PythonSource(QObject* parent = nullptr);
  ~PythonSource() override = default;

  void setJSONDescription(const QString& json);
  QString jsonDescription() const;

  /// Replace the description on a node that already exists, as the
  /// user does from the editor's Definition tab. Unlike
  /// setJSONDescription() this creates no ports and does not touch the
  /// label or the executor — the editor's own Name field and Execution
  /// tab own those, and are applied alongside this. Returns the names
  /// of parameters whose values could not be carried over.
  QStringList reconfigureDescription(const QString& json);

  void setScript(const QString& script);
  QString scriptSource() const;

  void setParameter(const QString& name, const QVariant& value);
  QVariant parameter(const QString& name) const;
  QMap<QString, QVariant> parameters() const;

  QString operatorName() const;

  bool execute() override;

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  /// Auto-execute poll: runs the script's should_auto_execute hook.
  bool queryShouldAutoExecute() override;

  /// Kernel write-back (`self.set_parameter`): install the values on
  /// the backend and emit parametersUpdated for those that changed.
  void applyParameterUpdates(const QVariantMap& updates) override;

private:
  PythonNodeBackend m_backend;
};

} // namespace pipeline
} // namespace tomviz

#endif
