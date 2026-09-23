/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveLoadTemplateReaction.h"

#include "ActiveObjects.h"
#include "RecentFilesMenu.h"
#include "Tvh5Format.h"
#include "pipeline/LegacyStateLoader.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineStateIO.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SinkNode.h"
#include "pipeline/SourceNode.h"
#include "Utilities.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSet>
#include <QString>

namespace tomviz {

SaveLoadTemplateReaction::SaveLoadTemplateReaction(QAction* parentObject, bool load, QString filename)
  : pqReaction(parentObject), m_load(load), m_filename(filename)
{}

void SaveLoadTemplateReaction::onTriggered()
{
  if (m_load) {
    loadTemplate(m_filename);
  } else {
    saveTemplate();
  }
}

bool SaveLoadTemplateReaction::saveTemplate()
{
  bool ok;
  QString text = QInputDialog::getText(tomviz::mainWidget(), tr("Save Pipeline Template"),
                                         tr("Template Name:"), QLineEdit::Normal, QString(), &ok);
  QString fileName = text.replace(" ", "_");
  if (ok && !text.isEmpty()) {
    QString path;
    // If TOMVIZ_PIPELINE_TEMPLATES_PATH is set, save to the first path it
    // lists; otherwise fall back to <user data path>/templates.
    QByteArray envOverride = qgetenv("TOMVIZ_PIPELINE_TEMPLATES_PATH");
    if (!envOverride.isEmpty()) {
      QStringList envPaths = QString::fromLocal8Bit(envOverride)
                               .split(QDir::listSeparator(), Qt::SkipEmptyParts);
      if (!envPaths.isEmpty()) {
        path = envPaths.first();
      }
    }
    if (path.isEmpty()) {
      path = tomviz::userTemplatesPath();
    }
    if (path.isEmpty()) {
      return false;
    }
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(path)) {
      QMessageBox::warning(
        tomviz::mainWidget(), "Could not create tomviz directory",
        QString("Could not create tomviz directory '%1'.").arg(path));
      return false;
    }
    QString destination =
      QString("%1%2%3.tvsm").arg(path).arg(QDir::separator()).arg(fileName);
    return SaveLoadTemplateReaction::saveTemplate(destination);
  }
  return false;
}

namespace {

bool loadTemplateFromJson(const QJsonObject& state, pipeline::Pipeline* pip)
{
  if (state.value("schemaVersion").toInt(1) < 2) {
    // Pre-schemaVersion files: templates stored their chain as a
    // top-level "operators" array, full state files nest the same
    // shape under "dataSources[].operators". Try the template form
    // first, then fall back to walking dataSources so a full v1 state
    // can also be ingested as a transform-only fragment.
    auto topLevel = state.value("operators").toArray();
    if (!topLevel.isEmpty()) {
      pipeline::LegacyStateLoader::loadTemplateOperators(topLevel, pip);
      return true;
    }
    for (const auto& dsVal : state.value("dataSources").toArray()) {
      auto ds = dsVal.toObject();
      pipeline::LegacyStateLoader::loadTemplateOperators(
        ds.value("operators").toArray(), pip);
    }
    return true;
  }

  auto pipelineJson = state.value("pipeline").toObject();
  if (pipelineJson.isEmpty()) {
    qWarning("Template file missing 'pipeline' section.");
    return false;
  }

  // Sanitize: keep only transform nodes. A correctly-saved template
  // already excludes sources/sinks/sink groups, but defend against
  // hand-edited or older files. Type names are dotted; the registry
  // uses the prefixes source./transform./sink. and the bare "sinkGroup".
  auto isExcludedType = [](const QString& type) {
    return type.startsWith("source.") || type.startsWith("sink.") ||
           type == QLatin1String("sinkGroup");
  };

  QSet<int> droppedIds;
  QJsonArray keptNodes;
  for (const auto& nv : pipelineJson.value("nodes").toArray()) {
    auto entry = nv.toObject();
    int id = entry.value("id").toInt(-1);
    if (id < 0 || isExcludedType(entry.value("type").toString())) {
      if (id >= 0) {
        droppedIds.insert(id);
      }
      continue;
    }
    keptNodes.append(entry);
  }

  QJsonArray keptLinks;
  for (const auto& lv : pipelineJson.value("links").toArray()) {
    auto entry = lv.toObject();
    int fromId = entry.value("from").toObject().value("node").toInt(-1);
    int toId = entry.value("to").toObject().value("node").toInt(-1);
    if (droppedIds.contains(fromId) || droppedIds.contains(toId)) {
      continue;
    }
    keptLinks.append(entry);
  }

  // Remap template ids to fresh ones from the active pipeline's
  // allocator so they can't collide with nodes already in the session.
  QHash<int, int> idMap;
  QJsonArray remappedNodes;
  for (const auto& nv : keptNodes) {
    auto entry = nv.toObject();
    int oldId = entry.value("id").toInt(-1);
    int newId = pip->nextNodeId();
    pip->setNextNodeId(newId + 1);
    idMap.insert(oldId, newId);
    entry["id"] = newId;
    remappedNodes.append(entry);
  }

  QJsonArray remappedLinks;
  for (const auto& lv : keptLinks) {
    auto entry = lv.toObject();
    auto from = entry.value("from").toObject();
    auto to = entry.value("to").toObject();
    from["node"] = idMap.value(from.value("node").toInt(-1), -1);
    to["node"] = idMap.value(to.value("node").toInt(-1), -1);
    entry["from"] = from;
    entry["to"] = to;
    remappedLinks.append(entry);
  }

  // Hand a minimal state object to PipelineStateIO::load. Drop
  // nextNodeId so it doesn't clobber the allocator we just advanced,
  // and ignore any views/layouts/palette that may be present.
  QJsonObject loadPipeline;
  loadPipeline["nodes"] = remappedNodes;
  loadPipeline["links"] = remappedLinks;

  QJsonObject loadState;
  loadState["schemaVersion"] = 2;
  loadState["pipeline"] = loadPipeline;

  if (!pipeline::PipelineStateIO::load(pip, loadState)) {
    return false;
  }

  // Parameters came from the template, so the nodes shouldn't be "New"
  // — that's the state the link-completion auto-edit-dialog uses to
  // decide a freshly-dropped node still needs to be configured.
  // Anything that survived as New here is just missing data.
  for (int newId : idMap.values()) {
    if (auto* n = pip->nodeById(newId)) {
      if (n->state() == pipeline::NodeState::New) {
        n->setStateNoCascade(pipeline::NodeState::Stale);
      }
    }
  }
  return true;
}

} // namespace

