/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LegacyStateLoader.h"

#include "ActiveObjects.h"
#include "EmdFormat.h"
#include "LoadDataReaction.h"
#include "Utilities.h"

#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/Node.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SinkNode.h"
#include "pipeline/SourceNode.h"
#include "pipeline/TransformNode.h"
#include "pipeline/data/VolumeData.h"

#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/MoleculeSink.h"
#include "pipeline/sinks/OutlineSink.h"
#include "pipeline/sinks/PlotSink.h"
#include "pipeline/sinks/RulerSink.h"
#include "pipeline/sinks/ScaleCubeSink.h"
#include "pipeline/sinks/SegmentSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ThresholdSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include "pipeline/transforms/ArrayWranglerTransform.h"
#include "pipeline/transforms/ConvertToFloatTransform.h"
#include "pipeline/transforms/ConvertToVolumeTransform.h"
#include "pipeline/transforms/CropTransform.h"
#include "pipeline/transforms/LegacyPythonTransform.h"
#include "pipeline/transforms/ReconstructionTransform.h"
#include "pipeline/transforms/SetTiltAnglesTransform.h"
#include "pipeline/transforms/SnapshotTransform.h"
#include "pipeline/transforms/TranslateAlignTransform.h"
#include "pipeline/transforms/TransposeDataTransform.h"

#include <h5cpp/h5readwrite.h>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqDeleteReaction.h>
#include <pqServer.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPVXMLElement.h>
#include <vtkPVXMLParser.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMProxyLocator.h>
#include <vtkSMProxyManager.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkVector.h>
#include <vtkWeakPointer.h>
#include <vtk_pugixml.h>

#include <sstream>

#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>
#include <QVariant>
#include <QWidget>

namespace tomviz {
namespace pipeline {

namespace {

void addUniqueProxy(vtkSMProxy* proxy, const char* property, vtkSMProxy* value)
{
  vtkSMPropertyHelper helper(proxy, property);
  for (unsigned int i = 0; i < helper.GetNumberOfElements(); ++i) {
    if (helper.GetAsProxy(i) == value) {
      return;
    }
  }
  helper.Add(value);
}

// Re-do the part of vtkSMParaViewPipelineController::RegisterViewProxy()
// that pqApplicationCore::loadState() skips.
//
// Views created through pqObjectBuilder::createView() are handed to the
// controller, which adds them to the animation scene's "ViewModules" and
// the time keeper's "Views". Views recreated from state are registered
// straight with the proxy manager by vtkSMStateLoader, so neither list
// gets them, and restoreViewsAndLayouts() resets the session first, so
// both lists start out empty. vtkSMAnimationScene only updates and
// still-renders the views in "ViewModules", which means every animation
// (camera orbits, time series) ticks the cues but never redraws
// anything: the viewport just sits there.
void registerViewsWithSceneAndTimeKeeper(vtkSMSessionProxyManager* pxm)
{
  if (!pxm) {
    return;
  }

  vtkNew<vtkSMParaViewPipelineController> controller;
  auto* session = pxm->GetSession();
  auto* scene = controller->GetAnimationScene(session);
  auto* timeKeeper = controller->FindTimeKeeper(session);

  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(pxm);
  iter->SetModeToOneGroup();
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    auto* view = vtkSMViewProxy::SafeDownCast(iter->GetProxy());
    if (!view) {
      continue;
    }
    if (scene) {
      addUniqueProxy(scene, "ViewModules", view);
    }
    if (timeKeeper) {
      addUniqueProxy(timeKeeper, "Views", view);
    }
  }

  if (scene) {
    scene->UpdateVTKObjects();
  }
  if (timeKeeper) {
    timeKeeper->UpdateVTKObjects();
  }
}

// Unwrap the legacy module JSON — which nests per-module state under a
// "properties" sub-object and carries colormap + detached-flag keys at
// the module's top level — into the flat shape the new sinks expose via
// LegacyModuleSink::serialize() / deserialize().
//
// Legacy:
//   { "type", "id", "viewId",
//     "activeScalars", "useDetachedColorMap",
//     "colorOpacityMap"?, "gradientOpacityMap"?,
//     "properties": { "visibility": bool, ...per-module keys... } }
//
// Native (post-translation):
//   { "label"?, "visible"?, "activeScalars"?,
//     "useDetachedColorMap"?, "colorOpacityMap"?, "gradientOpacityMap"?,
//     ...per-module keys... }
//
// "visibility" is renamed to "visible"; the other keys already match
// (the new sinks were designed around this shape).
// Translate a legacy module JSON into the shape the corresponding sink
// expects. The sinks' native serialize()/deserialize() now use the same
// keys as legacy for per-module state, so this is just structural
// flattening plus two small value conversions:
//
//   - Legacy stored per-module state nested under "properties": {}; the
//     new sinks read a flat top-level. Copy properties out.
//   - Legacy called the visibility flag "visibility"; the new sinks use
//     "visible" (flattened alongside base-class state like label).
//   - Legacy stored SliceSink's "direction" as a string ("XY"/"YZ"/...);
//     the new sink reads it as an int matching the Direction enum.
QJsonObject legacyModuleToSinkJson(const QJsonObject& module)
{
  QJsonObject flat;
  for (const auto& key : { QStringLiteral("label"),
                           QStringLiteral("activeScalars"),
                           QStringLiteral("useDetachedColorMap"),
                           QStringLiteral("colorOpacityMap"),
                           QStringLiteral("gradientOpacityMap") }) {
    if (module.contains(key)) {
      flat[key] = module.value(key);
    }
  }
  auto props = module.value("properties").toObject();
  for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
    if (it.key() == QStringLiteral("visibility")) {
      flat["visible"] = it.value();
    } else {
      flat[it.key()] = it.value();
    }
  }

  if (flat.value("direction").isString()) {
    static const QMap<QString, int> directionToInt = {
      { QStringLiteral("XY"), 0 },
      { QStringLiteral("YZ"), 1 },
      { QStringLiteral("XZ"), 2 },
      { QStringLiteral("Custom"), 3 },
    };
    auto name = flat.value("direction").toString();
    if (directionToInt.contains(name)) {
      flat["direction"] = directionToInt.value(name);
    }
  }

