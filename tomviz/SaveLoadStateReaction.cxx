/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveLoadStateReaction.h"

#include "ActiveObjects.h"
#include "AnimationSerializer.h"
#include "animations/AnimationSceneGuard.h"
#include "MainWindow.h"
#include "pipeline/LegacyStateLoader.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineStateIO.h"
#include "pipeline/InputPort.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "RecentFilesMenu.h"
#include "Tvh5Format.h"
#include "Utilities.h"
#include "ViewsLayoutsSerializer.h"

#include <vtkSMViewProxy.h>

#include <pqSettings.h>
#include <vtkSMProxyManager.h>

#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>

#include <vtk_pugixml.h>

namespace tomviz {

SaveLoadStateReaction::SaveLoadStateReaction(QAction* parentObject, bool load)
  : pqReaction(parentObject), m_load(load)
{}

void SaveLoadStateReaction::onTriggered()
{
  if (m_load) {
    loadState();
  } else {
    saveState();
  }
}

bool SaveLoadStateReaction::saveState()
{
  QString tvh5Filter = "Tomviz full state files (*.tvh5)";
  QString tvsmFilter = "Tomviz state files (*.tvsm)";
  QStringList filters;
  filters << tvh5Filter << tvsmFilter << "All files (*)";

  QFileDialog fileDialog(tomviz::mainWidget(), tr("Save State File"),
                         QString(), filters.join(";;"));
  fileDialog.setObjectName("SaveStateDialog");
  fileDialog.setAcceptMode(QFileDialog::AcceptSave);
  fileDialog.setFileMode(QFileDialog::AnyFile);
  if (fileDialog.exec() == QDialog::Accepted) {
    QString filename = fileDialog.selectedFiles()[0];
    QString format = fileDialog.selectedNameFilter();
    if (format == tvh5Filter && !filename.endsWith(".tvh5")) {
      filename = QString("%1%2").arg(filename, ".tvh5");
    } else if (format == tvsmFilter && !filename.endsWith(".tvsm")) {
      filename = QString("%1%2").arg(filename, ".tvsm");
    }
    bool success = SaveLoadStateReaction::saveState(filename);
    if (success) {
      // Only set the most recent state file if the user picked a file
      // to save via a file dialog, and the save was successful.
      MainWindow::instance()->setMostRecentStateFile(filename);
    }
    return success;
  }
  return false;
}

bool SaveLoadStateReaction::loadState()
{
  QStringList filters;
  filters << "Tomvis state files (*.tvsm *.tvh5)"
          << "All files (*)";

  QFileDialog fileDialog(tomviz::mainWidget(), tr("Load State File"),
                         QString(), filters.join(";;"));
  fileDialog.setObjectName("LoadStateDialog");
  fileDialog.setFileMode(QFileDialog::ExistingFile);
  if (fileDialog.exec() == QDialog::Accepted) {
    return SaveLoadStateReaction::loadState(fileDialog.selectedFiles()[0]);
  }
  return false;
}

bool SaveLoadStateReaction::loadState(const QString& filename)
{
  auto* pipeline = ActiveObjects::instance().pipeline();
  if (pipeline && !pipeline->nodes().isEmpty()) {
    if (QMessageBox::Yes !=
        QMessageBox::warning(tomviz::mainWidget(), "Load State Warning",
                             "Current data and operators will be cleared when "
                             "loading a state file.  Proceed anyway?",
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)) {
      return false;
    }
  }

  bool success = false;
  if (filename.endsWith(".tvh5")) {
    success = loadTvh5(filename, /*executePipelines=*/true);
  } else if (filename.endsWith(".tvsm")) {
    bool executePipelines = automaticallyExecutePipelines();
    success = loadTvsm(filename, executePipelines);
  } else {
    qCritical() << "Unknown state format for file: " << filename;
    return false;
  }

  if (success) {
    RecentFilesMenu::pushStateFile(filename);
    // Set the most recent state file if we successfully loaded a
    // state, whether it was done programmatically or via file dialog.
    // There is no main window to tell when a state file is loaded
    // headlessly, which is how the load path is tested.
    if (auto* window = MainWindow::instance()) {
      window->setMostRecentStateFile(filename);
    }
  }

  return success;
}

namespace {

/// Kick consume() on every LegacyModuleSink whose inputs are ready,
/// so pre-loaded upstream data gets wired into the VTK filter and
/// rendered. Does NOT call Pipeline::execute() — transforms and
/// sources are untouched. Used in the "don't auto execute" path so
/// the user sees whatever data was already restored (e.g. from .tvh5
/// payload groups) without any pipeline actually running.
void consumeReadySinks(pipeline::Pipeline* pipeline)
{
  for (auto* node : pipeline->nodes()) {
    auto* sink = dynamic_cast<pipeline::LegacyModuleSink*>(node);
    if (!sink) {
      continue;
    }
    bool ready = !sink->inputPorts().isEmpty();
    for (auto* in : sink->inputPorts()) {
      if (!in->link() || !in->hasData()) {
        ready = false;
        break;
      }
    }
    if (ready) {
      sink->execute();
    }
  }
}

/// Shared end-of-load orchestration for the new-format loaders.
/// Auto-execute path: unpause and schedule pipeline->execute(), which
///   walks the DAG (Current nodes skipped, Stale ones re-run).
/// Declined path: direct-consume any sink whose inputs are ready (so
///   pre-loaded data is displayed), then pause and fire
///   executionFinished so the deferred sink-deserialize lambdas apply
///   their saved visibility / colormaps against the now-Current sinks.
void finalizeNewFormatLoad(pipeline::Pipeline* pipeline,
                           bool executePipelines)
{
  if (executePipelines) {
    pipeline->setPaused(false);
    QTimer::singleShot(0, pipeline,
                       [pipeline]() { pipeline->execute(); });
    return;
  }
  consumeReadySinks(pipeline);
  pipeline->setPaused(true);
  QMetaObject::invokeMethod(pipeline, &pipeline::Pipeline::executionFinished,
                            Qt::QueuedConnection);
}

/// Active-view fallback: bind any LegacyModuleSink left without a
/// view to the application's active render view.
void bindFallbackView(pipeline::Pipeline* pipeline)
{
  ActiveObjects::instance().createRenderViewIfNeeded();
  auto* activeView = ActiveObjects::instance().activeView();
  if (activeView &&
      QString(activeView->GetXMLName()) != QLatin1String("RenderView")) {
    ActiveObjects::instance().setActiveViewToFirstRenderView();
    activeView = ActiveObjects::instance().activeView();
  }
  if (!activeView) {
    return;
  }
  for (auto* node : pipeline->nodes()) {
    if (auto* sink = dynamic_cast<pipeline::LegacyModuleSink*>(node)) {
      if (!sink->view()) {
        sink->initialize(activeView);
      }
    }
  }
}

} // namespace

bool SaveLoadStateReaction::loadTvh5(const QString& filename,
                                     bool executePipelines)
{
  // Peek at /tomviz_state to dispatch by format. Legacy .tvh5 files
  // have no schemaVersion (or < 2) and go through LegacyStateLoader.
  // New-format .tvh5 files carry their pipeline graph in the same
  // JSON and their voxels under /data/<nodeId>/<portName>.
  QJsonObject state = Tvh5Format::readState(filename.toStdString());
  int schemaVersion = state.value("schemaVersion").toInt(1);
  if (state.isEmpty() || schemaVersion < 2) {
    return pipeline::LegacyStateLoader::loadFromH5(filename, executePipelines);
  }

  auto* pipeline = ActiveObjects::instance().pipeline();
  if (!pipeline) {
    qWarning("No active pipeline for new-format .tvh5 load.");
    return false;
  }

  // A playback still running would tear down its own scene from
  // inside a tick
  interruptAnimationPlayback(/*rewind=*/false);
  pipeline->clear();

  QMap<int, vtkSMViewProxy*> viewIdMap;
  pipeline::LegacyStateLoader::restoreViewsLayoutsAndPalette(
    state, pipeline, &viewIdMap);

  // Source voxels live in HDF5 groups inside this file. A pre-execute
  // hook populates source output ports directly so eager-execute in
  // PipelineStateIO::load becomes a no-op for those sources.
  std::string fileNameStd = filename.toStdString();
  auto hook = [fileNameStd](pipeline::Pipeline* p,
                             const QJsonObject& pipelineJson) {
    Tvh5Format::populatePayloadData(p, pipelineJson, fileNameStd);
  };

  if (!pipeline::PipelineStateIO::load(pipeline, state, viewIdMap, hook)) {
    return false;
  }

  bindFallbackView(pipeline);
  AnimationSerializer::restore(state, pipeline);
  finalizeNewFormatLoad(pipeline, executePipelines);
  return true;
}

bool SaveLoadStateReaction::loadTvsm(const QString& filename,
                                     bool executePipelines)
{
  QFile openFile(filename);
  if (!openFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open state file.");
    return false;
  }

  QJsonParseError error;
  auto contents = openFile.readAll();
  auto doc = QJsonDocument::fromJson(contents, &error);
  bool legacyStateFile = false;
  if (doc.isNull()) {
    // See if user is trying to load a old XML base state file.
    if (error.error == QJsonParseError::IllegalValue) {
      legacyStateFile = checkForLegacyStateFileFormat(contents);
    }

    // If its a legacy state file we are done.
    if (legacyStateFile) {
      return false;
    }
  }

  if (doc.isObject()) {
    auto object = doc.object();
    int schemaVersion = object.value("schemaVersion").toInt(1);
    if (schemaVersion >= 2) {
      auto* pipeline = ActiveObjects::instance().pipeline();
      if (!pipeline) {
        qWarning("No active pipeline for new-format state load.");
        return false;
      }
      // Drop whatever was in the pipeline (matching the confirmation
      // dialog we already showed the user in loadState()).
      // A playback still running would tear down its own scene from
      // inside a tick
      interruptAnimationPlayback(/*rewind=*/false);
      pipeline->clear();

      // Order matters: restore views first so sinks can bind to them
      // during PipelineStateIO::load. The view-state application is
      // scheduled on Pipeline::executionFinished so the camera lands
      // after any resetCameraIfFirstSink pass runs.
      QMap<int, vtkSMViewProxy*> viewIdMap;
      pipeline::LegacyStateLoader::restoreViewsLayoutsAndPalette(
        object, pipeline, &viewIdMap);
      if (!pipeline::PipelineStateIO::load(pipeline, object, viewIdMap)) {
        return false;
      }
      bindFallbackView(pipeline);
      AnimationSerializer::restore(object, pipeline);
      finalizeNewFormatLoad(pipeline, executePipelines);
      return true;
    }
    return pipeline::LegacyStateLoader::load(
      object, QFileInfo(filename).dir(), executePipelines);
  }

  if (!legacyStateFile) {
    QMessageBox::warning(
      tomviz::mainWidget(), "Invalid state file",
      QString("Unable to read state file: %1").arg(error.errorString()));
  }

  return false;
}

bool SaveLoadStateReaction::saveState(const QString& fileName, bool interactive)
{
  if (fileName.endsWith(".tvsm")) {
    return saveTvsm(fileName, interactive);
  } else if (fileName.endsWith(".tvh5")) {
    return saveTvh5(fileName);
  }
  qCritical() << "Unknown format for saveState(): " << fileName;
  return false;
}

bool SaveLoadStateReaction::saveTvh5(const QString& fileName)
{
  auto* pip = ActiveObjects::instance().pipeline();
  if (!pip) {
    qWarning("No active pipeline to save.");
    return false;
  }
  QJsonObject extraState;
  ViewsLayoutsSerializer::saveActive(extraState);
  AnimationSerializer::save(extraState);
  return Tvh5Format::write(fileName.toStdString(), pip, extraState);
}

bool SaveLoadStateReaction::saveTvsm(const QString& fileName, bool /*interactive*/)
{
  QFile saveFile(fileName);
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  auto* pip = ActiveObjects::instance().pipeline();
  if (!pip) {
    qWarning("No active pipeline to save.");
    return false;
  }

  QJsonObject state;
  if (!pipeline::PipelineStateIO::save(pip, state)) {
    return false;
  }
  // PipelineStateIO leaves views/layouts/palette to the caller;
  // append them via the shared helper.
  ViewsLayoutsSerializer::saveActive(state);
  AnimationSerializer::save(state);

  QJsonDocument doc(state);
  return saveFile.write(doc.toJson()) != -1;
}

QString SaveLoadStateReaction::extractLegacyStateFileVersion(
  const QByteArray state)
{
  QString fullVersion;
  pugi::xml_document doc;

  if (doc.load_buffer(state.data(), state.size())) {
    pugi::xpath_node version = doc.select_node("/tomvizState/version");

    if (version) {
      fullVersion = version.node().attribute("full").value();
    }
  }

  return fullVersion;
}

bool SaveLoadStateReaction::automaticallyExecutePipelines()
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  QString key = "PipelineSettings.AutoExecuteOnStateLoad";
  if (settings->contains(key)) {
    return settings->value(key).toBool();
  }