bool SaveLoadTemplateReaction::loadTemplate(const QString& fileName)
{
  auto* pip = ActiveObjects::instance().pipeline();
  if (!pip) {
    qWarning("No active pipeline to load template into.");
    return false;
  }

  QJsonObject state;
  if (fileName.endsWith(".tvh5", Qt::CaseInsensitive)) {
    state = Tvh5Format::readState(fileName.toStdString());
    if (state.isEmpty()) {
      qWarning() << "Could not read /tomviz_state from:" << fileName;
      return false;
    }
  } else {
    QFile openFile(fileName);
    if (!openFile.open(QIODevice::ReadOnly)) {
      qWarning() << "Couldn't open template file:" << fileName;
      return false;
    }
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(openFile.readAll(), &error);
    if (!doc.isObject()) {
      qWarning() << "Invalid template file:" << fileName << error.errorString();
      return false;
    }
    state = doc.object();
  }

  return loadTemplateFromJson(state, pip);
}

bool SaveLoadTemplateReaction::loadTemplateWithDialog()
{
  QStringList filters;
  filters << "Tomviz state files (*.tvsm *.tvh5)"
          << "All files (*)";
  QFileDialog dialog(tomviz::mainWidget(), tr("Load Template"), QString(),
                     filters.join(";;"));
  dialog.setObjectName("LoadTemplateDialog");
  dialog.setFileMode(QFileDialog::ExistingFile);
  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }
  QString fileName = dialog.selectedFiles().value(0);
  if (fileName.isEmpty()) {
    return false;
  }
  if (!loadTemplate(fileName)) {
    return false;
  }
  RecentFilesMenu::pushTemplateFile(fileName);
  return true;
}

bool SaveLoadTemplateReaction::saveTemplateAs()
{
  QString tvsmFilter = "Tomviz state files (*.tvsm)";
  QStringList filters;
  filters << tvsmFilter << "All files (*)";
  QFileDialog dialog(tomviz::mainWidget(), tr("Save Template As"), QString(),
                     filters.join(";;"));
  dialog.setObjectName("SaveTemplateDialog");
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setFileMode(QFileDialog::AnyFile);
  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }
  QString fileName = dialog.selectedFiles().value(0);
  if (fileName.isEmpty()) {
    return false;
  }
  if (dialog.selectedNameFilter() == tvsmFilter &&
      !fileName.endsWith(".tvsm", Qt::CaseInsensitive)) {
    fileName += ".tvsm";
  }
  return saveTemplate(fileName);
}

bool SaveLoadTemplateReaction::saveTemplate(const QString& fileName)
{
  auto* pip = ActiveObjects::instance().pipeline();
  if (!pip) {
    qWarning("No active pipeline to save as template.");
    return false;
  }

  QJsonObject state;
  if (!pipeline::PipelineStateIO::save(pip, state)) {
    return false;
  }

  // A template is a transform-only graph fragment. Drop sources, sinks,
  // and sink groups (plus any link touching them). What's left is the
  // transform graph and its connectivity — to be applied to whatever
  // data the user loads later.
  QSet<int> excludedIds;
  for (auto* node : pip->nodes()) {
    if (dynamic_cast<pipeline::SourceNode*>(node) ||
        dynamic_cast<pipeline::SinkNode*>(node) ||
        dynamic_cast<pipeline::SinkGroupNode*>(node)) {
      excludedIds.insert(pip->nodeId(node));
    }
  }

  QJsonObject pipelineJson = state.value("pipeline").toObject();

  QJsonArray filteredNodes;
  for (const auto& nv : pipelineJson.value("nodes").toArray()) {
    auto entry = nv.toObject();
    if (excludedIds.contains(entry.value("id").toInt(-1))) {
      continue;
    }
    filteredNodes.append(entry);
  }
  pipelineJson["nodes"] = filteredNodes;

  QJsonArray filteredLinks;
  for (const auto& lv : pipelineJson.value("links").toArray()) {
    auto entry = lv.toObject();
    int fromId = entry.value("from").toObject().value("node").toInt(-1);
    int toId = entry.value("to").toObject().value("node").toInt(-1);
    if (excludedIds.contains(fromId) || excludedIds.contains(toId)) {
      continue;
    }
    filteredLinks.append(entry);
  }
  pipelineJson["links"] = filteredLinks;

  state["pipeline"] = pipelineJson;

  QFile saveFile(fileName);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning() << "Could not open template file for writing:" << fileName;
    return false;
  }
  QJsonDocument doc(state);
  return saveFile.write(doc.toJson()) != -1;
}

} // namespace tomviz