  return flat;
}

// ParaView proxy-XML helpers. These mirror the ones in legacy
// ModuleManager.cxx (createXmlProperty / createXmlLayout) so the XML we
// hand to pqApplicationCore::loadState() matches the structure
// ParaView's state loader expects.
void xmlPropertyHeader(pugi::xml_node& n, const char* name, int id)
{
  n.set_name("Property");
  n.append_attribute("name").set_value(name);
  QString idStr = QString::number(id) + "." + name;
  n.append_attribute("id").set_value(idStr.toStdString().c_str());
}

template <typename T>
void xmlScalarProperty(pugi::xml_node& n, const char* name, int id, T value)
{
  xmlPropertyHeader(n, name, id);
  n.append_attribute("number_of_elements").set_value(1);
  auto element = n.append_child("Element");
  element.append_attribute("index").set_value(0);
  element.append_attribute("value").set_value(value);
}

void xmlArrayProperty(pugi::xml_node& n, const char* name, int id,
                      const QJsonArray& arr)
{
  xmlPropertyHeader(n, name, id);
  n.append_attribute("number_of_elements").set_value(arr.size());
  for (int i = 0; i < arr.size(); ++i) {
    auto element = n.append_child("Element");
    element.append_attribute("index").set_value(i);
    element.append_attribute("value").set_value(arr[i].toDouble(-1));
  }
}

void xmlLayoutItems(pugi::xml_node& n, const QJsonArray& items)
{
  n.set_name("Layout");
  n.append_attribute("number_of_elements").set_value(items.size());
  for (int i = 0; i < items.size(); ++i) {
    auto obj = items[i].toObject();
    auto item = n.append_child("Item");
    item.append_attribute("direction").set_value(obj["direction"].toInt());
    item.append_attribute("fraction").set_value(obj["fraction"].toDouble());
    item.append_attribute("view").set_value(obj["viewId"].toInt());
  }
}

// Pick the single view JSON to restore from a legacy state's "views"
// array. Prefers the one marked "active": true, falls back to the
// first entry. Returns an empty object if the array is missing/empty.
QJsonObject selectViewJson(const QJsonObject& state)
{
  auto views = state.value("views").toArray();
  for (const auto& v : views) {
    auto obj = v.toObject();
    if (obj.value("active").toBool()) {
      return obj;
    }
  }
  if (!views.isEmpty()) {
    return views.first().toObject();
  }
  return {};
}

// Resolve a DataSource reader filename relative to the state file directory.
QString resolveReaderFileName(const QJsonObject& reader, const QDir& stateDir)
{
  auto fileNames = reader.value("fileNames").toArray();
  if (fileNames.isEmpty()) {
    return {};
  }
  QString first = fileNames.at(0).toString();
  QFileInfo info(first);
  if (info.isAbsolute()) {
    return first;
  }
  return stateDir.absoluteFilePath(first);
}

} // namespace

void LegacyStateLoader::restoreViewsLayoutsAndPalette(
  const QJsonObject& state, Pipeline* pipeline,
  QMap<int, vtkSMViewProxy*>* viewIdMap)
{
  LoadContext ctx;
  ctx.pipeline = pipeline;
  if (state.contains("paletteColor")) {
    applyPaletteColor(state.value("paletteColor").toArray());
  }
  restoreViewsAndLayouts(state, ctx);
  scheduleViewStatesApply(state, ctx);
  if (viewIdMap) {
    *viewIdMap = ctx.viewIdMap;
  }
}

bool LegacyStateLoader::load(const QJsonObject& state, const QDir& stateDir,
                              bool executePipeline)
{
  LoadContext ctx;
  ctx.pipeline = ActiveObjects::instance().pipeline();
  ctx.stateDir = stateDir;
  if (!ctx.pipeline) {
    qWarning() << "LegacyStateLoader: no active pipeline to load into";
    return false;
  }

  ctx.pipeline->clear();

  if (state.contains("paletteColor")) {
    applyPaletteColor(state.value("paletteColor").toArray());
  }
  // Restore all views + the layout that hosts them via ParaView's
  // loadState. Populates ctx.viewIdMap so buildModule() can bind each
  // sink to the specific view its legacy module referenced. On failure
  // or if the state has no views[], we fall through to resolveView()
  // and use the app's current active view for every sink.
  restoreViewsAndLayouts(state, ctx);
  ctx.view = resolveView(ctx);
  scheduleViewStatesApply(state, ctx);

  if (!state.value("dataSources").isArray()) {
    qWarning() << "LegacyStateLoader: state has no dataSources array";
    return false;
  }

  for (const auto& dsVal : state.value("dataSources").toArray()) {
    auto ds = dsVal.toObject();
    auto* source = buildSource(ds, ctx);
    if (!source) {
      continue;
    }
    ctx.pipeline->addNode(source);
    applyDataSourceMetadata(source, ds);
    walkDataSource(source, ds, ctx);
  }

  // Warn the user about skipped operators/modules, if any.
  if (!ctx.skippedOperators.isEmpty() || !ctx.skippedModules.isEmpty()) {
    QStringList parts;
    if (!ctx.skippedOperators.isEmpty()) {
      parts << QObject::tr("Operators: %1")
                 .arg(ctx.skippedOperators.join(", "));
    }
    if (!ctx.skippedModules.isEmpty()) {
      parts << QObject::tr("Modules: %1")
                 .arg(ctx.skippedModules.join(", "));
    }
    qWarning().noquote() << "LegacyStateLoader: skipped unrecognized types —"
                         << parts.join("; ");
  }

  auto* pip = ctx.pipeline;
  if (executePipeline) {
    // Must clear any leftover paused state from a prior no-execute
    // load, otherwise Pipeline::execute() early-returns.
    pip->setPaused(false);
    // Defer execution so the event loop can process pending signals
    // first, matching LoadDataReaction::sourceNodeAdded.
    QTimer::singleShot(0, pip, [pip]() { pip->execute(); });
  } else {
    // User declined auto-execute. Pause the pipeline so parameter
    // changes or other interactions don't trigger execution until the
    // user explicitly unpauses (PipelineControlsWidget has a toggle).
    // Fire executionFinished once anyway so our deferred state-apply
    // handlers get to run (sink deserialize, view state, etc.). No
    // transforms run — their output ports stay empty until the user
    // triggers execution manually.
    pip->setPaused(true);
    QMetaObject::invokeMethod(pip, &Pipeline::executionFinished,
                              Qt::QueuedConnection);
  }

  return true;
}

