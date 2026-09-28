/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AddPythonSourceReaction.h"

#include "LoadDataReaction.h"
#include "MainWindow.h"

#include "pipeline/DeferredLinkInfo.h"
#include "pipeline/Node.h"
#include "pipeline/NodeEditDialog.h"
#include "pipeline/SourceNode.h"
#include "pipeline/sources/PythonSource.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace tomviz {

AddPythonSourceReaction::AddPythonSourceReaction(QAction* parentObject,
                                                 const QString& script,
                                                 const QString& json)
  : pqReaction(parentObject), m_script(script), m_json(json)
{
}

void AddPythonSourceReaction::onTriggered()
{
  auto* mainWindow = MainWindow::instance();
  auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
  if (!pip) {
    return;
  }

  // Build the source up-front so the editor dialog can read its JSON
  // description, script, and parameter defaults.
  auto* source = new pipeline::PythonSource();
  source->setJSONDescription(m_json);
  source->setScript(m_script);

  // A description can ask for periodic execution from the start, for
  // sources that exist to be polled. The setting is the node's from
  // here on, so the Execution tab shows and edits it as usual.
  const QJsonObject description =
    QJsonDocument::fromJson(m_json.toUtf8()).object();
  if (description.contains(QStringLiteral("autoExecute"))) {
    const QJsonObject autoExec =
      description.value(QStringLiteral("autoExecute")).toObject();
    source->setAutoExecuteIntervalSeconds(
      autoExec.value(QStringLiteral("intervalSeconds"))
        .toInt(source->autoExecuteIntervalSeconds()));
    source->setAutoExecuteEnabled(
      autoExec.value(QStringLiteral("enabled")).toBool(false));
  }

  // Add the source to the pipeline before opening the dialog. This is
  // what gives us the cancel-rollback symmetry with transform
  // insertion: NodeEditDialog::reject() removes the node from the
  // pipeline (and deletes it). On OK we finish the standard source
  // scaffolding (sinks, color map, execute) via completeSourceSetup.
  LoadDataReaction::addSourceToPipeline(source);

  pipeline::DeferredLinkInfo deferred; // sources have no links to defer
  auto* dialog =
    new pipeline::NodeEditDialog(source, pip, deferred, mainWindow);
  dialog->setAttribute(Qt::WA_DeleteOnClose);

  QString title = description.value(QStringLiteral("label")).toString();
  if (title.isEmpty()) {
    title = source->label();
  }
  dialog->setWindowTitle(QString("Generate - %1").arg(title));

  // The dialog's onOkay/onApply already mark stale + execute and emit
  // insertionCompleted. Hook the signal to finish the source setup
  // (sink group + default modules). Cancel removes the source for us.
  QObject::connect(dialog, &pipeline::NodeEditDialog::insertionCompleted,
                   dialog, [](pipeline::Node* node) {
                     if (auto* src =
                           qobject_cast<pipeline::SourceNode*>(node)) {
                       LoadDataReaction::completeSourceSetup(src);
                     }
                   });

  dialog->show();
}

} // namespace tomviz