  QDialog dialog(tomviz::mainWidget());
  dialog.setFixedWidth(300);
  dialog.setMaximumHeight(50);
  dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QVBoxLayout vLayout;
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Load state"));

  QFormLayout formLayout;
  vLayout.addLayout(&formLayout);

  QLabel title(tr("Automatically execute pipelines?"));
  formLayout.addRow(&title);

  QCheckBox dontAskAgain("Don't ask again");
  formLayout.addRow(&dontAskAgain);

  QDialogButtonBox buttons(QDialogButtonBox::Yes | QDialogButtonBox::No);
  vLayout.addWidget(&buttons);

  connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto r = dialog.exec();

  bool executePipelines = r == QDialog::Accepted;
  if (dontAskAgain.isChecked()) {
    settings->setValue(key, executePipelines);
  }

  return executePipelines;
}

bool SaveLoadStateReaction::checkForLegacyStateFileFormat(
  const QByteArray state)
{

  auto version = extractLegacyStateFileVersion(state);
  if (version.length() > 0) {
    QString url = "https://github.com/OpenChemistry/tomviz/releases";
    QString versionString = QString("Tomviz %1").arg(version);
    if (!version.contains("-g")) {
      url = QString("https://github.com/OpenChemistry/tomviz/releases/%1")
              .arg(version);
      versionString = QString("<a href=%1>Tomviz %2</a>").arg(url).arg(version);
    }

    QMessageBox versionWarning(tomviz::mainWidget());
    versionWarning.setIcon(QMessageBox::Icon::Warning);
    versionWarning.setTextFormat(Qt::RichText);
    versionWarning.setWindowTitle("Trying to load a legacy state file?");
    versionWarning.setText(
      QString("This state file was written using %1."
              " The format is not supported by the version of Tomviz you are "
              "running. "
              "A compatible version can be downloaded <a href=%2>here<a>")
        .arg(versionString)
        .arg(url));
    versionWarning.exec();
    return true;
  }

  return false;
}

} // namespace tomviz