bool LegacyStateLoader::loadFromH5(const QString& filename,
                                    bool executePipeline)
{
  using h5::H5ReadWrite;
  H5ReadWrite reader(filename.toStdString(), H5ReadWrite::OpenMode::ReadOnly);
  auto stateVec = reader.readData<char>("tomviz_state");
  QString stateStr = std::string(stateVec.begin(), stateVec.end()).c_str();
  auto doc = QJsonDocument::fromJson(stateStr.toUtf8());
  if (!doc.isObject()) {
    qWarning() << "LegacyStateLoader: failed to parse tomviz_state in"
               << filename;
    return false;
  }

  LoadContext ctx;
  ctx.pipeline = ActiveObjects::instance().pipeline();
  ctx.stateDir = QFileInfo(filename).dir();
  ctx.h5 = &reader;
  if (!ctx.pipeline) {
    qWarning() << "LegacyStateLoader: no active pipeline to load into";
    return false;
  }

  ctx.pipeline->clear();

  auto state = doc.object();
  if (state.contains("paletteColor")) {
    applyPaletteColor(state.value("paletteColor").toArray());
  }
  restoreViewsAndLayouts(state, ctx);
  ctx.view = resolveView(ctx);
  scheduleViewStatesApply(state, ctx);

  if (!state.value("dataSources").isArray()) {
    qWarning() << "LegacyStateLoader: state has no dataSources array";
    return false;
  }

  // Identify which DataSource id the /data/tomography soft-link points
  // at. Legacy writers link the active DataSource there to avoid storing
  // the data twice.
  for (const auto& dsVal : state.value("dataSources").toArray()) {
    auto ds = dsVal.toObject();
    if (ds.value("active").toBool()) {
      ctx.activeDataSourceId = ds.value("id").toString();
      break;
    }
  }

  for (const auto& dsVal : state.value("dataSources").toArray()) {
    auto ds = dsVal.toObject();
    auto* source = buildSource(ds, ctx);
    if (!source) {
      continue;
    }
    ctx.pipeline->addNode(source);
    applyDataSourceMetadata(source, ds);
    walkDataSource(source, ds, ctx);
  }

  auto* pip = ctx.pipeline;
  if (executePipeline) {
    // Must clear any leftover paused state from a prior no-execute
    // load, otherwise Pipeline::execute() early-returns.
    pip->setPaused(false);
    QTimer::singleShot(0, pip, [pip]() { pip->execute(); });
  } else {
    // User declined auto-execute. Pause the pipeline so parameter
    // changes or other interactions don't trigger execution until the
    // user explicitly unpauses (PipelineControlsWidget has a toggle).
    // Fire executionFinished once anyway so our deferred state-apply
    // handlers get to run (sink deserialize, view state, etc.).
    pip->setPaused(true);
    QMetaObject::invokeMethod(pip, &Pipeline::executionFinished,
                              Qt::QueuedConnection);
  }
  return true;
}

SourceNode* LegacyStateLoader::buildSource(const QJsonObject& dsJson,
                                           LoadContext& ctx)
{
  // .tvh5 path: read voxel data straight out of the HDF5 container, skipping
  // any on-disk reader lookup.
  if (ctx.h5) {
    auto id = dsJson.value("id").toString();
    if (id.isEmpty()) {
      qWarning() << "LegacyStateLoader: dataSource has no id (tvh5)";
      return nullptr;
    }
    std::string path = "/tomviz_datasources/" + id.toStdString();
    vtkNew<vtkImageData> image;
    QVariantMap options = { { "askForSubsample", false } };
    if (!EmdFormat::readNode(*ctx.h5, path, image, options)) {
      qWarning() << "LegacyStateLoader: failed to read tvh5 node" << path.c_str();
      return nullptr;
    }
    auto* source = new SourceNode();
    QString label = dsJson.value("label").toString();
    if (label.isEmpty()) {
      label = QStringLiteral("Loaded Data");
    }
    source->setLabel(label);

    vtkSmartPointer<vtkImageData> img = image.GetPointer();
    const bool isTiltSeries = VolumeData::hasTiltAngles(img);
    const PortType declaredType =
      isTiltSeries ? PortType::TiltSeries : PortType::ImageData;

    source->addOutput("volume", declaredType);
    auto vol = std::make_shared<VolumeData>(img);
    vol->setLabel(label);
    source->setOutputData("volume", PortData(vol, declaredType));
    return source;
  }

  // .tvsm path: delegate to LoadDataReaction so we get the correct reader
  // for the file extension. We disable default modules / auto-add to
  // pipeline because the loader controls node and link construction.
  auto reader = dsJson.value("reader").toObject();
  QString fileName = resolveReaderFileName(reader, ctx.stateDir);
  if (fileName.isEmpty()) {
    qWarning() << "LegacyStateLoader: dataSource has no reader.fileNames";
    return nullptr;
  }

  QJsonObject opts;
  opts["defaultModules"] = false;
  opts["addToPipeline"] = false;
  opts["addToRecent"] = false;

  // Forward reader-specific options (e.g. subsampleSettings) through.
  if (reader.contains("subsampleSettings")) {
    opts["subsampleSettings"] = reader.value("subsampleSettings");
  }
  if (reader.contains("name")) {
    QJsonObject readerBlock = reader;
    opts["reader"] = readerBlock;
  }

  auto* source = LoadDataReaction::loadData(fileName, opts);
  if (!source) {
    qWarning() << "LegacyStateLoader: failed to load source file" << fileName;
    return nullptr;
  }
  if (dsJson.contains("label")) {
    source->setLabel(dsJson.value("label").toString());
  }
  return source;
}

void LegacyStateLoader::applyDataSourceMetadata(Node* node,
                                                 const QJsonObject& dsJson)
{
  if (!node || node->outputPorts().isEmpty()) {
    return;
  }
  auto* outPort = node->outputPorts().first();
  if (!outPort || !outPort->hasData()) {
    return;
  }
  auto portData = outPort->data();
  if (!portData.isValid() || !isVolumeType(portData.type())) {
    return;
  }
  auto vol = portData.value<VolumeDataPtr>();
  if (!vol) {
    return;
  }

  // The subset of legacy DataSource keys that map onto VolumeData
  // (label, units, spacing, origin, orientation, colorOpacityMap,
  // gradientOpacityMap) already uses the native VolumeData shape, so we
  // pass the whole DataSource JSON through — VolumeData::deserialize
  // ignores keys it doesn't own (operators, modules, reader, ...).
  vol->deserialize(dsJson);
  // TODO: activeScalars by name (legacy stored the scalar array name)
  // and colorMap2DBox.
}

void LegacyStateLoader::walkDataSource(Node* upstream,
                                        const QJsonObject& ds,
                                        LoadContext& ctx)
{
  if (!upstream || upstream->outputPorts().isEmpty()) {
    return;
  }

  // Walk the operator chain first. Each operator becomes a TransformNode
  // linked to the previous node's primary output port.
  Node* current = upstream;
  auto operators = ds.value("operators").toArray();
  for (const auto& opVal : operators) {
    auto op = opVal.toObject();
    auto* transform = buildOperator(op, ctx);
    if (!transform) {
      continue;
    }
    ctx.pipeline->addNode(transform);
    if (!current->outputPorts().isEmpty() &&
        !transform->inputPorts().isEmpty()) {
      ctx.pipeline->createLink(current->outputPorts().first(),
                               transform->inputPorts().first());
    }

    // Legacy child DataSource — its operators/modules attach to the
    // transform's primary output port, not to a new SourceNode.
    auto childArray = op.value("dataSources").toArray();
    for (const auto& childVal : childArray) {
      auto child = childVal.toObject();
      walkDataSource(transform, child, ctx);

      // DataSource-level state (colormap, spacing, units, ...) on a
      // child applies to the VolumeData produced by the parent
      // transform, which doesn't exist until the pipeline executes.
      // Defer to executionFinished. MainWindow's handler on the same
      // signal initializes colormaps via copyColorMapFrom(upstream)
      // and is connected first, so it runs first; our handler runs
      // after and overwrites the copied colormap with the saved one.
      // We then re-run updateColorMap on sinks so they pick up the
      // restored control points before the next paint.
      //
      // We cannot use Qt::SingleShotConnection here: in no-execute
      // mode the first executionFinished is our synthetic fire, the
      // transform's output port has no data yet, and a single-shot
      // slot would detach before the user actually runs the pipeline.
      // Self-disconnect only once the transform has produced data.
      auto* pipeline = ctx.pipeline;
      auto conn = std::make_shared<QMetaObject::Connection>();
      *conn = QObject::connect(
        pipeline, &Pipeline::executionFinished, pipeline,
        [pipeline, transform, child, conn]() {
          if (transform->outputPorts().isEmpty() ||
              !transform->outputPorts().first()->hasData()) {
            return; // no execute has happened yet; wait
          }
          applyDataSourceMetadata(transform, child);
          for (auto* n : pipeline->nodes()) {
            auto* sink = dynamic_cast<LegacyModuleSink*>(n);
            if (sink && sink->isColorMapNeeded()) {
              sink->updateColorMap();
            }
          }
          QObject::disconnect(*conn);
        });
    }

    current = transform;
  }

  // Attach modules as sinks to the last node in the chain.
  auto modules = ds.value("modules").toArray();
  if (modules.isEmpty() || current->outputPorts().isEmpty()) {
    return;
  }

  auto* sourcePort = current->outputPorts().first();

  // All modules from the same DataSource share one SinkGroupNode so
  // intermediate data is not duplicated — matches the default sink-setup
  // pattern in LoadDataReaction::sourceNodeAdded.
  auto* group = new SinkGroupNode();
  group->addPassthrough("volume", PortType::ImageData);
  ctx.pipeline->addNode(group);
  ctx.pipeline->createLink(sourcePort, group->inputPorts().first());
  auto* groupOut = group->outputPorts().first();

  for (const auto& modVal : modules) {
    buildModule(modVal.toObject(), groupOut, group, ctx);
  }
}

void LegacyStateLoader::loadTemplateOperators(const QJsonArray& operators,
                                               Pipeline* pipeline)
{
  if (!pipeline) {
    return;
  }
  LoadContext ctx;
  ctx.pipeline = pipeline;
  TransformNode* prev = nullptr;
  for (const auto& opVal : operators) {
    auto op = opVal.toObject();
    auto* transform = buildOperator(op, ctx);
    if (!transform) {
      continue;
    }
    pipeline->addNode(transform);
    if (prev && !prev->outputPorts().isEmpty() &&
        !transform->inputPorts().isEmpty()) {
      pipeline->createLink(prev->outputPorts().first(),
                           transform->inputPorts().first());
    }
    // Parameters come from the template, so the node isn't "New" in
    // the sense the link-completion auto-edit-dialog cares about.
    transform->setStateNoCascade(NodeState::Stale);
    prev = transform;
  }
}

TransformNode* LegacyStateLoader::buildOperator(const QJsonObject& op,
                                                 LoadContext& ctx)
{
  const QString type = op.value("type").toString();

  TransformNode* transform = nullptr;
  if (type == QStringLiteral("Python")) {
    transform = new LegacyPythonTransform();
  } else if (type == QStringLiteral("ArrayWrangler")) {
    transform = new ArrayWranglerTransform();
  } else if (type == QStringLiteral("ConvertToFloat")) {
    transform = new ConvertToFloatTransform();
  } else if (type == QStringLiteral("ConvertToVolume")) {
    transform = new ConvertToVolumeTransform();
  } else if (type == QStringLiteral("Crop")) {
    transform = new CropTransform();
  } else if (type == QStringLiteral("CxxReconstruction")) {
    transform = new ReconstructionTransform();
  } else if (type == QStringLiteral("SetTiltAngles")) {
    transform = new SetTiltAnglesTransform();
  } else if (type == QStringLiteral("TranslateAlign")) {
    transform = new TranslateAlignTransform();
  } else if (type == QStringLiteral("TransposeData")) {
    transform = new TransposeDataTransform();
  } else if (type == QStringLiteral("Snapshot")) {
    transform = new SnapshotTransform();
  } else {
    if (!type.isEmpty()) {
      ctx.skippedOperators.append(type);
    }
    return nullptr;
  }

  if (transform) {
    transform->deserialize(op);
  }
  return transform;
}

LegacyModuleSink* LegacyStateLoader::buildModule(const QJsonObject& module,
                                                   OutputPort* sourcePort,
                                                   SinkGroupNode* group,
                                                   LoadContext& ctx)
{
  const QString type = module.value("type").toString();

  LegacyModuleSink* sink = nullptr;
  if (type == QStringLiteral("Outline")) {
    sink = new OutlineSink();
  } else if (type == QStringLiteral("Volume")) {
    sink = new VolumeSink();
  } else if (type == QStringLiteral("Contour")) {
    sink = new ContourSink();
  } else if (type == QStringLiteral("Slice") ||
             type == QStringLiteral("OrthogonalSlice")) {
    // OrthogonalSlice is handled by the same sink; ortho-specific behavior
    // (if any) is expected to be encoded in the module's "properties".
    sink = new SliceSink();
  } else if (type == QStringLiteral("Clip")) {
    sink = new ClipSink();
  } else if (type == QStringLiteral("Threshold")) {
    sink = new ThresholdSink();
  } else if (type == QStringLiteral("Segment")) {
    sink = new SegmentSink();
  } else if (type == QStringLiteral("Molecule")) {
    sink = new MoleculeSink();
  } else if (type == QStringLiteral("Plot")) {
    sink = new PlotSink();
  } else if (type == QStringLiteral("Ruler")) {
    sink = new RulerSink();
  } else if (type == QStringLiteral("ScaleCube")) {
    sink = new ScaleCubeSink();
  } else {
    if (!type.isEmpty()) {
      ctx.skippedModules.append(type);
    }
    return nullptr;
  }

  // Prefer the view that matches the module's saved viewId; fall back
  // to the app's current active view when the id isn't in the map
  // (which happens when restoreViewsAndLayouts wasn't invoked or the
  // module targets a view type we don't display).
  vtkSMViewProxy* targetView = ctx.view;
  if (module.contains("viewId")) {
    int vid = module.value("viewId").toInt();
    if (auto* mapped = ctx.viewIdMap.value(vid, nullptr)) {
      targetView = mapped;
    }
  }
  if (targetView) {
    sink->initialize(targetView);
  }

  ctx.pipeline->addNode(sink);
  if (sourcePort && !sink->inputPorts().isEmpty()) {
    ctx.pipeline->createLink(sourcePort, sink->inputPorts().first());
  }

  // Defer deserialization of sink state until the pipeline has produced
  // data for the first time. If we apply it now, calls like
  // setVisibility(true) can flip the VTK actor visibility on before the
  // upstream filter has any input (OutlineSink::consume is what calls
  // SetInputData on the filter), producing "Input port 0 ... has 0
  // connections" warnings on each paint until consume runs.
  //
  // Extra wrinkle: when the user loads a state file with "don't auto-
  // execute" (or the sink's upstream is a transform that didn't run),
  // consume never happens and applying visible=true here is similarly
  // premature. So we split the apply:
  //   - All non-visibility state gets applied as soon as this handler
  //     fires (on the first executionFinished), even if the sink's
  //     state isn't Current — properties like detached colormap,
  //     activeScalars, blendingMode, etc. are safe before consume.
  //   - The saved visibility is only applied once the sink actually
  //     reaches NodeState::Current (i.e. consume ran successfully),
  //     which may be this first firing (normal execute) or some later
  //     firing (user manually runs the pipeline).
  auto* pip = ctx.pipeline;
  QJsonObject sinkJson = legacyModuleToSinkJson(module);
  QObject::connect(
    pip, &Pipeline::executionFinished, sink,
    [sink, sinkJson, pip]() {
      const bool targetVis = sinkJson.value("visible").toBool(true);
      const bool sinkIsCurrent = (sink->state() == NodeState::Current);
      if (sinkIsCurrent || !targetVis) {
        sink->deserialize(sinkJson);
        return;
      }
      // Apply everything except visible=true; hook the visibility
      // flip to the next successful execution.
      QJsonObject later = sinkJson;
      later["visible"] = false;
      sink->deserialize(later);
      auto conn = std::make_shared<QMetaObject::Connection>();
      *conn = QObject::connect(
        pip, &Pipeline::executionFinished, sink,
        [sink, targetVis, conn]() {
          if (sink->state() == NodeState::Current) {
            sink->setVisibility(targetVis);
            QObject::disconnect(*conn);
          }
        });
    },
    Qt::SingleShotConnection);

  Q_UNUSED(group);
  return sink;
}

vtkSMViewProxy* LegacyStateLoader::resolveView(LoadContext& ctx)
{
  Q_UNUSED(ctx);
  ActiveObjects::instance().createRenderViewIfNeeded();
  auto* view = ActiveObjects::instance().activeView();
  if (!view || QString(view->GetXMLName()) != QStringLiteral("RenderView")) {
    ActiveObjects::instance().setActiveViewToFirstRenderView();
    view = ActiveObjects::instance().activeView();
  }
  return view;
}

void LegacyStateLoader::applyPaletteColor(const QJsonArray& color)
{
  if (color.size() != 3) {
    return;
  }
  auto* pxm = ActiveObjects::instance().proxyManager();
  if (!pxm) {
    return;
  }
  auto* palette = pxm->GetProxy("settings", "ColorPalette");
  if (!palette) {
    return;
  }
  double rgb[3] = { color.at(0).toDouble(), color.at(1).toDouble(),
                    color.at(2).toDouble() };
  vtkSMPropertyHelper(palette, "BackgroundColor").Set(rgb, 3);
  palette->UpdateVTKObjects();
}

void LegacyStateLoader::applyViewState(vtkSMViewProxy* view,
                                        const QJsonObject& viewJson,
                                        bool cameraToo)
{
  if (!view || viewJson.isEmpty()) {
    return;
  }

  // Interaction + projection modes. Apply before camera so the view's
  // constraints (e.g. 2D locks CameraParallelProjection) don't override
  // the camera state we set next.
  if (viewJson.contains("interactionMode")) {
    auto mode = viewJson.value("interactionMode").toString();
    int modeInt = 0; // "3D"
    if (mode == QStringLiteral("2D")) {
      modeInt = 1;
    } else if (mode == QStringLiteral("selection")) {
      modeInt = 2;
    }
    vtkSMPropertyHelper(view, "InteractionMode").Set(modeInt);
  }
  if (viewJson.contains("isOrthographic")) {
    vtkSMPropertyHelper(view, "CameraParallelProjection")
      .Set(viewJson.value("isOrthographic").toBool() ? 1 : 0);
  }

  // Background. Legacy serialized a single solid color as a 1-element
  // array-of-arrays (or 2 elements for Gradient mode). For a first pass
  // we just restore the primary Background; gradient mode is rare.
  if (viewJson.contains("backgroundColor")) {
    auto outer = viewJson.value("backgroundColor").toArray();
    if (outer.size() >= 1 && outer.at(0).isArray()) {
      auto bg = outer.at(0).toArray();
      if (bg.size() == 3) {
        double rgb[3] = { bg.at(0).toDouble(), bg.at(1).toDouble(),
                          bg.at(2).toDouble() };
        vtkSMPropertyHelper(view, "Background").Set(rgb, 3);
      }
    }
  }
  if (viewJson.contains("useColorPaletteForBackground") &&
      view->GetProperty("UseColorPaletteForBackground")) {
    vtkSMPropertyHelper(view, "UseColorPaletteForBackground")
      .Set(viewJson.value("useColorPaletteForBackground").toInt());
  }

  // Axes.
  if (viewJson.contains("centerAxesVisible")) {
    vtkSMPropertyHelper(view, "CenterAxesVisibility")
      .Set(viewJson.value("centerAxesVisible").toBool() ? 1 : 0);
  }
  if (viewJson.contains("orientationAxesVisible")) {
    vtkSMPropertyHelper(view, "OrientationAxesVisibility")
      .Set(viewJson.value("orientationAxesVisible").toBool() ? 1 : 0);
  }
  if (view->GetProperty("AxesGrid") &&
      viewJson.contains("axesGridVisibility")) {
    vtkSMPropertyHelper axesGridProp(view, "AxesGrid");
    vtkSMProxy* axesGrid = axesGridProp.GetAsProxy();
    if (!axesGrid) {
      auto* pxm = view->GetSessionProxyManager();
      axesGrid = pxm->NewProxy("annotations", "GridAxes3DActor");
      axesGridProp.Set(axesGrid);
      axesGrid->Delete();
    }
    vtkSMPropertyHelper(axesGrid, "Visibility")
      .Set(viewJson.value("axesGridVisibility").toBool() ? 1 : 0);
    axesGrid->UpdateVTKObjects();
  }

  if (viewJson.contains("centerOfRotation")) {
    auto arr = viewJson.value("centerOfRotation").toArray();
    if (arr.size() == 3) {
      double c[3] = { arr.at(0).toDouble(), arr.at(1).toDouble(),
                      arr.at(2).toDouble() };
      vtkSMPropertyHelper(view, "CenterOfRotation").Set(c, 3);
    }
  }

  // Camera. Applied last so view-mode constraints set above have
  // already been committed. Skipped when cameraToo is false — used by
  // the two-phase restore where non-camera state (background / axes /
  // interaction) lands immediately and the camera is deferred until
  // after a real execute so it isn't overwritten by
  // LegacyModuleSink::resetCameraIfFirstSink.
  if (cameraToo) {
    auto camera = viewJson.value("camera").toObject();
    auto setVec3 = [&](const char* key, const char* prop) {
      if (!camera.contains(key)) {
        return;
      }
      auto arr = camera.value(key).toArray();
      if (arr.size() == 3) {
        double v[3] = { arr.at(0).toDouble(), arr.at(1).toDouble(),
                        arr.at(2).toDouble() };
        vtkSMPropertyHelper(view, prop).Set(v, 3);
      }
    };
    setVec3("position", "CameraPosition");
    setVec3("focalPoint", "CameraFocalPoint");
    setVec3("viewUp", "CameraViewUp");
    if (camera.contains("viewAngle")) {
      vtkSMPropertyHelper(view, "CameraViewAngle")
        .Set(camera.value("viewAngle").toDouble());
    }
    if (camera.contains("eyeAngle") && view->GetProperty("EyeAngle")) {
      vtkSMPropertyHelper(view, "EyeAngle")
        .Set(camera.value("eyeAngle").toDouble());
    }
    if (camera.contains("parallelScale")) {
      vtkSMPropertyHelper(view, "CameraParallelScale")
        .Set(camera.value("parallelScale").toDouble());
    }
  }

  view->UpdateVTKObjects();
  if (auto* renderView = vtkSMRenderViewProxy::SafeDownCast(view)) {
    renderView->StillRender();
  }
}

void LegacyStateLoader::scheduleViewStateApply(const QJsonObject& state,
                                                vtkSMViewProxy* view,
                                                Pipeline* pipeline)
{
  QJsonObject viewJson = selectViewJson(state);
  if (viewJson.isEmpty() || !view || !pipeline) {
    return;
  }
  // Weak pointer: the view may be destroyed before this fires (see the
  // note in scheduleViewStatesApply); applyViewState() no-ops on null.
  vtkWeakPointer<vtkSMViewProxy> weakView(view);
  QObject::connect(
    pipeline, &Pipeline::executionFinished, pipeline,
    [weakView, viewJson]() { applyViewState(weakView, viewJson); },
    Qt::SingleShotConnection);
}

void LegacyStateLoader::scheduleViewStatesApply(const QJsonObject& state,
                                                 LoadContext& ctx)
{
  auto* pipeline = ctx.pipeline;
  if (!pipeline) {
    return;
  }

  // Phase 1 (now): apply non-camera view state (background / axes /
  // interaction mode / orthographic / centerOfRotation) immediately so
  // the scene looks right as soon as the state file loads, even if the
  // user declined the auto-execute prompt.
  //
  // Phase 2 (deferred): apply camera state after the first real
  // executionFinished. LegacyModuleSink::resetCameraIfFirstSink queues
  // a ResetCamera on the sink's main-thread on first consume; that
  // queued call runs before the cross-thread executionFinished slot
  // fires in auto-execute mode (so our saved camera wins), but in
  // no-execute mode the synthetic executionFinished we emit fires
  // before any sink consumes — so single-shot-apply-now would be
  // overwritten by the reset the next time the user actually runs the
  // pipeline. Using a self-disconnecting connection guarded by "any
  // node has reached NodeState::Current" handles both cases: it skips
  // the synthetic fire in no-execute mode and lands on the first real
  // execute, then detaches so the user's subsequent camera edits
  // survive re-executions.
  auto schedule = [pipeline](vtkSMViewProxy* view,
                             const QJsonObject& viewJson) {
    if (!view) {
      return;
    }
    applyViewState(view, viewJson, /*cameraToo=*/false);

    // Capture the view as a weak pointer. This connection is bound to
    // the long-lived `pipeline` (ResetReaction only calls
    // Pipeline::clear(), so the Pipeline survives a reset / re-load),
    // but the view proxy is destroyed by pqDeleteReaction::deleteAll()
    // in the next restoreViewsAndLayouts(). If a prior load left this
    // connection pending — its pipeline never reached "any sink
    // Current", so it never self-disconnected — a later execute would
    // otherwise fire applyViewState() on a freed view and crash.
    vtkWeakPointer<vtkSMViewProxy> weakView(view);
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = QObject::connect(
      pipeline, &Pipeline::executionFinished, pipeline,
      [weakView, viewJson, pipeline, conn]() {
        if (!weakView) {
          // View was torn down by a reset / subsequent load. Drop the
          // stale connection so it stops firing.
          QObject::disconnect(*conn);
          return;
        }
        // A SinkNode only reaches NodeState::Current after consume()
        // succeeded — so "any sink Current" reliably discriminates
        // the real execute from the synthetic executionFinished we
        // emit in no-execute mode. The source node is Current as
        // soon as LoadDataReaction::loadData returns, so checking
        // all nodes would false-positive on the synthetic fire.
        bool anySinkCurrent = false;
        for (auto* node : pipeline->nodes()) {
          if (dynamic_cast<SinkNode*>(node) &&
              node->state() == NodeState::Current) {
            anySinkCurrent = true;
            break;
          }
        }
        if (!anySinkCurrent) {
          return; // no sink has consumed yet; wait for a real execute
        }
        // Defer the camera apply by one event-loop tick. Two reasons:
        //  - LegacyModuleSink::resetCameraIfFirstSink queues its
        //    ResetCamera via QMetaObject::invokeMethod from the worker
        //    thread; most of those run before this signal reaches us,
        //    but a late one (e.g. from a sink that consumes near the
        //    end of the run) can land after the signal cascade and
        //    overwrite a saved camera we just restored.
        //  - ParaView sometimes does a deferred clipping-range /
        //    first-render adjust on a freshly-created view; letting
        //    that settle before we push our camera prevents it from
        //    clipping our saved position.
        QTimer::singleShot(0, pipeline, [weakView, viewJson, conn]() {
          applyViewState(weakView, viewJson, /*cameraToo=*/true);
          QObject::disconnect(*conn);
        });
      });
  };

  if (ctx.viewIdMap.isEmpty()) {
    QJsonObject viewJson = selectViewJson(state);
    if (!viewJson.isEmpty()) {
      schedule(ctx.view, viewJson);
    }
    return;
  }

  auto views = state.value("views").toArray();
  const auto idMap = ctx.viewIdMap;
  for (const auto& v : views) {
    auto view = v.toObject();
    int id = view.value("id").toInt();
    auto it = idMap.find(id);
    if (it == idMap.end()) {
      continue;
    }
    schedule(it.value(), view);
  }
}

bool LegacyStateLoader::restoreViewsAndLayouts(const QJsonObject& state,
                                                LoadContext& ctx)
{
  auto views = state.value("views").toArray();
  if (views.isEmpty()) {
    return false;
  }
  auto layouts = state.value("layouts").toArray();

  // Build ParaView proxy-state XML. This mirrors legacy
  // ModuleManager::deserialize. The shapes must match what ParaView's
  // state loader expects: a <ServerManagerState> root with
  // <ProxyCollection name="views"|"layouts"> summaries plus one
  // <Proxy group=... id=... servers=...> per view/layout with nested
  // <Property> / <Layout> children.
  pugi::xml_document document;
  auto root = document.append_child("ParaView");
  auto pvState = root.append_child("ServerManagerState");
  pvState.append_attribute("version").set_value("5.5.0");
  auto viewCollection = pvState.append_child("ProxyCollection");
  viewCollection.append_attribute("name").set_value("views");
  auto layoutCollection = pvState.append_child("ProxyCollection");
  layoutCollection.append_attribute("name").set_value("layouts");

  int numViews = 0;
  int numLayouts = 0;
  for (const auto& v : views) {
    auto view = v.toObject();
    int viewId = view.value("id").toInt();
    auto xmlName = view.value("xmlName").toString("RenderView");

    auto proxyNode = pvState.append_child("Proxy");
    proxyNode.append_attribute("group").set_value("views");
    proxyNode.append_attribute("type").set_value(xmlName.toStdString().c_str());
    proxyNode.append_attribute("id").set_value(viewId);
    proxyNode.append_attribute("servers").set_value(
      view.value("servers").toInt());

    if (view.contains("centerOfRotation")) {
      auto p = proxyNode.append_child("Property");
      xmlArrayProperty(p, "CenterOfRotation", viewId,
                       view.value("centerOfRotation").toArray());
    }
    auto camera = view.value("camera").toObject();
    if (camera.contains("focalPoint")) {
      auto p = proxyNode.append_child("Property");
      xmlArrayProperty(p, "CameraFocalPoint", viewId,
                       camera.value("focalPoint").toArray());
    }
    if (view.contains("useColorPaletteForBackground")) {
      auto p = proxyNode.append_child("Property");
      xmlScalarProperty(p, "UseColorPaletteForBackground", viewId,
                        view.value("useColorPaletteForBackground").toInt());
    }
    if (view.contains("backgroundColor")) {
      auto backgroundColor = view.value("backgroundColor").toArray();
      if (!view.contains("useColorPaletteForBackground")) {
        // Older state files: turn palette off when an explicit
        // background was stored.
        auto p = proxyNode.append_child("Property");
        xmlScalarProperty(p, "UseColorPaletteForBackground", viewId, 0);
      }
      if (!backgroundColor.isEmpty() && backgroundColor.at(0).isArray()) {
        auto p = proxyNode.append_child("Property");
        xmlArrayProperty(p, "Background", viewId,
                         backgroundColor.at(0).toArray());
      }
      if (backgroundColor.size() > 1 && backgroundColor.at(1).isArray()) {
        auto p = proxyNode.append_child("Property");
        xmlArrayProperty(p, "Background2", viewId,
                         backgroundColor.at(1).toArray());
        auto g = proxyNode.append_child("Property");
        xmlScalarProperty(g, "UseGradientBackground", viewId, 1);
      }
    }
    if (view.contains("isOrthographic")) {
      auto p = proxyNode.append_child("Property");
      xmlScalarProperty(p, "CameraParallelProjection", viewId,
                        view.value("isOrthographic").toBool() ? 1 : 0);
    }
    if (view.contains("interactionMode")) {
      auto mode = view.value("interactionMode").toString();
      int m = 0;
      if (mode == QStringLiteral("2D")) {
        m = 1;
      } else if (mode == QStringLiteral("selection")) {
        m = 2;
      }
      auto p = proxyNode.append_child("Property");
      xmlScalarProperty(p, "InteractionMode", viewId, m);
    }

    auto summary = viewCollection.append_child("Item");
    summary.append_attribute("id").set_value(viewId);
    summary.append_attribute("name").set_value(
      QString("View%1").arg(++numViews).toStdString().c_str());
  }

  for (const auto& l : layouts) {
    auto layout = l.toObject();
    int layoutId = layout.value("id").toInt();
    auto proxyNode = pvState.append_child("Proxy");
    proxyNode.append_attribute("group").set_value("misc");
    proxyNode.append_attribute("type").set_value("ViewLayout");
    proxyNode.append_attribute("id").set_value(layoutId);
    proxyNode.append_attribute("servers").set_value(
      layout.value("servers").toInt());

    auto items = layout.value("items").toArray();
    for (const auto& row : items) {
      auto layoutNode = proxyNode.append_child("Layout");
      xmlLayoutItems(layoutNode, row.toArray());
    }

    auto summary = layoutCollection.append_child("Item");
    summary.append_attribute("id").set_value(layoutId);
    summary.append_attribute("name").set_value(
      QString("Layout%1").arg(++numLayouts).toStdString().c_str());
  }

  std::ostringstream stream;
  document.first_child().print(stream);

  vtkNew<vtkPVXMLParser> parser;
  if (!parser->Parse(stream.str().c_str())) {
    qWarning() << "LegacyStateLoader: failed to parse synthesized view "
                  "state XML";
    return false;
  }

  // Clear the existing views/layouts so loadState can recreate them
  // with the legacy global IDs. pqDeleteReaction::deleteAll handles
  // views, layouts, and any other proxies the UI knows about.
  pqDeleteReaction::deleteAll();

  // Capture the ProxyLocator that loadState uses to resolve the
  // just-created proxies. The signal fires synchronously during
  // loadState; auto-disconnect via SingleShotConnection so we don't
  // leak the connection across subsequent state loads.
  auto* appCore = pqApplicationCore::instance();
  auto& idMap = ctx.viewIdMap;
  QObject::connect(
    appCore, &pqApplicationCore::stateLoaded, appCore,
    [&idMap, &views](vtkPVXMLElement*, vtkSMProxyLocator* locator) {
      if (!locator) {
        return;
      }
      for (const auto& v : views) {
        auto view = v.toObject();
        int viewId = view.value("id").toInt();
        if (auto* proxy = vtkSMViewProxy::SafeDownCast(
              locator->LocateProxy(viewId))) {
          idMap.insert(viewId, proxy);
        }
      }
    },
    Qt::SingleShotConnection);

  auto* server = pqActiveObjects::instance().activeServer();
  appCore->loadState(parser->GetRootElement(), server);

  registerViewsWithSceneAndTimeKeeper(server ? server->proxyManager()
                                             : nullptr);

  // The animation scene above is a brand new one, back on ParaView's
  // 10 frame default. Re-apply the frame count tomviz uses for freshly
  // loaded data, otherwise a camera orbit created after a state load is
  // over in a blink and looks like it did nothing.
  tomviz::setAnimationNumberOfFrames(200);

  // Make the "active" view (if any) the app's active view too.
  for (const auto& v : views) {
    auto view = v.toObject();
    if (view.value("active").toBool()) {
      if (auto* proxy = idMap.value(view.value("id").toInt(), nullptr)) {
        ActiveObjects::instance().setActiveView(proxy);
      }
      break;
    }
  }
  return !idMap.isEmpty();
}

} // namespace pipeline
} // namespace tomviz
