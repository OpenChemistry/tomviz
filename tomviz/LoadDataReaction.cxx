/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LoadDataReaction.h"

#include "ActiveObjects.h"
#include "DataExchangeFormat.h"
#include "EmdFormat.h"
#include "FileFormatManager.h"
#include "FileReader.h"
#include "FxiFormat.h"
#include "GenericHDF5Format.h"
#include "ImageStackDialog.h"
#include "ImageStackModel.h"
#include "LoadStackReaction.h"
#include "PythonReader.h"
#include "PythonUtilities.h"
#include "RAWFileReaderDialog.h"
#include "RecentFilesMenu.h"
#include "Utilities.h"
#include "vtkOMETiffReader.h"

#include "animations/TimeSeriesAnimation.h"

#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/PortUtils.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SourceNode.h"
#include "pipeline/sources/ReaderSourceNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/OutlineSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/VolumeSink.h"
#include "ColorMap.h"

#include <pqActiveObjects.h>
#include <pqLoadDataReaction.h>
#include <pqPipelineSource.h>
#include <pqProxyWidgetDialog.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>
#include <pqView.h>
#include <vtkSMCoreUtilities.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMStringVectorProperty.h>
#include <vtkSMViewProxy.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTIFFReader.h>
#include <vtkTrivialProducer.h>

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QMessageBox>
#include <QTimer>

#include <sstream>

namespace {
bool hasData(vtkSMProxy* reader)
{
  vtkSMSourceProxy* dataSource = vtkSMSourceProxy::SafeDownCast(reader);
  if (!dataSource) {
    return false;
  }

  dataSource->UpdatePipeline();
  vtkAlgorithm* vtkalgorithm =
    vtkAlgorithm::SafeDownCast(dataSource->GetClientSideObject());
  if (!vtkalgorithm) {
    return false;
  }

  // Create a clone and release the reader data.
  vtkImageData* data =
    vtkImageData::SafeDownCast(vtkalgorithm->GetOutputDataObject(0));
  if (!data) {
    return false;
  }

  int extent[6];
  data->GetExtent(extent);
  if (extent[0] > extent[1] || extent[2] > extent[3] ||
      extent[4] > extent[5]) {
    return false;
  }

  vtkPointData* pd = data->GetPointData();
  if (!pd) {
    return false;
  }

  if (pd->GetNumberOfArrays() < 1) {
    return false;
  }
  return true;
}

void applyDefaultColorMapPreset(tomviz::pipeline::OutputPort* port)
{
  if (!port) {
    return;
  }
  auto vol = tomviz::pipeline::getOutputData<tomviz::pipeline::VolumeDataPtr>(
    port->node(), port->name());
  if (!vol) {
    return;
  }
  tomviz::ColorMap::instance().applyPreset(vol->colorMap());
  vol->rescaleColorMap();
}
} // namespace

namespace tomviz {

LoadDataReaction::LoadDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{}

LoadDataReaction::~LoadDataReaction() = default;

void LoadDataReaction::onTriggered()
{
  loadData();
}

pipeline::SourceNode* LoadDataReaction::createSourceFromImageData(
  vtkImageData* image, const QString& label, const QStringList& fileNames)
{
  QString nodeLabel = label;
  if (nodeLabel.isEmpty() && !fileNames.isEmpty()) {
    nodeLabel = QFileInfo(fileNames[0]).completeBaseName();
  }

  auto volumeData = std::make_shared<pipeline::VolumeData>(image);
  volumeData->setLabel(nodeLabel);
  pipeline::PortType dataType = volumeData->hasTiltAngles()
    ? pipeline::PortType::TiltSeries
    : pipeline::PortType::Volume;

  // If we have file paths, use a ReaderSourceNode so a future state-file
  // load can re-run the reader from the saved paths. Otherwise fall back
  // to a bare SourceNode — the data lives purely in memory and can only
  // round-trip through a container that embeds voxels (.tvh5).
  pipeline::SourceNode* source = nullptr;
  if (!fileNames.isEmpty()) {
    auto* reader = new pipeline::ReaderSourceNode();
    reader->setFileNames(fileNames);
    // Pre-populate the output port with the already-read data so the
    // caller doesn't have to re-read immediately. ReaderSourceNode's
    // constructor declares "volume" as ImageData; refine to the
    // detected effective type (TiltSeries / Volume).
    reader->outputPort("volume")->setDeclaredType(dataType);
    source = reader;
  } else {
    source = new pipeline::SourceNode();
    source->addOutput("volume", dataType);
  }
  source->setLabel(nodeLabel);
  source->setOutputData("volume", pipeline::PortData(volumeData, dataType));
  return source;
}

QList<pipeline::SourceNode*> LoadDataReaction::loadData(bool isTimeSeries)
{
  QStringList filters;
  filters << "Common file types (*.emd *.jpg *.jpeg *.png *.tiff *.tif *.h5 "
             "*.hspy *.nxs *.raw *.dat *.bin *.txt *.mhd *.mha *.vti *.mrc "
             "*.st *.rec *.ali *.xmf *.xdmf *.npy *.mat *.dcm)"
          << "EMD (*.emd)"
          << "JPeg Image files (*.jpg *.jpeg)"
          << "PNG Image files (*.png)"
          << "TIFF Image files (*.tiff *.tif)"
          << "HDF5 files (*.h5 *.hspy *.nxs)"
          << "OME-TIFF Image files (*.ome.tif)"
          << "Raw data files (*.raw *.dat *.bin)"
          << "Meta Image files (*.mhd *.mha)"
          << "VTK ImageData Files (*.vti)"
          << "MRC files (*.mrc *.st *.rec *.ali)"
          << "XDMF files (*.xmf *.xdmf)"
          << "Text files (*.txt)";

  foreach (auto reader,
           FileFormatManager::instance().pythonReaderFactories()) {
    filters << reader->getFileDialogFilter();
  }

  filters << "All files (*.*)";

  QFileDialog dialog(tomviz::mainWidget());
  dialog.setFileMode(QFileDialog::ExistingFiles);
  dialog.setNameFilters(filters);
  dialog.setObjectName("FileOpenDialog-tomviz"); // avoid name collision?

  if (!dialog.exec()) {
    return {};
  }

  QStringList filenames = dialog.selectedFiles();

  QJsonObject options;
  if (isTimeSeries) {
    // Sort the file names so we get consistent behavior.
    filenames.sort();
    options["createCameraOrbit"] = false;
  }

  QList<pipeline::SourceNode*> sources;
  for (auto f : filenames) {
    sources << loadData(f, options);
    if (isTimeSeries) {
      // After loading the first source in a time series, don't
      // add any more to the pipeline. We'll delete them below.
      options["addToPipeline"] = false;
    }
  }

  if (isTimeSeries && !sources.isEmpty()) {
    // Combine all sources into the first one via time steps.
    auto firstVol =
      pipeline::getOutputData<pipeline::VolumeDataPtr>(sources[0]);
    if (firstVol) {
      std::vector<double> times;
      QList<pipeline::VolumeData::TimeStep> timeSteps;

      for (int i = 0; i < sources.size(); ++i) {
        auto vol =
          pipeline::getOutputData<pipeline::VolumeDataPtr>(sources[i]);
        if (!vol) {
          continue;
        }
        double t = static_cast<double>(i);
        times.push_back(t);
        timeSteps.append({ vol->label(), vol->imageData(), t });

        if (i != 0) {
          // Delete all sources other than the first one. These were not
          // added to the pipeline.
          sources[i]->deleteLater();
        }
      }
      firstVol->setTimeSteps(timeSteps);
      sources = { sources[0] };

      // Drive step switching from the animation clock. Self-owned: it
      // deletes itself when the source node goes away.
      new TimeSeriesAnimation(sources[0]);

      // Set the animation time steps and change the play mode to
      // "Snap To TimeSteps".
      tomviz::snapAnimationToTimeSteps(times);

      // Also set the number of time steps in the sequence to match
      // (this only matters if the user switches to "Sequence" play mode)
      tomviz::setAnimationNumberOfFrames(static_cast<int>(times.size()));
    }
  }

  return sources;
}

pipeline::SourceNode* LoadDataReaction::loadData(const QString& fileName,
                                                  bool defaultModules,
                                                  bool addToRecent, bool child,
                                                  const QJsonObject& options)
{
  QJsonObject opts = options;
  opts["defaultModules"] = defaultModules;
  opts["addToRecent"] = addToRecent;
  opts["child"] = child;
  return LoadDataReaction::loadData(fileName, opts);
}

pipeline::SourceNode* LoadDataReaction::loadData(const QString& fileName,
                                                  const QJsonObject& val)
{
  QStringList fileNames;
  fileNames << fileName;

  return loadData(fileNames, val);
}

pipeline::SourceNode* LoadDataReaction::loadData(const QStringList& fileNames,
                                                  const QJsonObject& options)
{
  bool defaultModules = options["defaultModules"].toBool(true);
  bool addToRecent = options["addToRecent"].toBool(true);
  bool addToPipeline = options["addToPipeline"].toBool(true);
  bool createCameraOrbit = options["createCameraOrbit"].toBool(true);
  bool child = options["child"].toBool(false);

  QString fileName;
  if (fileNames.size() > 0) {
    fileName = fileNames[0];
  }
  QFileInfo info(fileName);

  // Try the headless helper first. It handles every format we can read
  // without UI (tvh5, emd, h5, ome.tif, tif/png/jpg/vti/mrc, registered
  // Python readers, and ParaView readers when a descriptor is supplied
  // in options["reader"]).
  ReadResult result = readImageData(fileNames, options);

  pipeline::SourceNode* source = nullptr;
  if (result.imageData) {
    source = createSourceFromImageData(result.imageData,
                                       info.completeBaseName(), fileNames);
    if (!source) {
      return nullptr;
    }
    // Tilt angles — copy onto the VolumeData and promote port type.
    if (!result.tiltAngles.isEmpty()) {
      auto vol = pipeline::getOutputData<pipeline::VolumeDataPtr>(source);
      if (vol) {
        vol->setTiltAngles(result.tiltAngles);
        source->outputPort("volume")->setDeclaredType(
          pipeline::PortType::TiltSeries);
        source->setProperty("dataType", "tiltSeries");
      }
    }
    // DataExchange / FXI auxiliary frames.
    if (result.darkData) {
      source->setProperty(
        "darkData", QVariant::fromValue(
                      static_cast<void*>(result.darkData.GetPointer())));
    }
    if (result.whiteData) {
      source->setProperty(
        "whiteData",
        QVariant::fromValue(
          static_cast<void*>(result.whiteData.GetPointer())));
    }
    if (!result.readerProperties.isEmpty()) {
      source->setProperty(
        "readerProperties",
        QVariant::fromValue(result.readerProperties.toVariantMap()));
    }
    if (!result.tvh5NodePath.isEmpty()) {
      source->setProperty("tvh5NodePath", result.tvh5NodePath);
    }
    // If the node is a ReaderSourceNode, stash the reader descriptor and
    // subsample settings so a re-execute after a state-file load can
    // invoke readImageData with the same configuration.
    if (auto* reader = qobject_cast<pipeline::ReaderSourceNode*>(source)) {
      QJsonObject readerOpts;
      if (!result.readerProperties.isEmpty()) {
        readerOpts["reader"] = result.readerProperties;
      }
      if (options.contains("subsampleSettings")) {
        readerOpts["subsampleSettings"] = options.value("subsampleSettings");
      }
      if (!result.tvh5NodePath.isEmpty()) {
        readerOpts["tvh5NodePath"] = result.tvh5NodePath;
      }
      if (!readerOpts.isEmpty()) {
        reader->setReaderOptions(readerOpts);
      }
    }
  } else if (!options.contains("reader")) {
    // Fallback path — let ParaView's interactive file loader run (this
    // may pop a reader-config dialog). Only reached when nothing else
    // claimed the extension AND the caller didn't provide a reader
    // descriptor (state-file loads / re-executions always do).
    pqPipelineSource* reader = pqLoadDataReaction::loadData(fileNames);
    if (!reader) {
      return nullptr;
    }
    bool addToThePipeline = false;
    source = createFromParaViewReader(reader->getProxy(), defaultModules,
                                      child, addToThePipeline);
    if (!source) {
      return nullptr;
    }

    QJsonObject props = readerProperties(reader->getProxy());
    props["name"] = reader->getProxy()->GetXMLName();
    source->setProperty("readerProperties",
                        QVariant::fromValue(props.toVariantMap()));
    // Propagate to ReaderSourceNode so re-executes after a state-file
    // load can rebuild the ParaView reader headlessly.
    if (auto* readerNode =
          qobject_cast<pipeline::ReaderSourceNode*>(source)) {
      QJsonObject readerOpts;
      readerOpts["reader"] = props;
      readerNode->setReaderOptions(readerOpts);
    }

    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(reader->getProxy());
  }

  if (!source) {
    return nullptr;
  }

  // Set file names as a node property so the label is available.
  source->setProperty("fileNames", fileNames);

  if (addToPipeline) {
    LoadDataReaction::sourceNodeAdded(source, defaultModules, child,
                                      createCameraOrbit);
  }

  if (addToRecent && source) {
    RecentFilesMenu::pushDataReader(source);
  }
  return source;
}

pipeline::SourceNode* LoadDataReaction::createFromParaViewReader(
  vtkSMProxy* reader, bool defaultModules, bool child, bool addToPipeline)
{
  // Prompt user for reader configuration, unless it is TIFF.
  QScopedPointer<QDialog> dialog(new pqProxyWidgetDialog(reader));
  bool hasVisibleWidgets =
    qobject_cast<pqProxyWidgetDialog*>(dialog.data())->hasVisibleWidgets();
  if (QString(reader->GetXMLName()) == "TVRawImageReader") {
    dialog.reset(new RAWFileReaderDialog(reader));
    hasVisibleWidgets = true;
    // This will show only a partial filename when reading multiple files as a
    // raw image
    vtkSMPropertyHelper fname(reader, "FilePrefix");
    QFileInfo info(fname.GetAsString());
    dialog->setWindowTitle(QString("Opening %1").arg(info.fileName()));
  } else {
    dialog->setWindowTitle("Configure Reader Parameters");
  }
  dialog->setObjectName("ConfigureReaderDialog");
  if (QString(reader->GetXMLName()) == "TIFFSeriesReader" ||
      hasVisibleWidgets == false || dialog->exec() == QDialog::Accepted) {

    if (!hasData(reader)) {
      qCritical() << "Error: failed to load file!";
      return nullptr;
    }

    auto pvSource = vtkSMSourceProxy::SafeDownCast(reader);
    pvSource->UpdatePipeline();
    auto algo =
      vtkAlgorithm::SafeDownCast(pvSource->GetClientSideObject());
    auto data = algo->GetOutputDataObject(0);
    auto image = vtkImageData::SafeDownCast(data);

    QString readerFileName;
    auto prop = reader->GetProperty("FileNames");
    if (prop != nullptr) {
      auto jsonProp = toJson(prop);
      if (jsonProp.toArray().size() > 0) {
        readerFileName = jsonProp.toArray()[0].toString();
      } else {
        readerFileName = jsonProp.toString();
      }
      QFileInfo fInfo(readerFileName);
      // Special case: mrc files store spacing in Angstrom
      QStringList mrcExt;
      mrcExt << "mrc"
             << "st"
             << "rec"
             << "ali";
      if (mrcExt.contains(fInfo.suffix().toLower())) {
        double spacing[3];
        image->GetSpacing(spacing);
        for (int i = 0; i < 3; ++i) {
          spacing[i] *= 0.1;
        }
        image->SetSpacing(spacing);
      }
    }

    QString label;
    if (!readerFileName.isEmpty()) {
      label = QFileInfo(readerFileName).completeBaseName();
    }

    auto* source = createSourceFromImageData(
      image, label, readerFileName.isEmpty() ? QStringList()
                                             : QStringList({ readerFileName }));

    // Do whatever we need to do with a new source node.
    if (addToPipeline) {
      LoadDataReaction::sourceNodeAdded(source, defaultModules, child);
    }
    return source;
  }
  return nullptr;
}

void LoadDataReaction::addSourceToPipeline(pipeline::SourceNode* source)
{
  if (!source) {
    return;
  }
  auto* pip = ActiveObjects::instance().pipeline();
  if (!pip) {
    return;
  }
  pip->addNode(source);
}

void LoadDataReaction::completeSourceSetup(pipeline::SourceNode* source,
                                           bool defaultModules)
{
  if (!source) {
    return;
  }
  auto* pip = ActiveObjects::instance().pipeline();
  if (!pip) {
    return;
  }

  // Apply the default color map preset to every output port. File-loaded
  // sources already have data when we get here; sources that produce data
  // on first execute get a single-shot listener per port.
  for (auto* port : source->outputPorts()) {
    if (port->hasData()) {
      applyDefaultColorMapPreset(port);
    } else {
      QObject::connect(port, &pipeline::OutputPort::dataChanged, port,
                       [port]() { applyDefaultColorMapPreset(port); },
                       Qt::SingleShotConnection);
    }
  }

  // Get view for visualization
  ActiveObjects::instance().createRenderViewIfNeeded();
  auto view = ActiveObjects::instance().activeView();

  if (!view || QString(view->GetXMLName()) != "RenderView") {
    ActiveObjects::instance().setActiveViewToFirstRenderView();
    view = ActiveObjects::instance().activeView();
  }

  if (defaultModules && view) {
    // Only create default sinks for the first output port; additional
    // outputs get colormaps but no automatic visualization.
    auto* group = new pipeline::SinkGroupNode();
    group->addPassthrough("volume", pipeline::PortType::ImageData);
    pip->addNode(group);
    pip->createLink(source->outputPorts()[0], group->inputPorts()[0]);

    auto* outline = new pipeline::OutlineSink();
    outline->setLabel("Outline");
    outline->initialize(view);
    pip->addNode(outline);
    pip->createLink(group->outputPorts()[0], outline->inputPorts()[0]);

    bool isTiltSeries = source->outputPorts()[0]->type() ==
                        pipeline::PortType::TiltSeries;
    if (isTiltSeries) {
      auto* slice = new pipeline::SliceSink();
      slice->setLabel("Slice");
      slice->initialize(view);
      pip->addNode(slice);
      pip->createLink(group->outputPorts()[0], slice->inputPorts()[0]);
    } else {
      auto* volume = new pipeline::VolumeSink();
      volume->setLabel("Volume");
      volume->initialize(view);
      pip->addNode(volume);
      pip->createLink(group->outputPorts()[0], volume->inputPorts()[0]);
    }
  }
}

void LoadDataReaction::sourceNodeAdded(pipeline::SourceNode* source,
                                       bool defaultModules, bool /* child */,
                                       bool createCameraOrbit)
{
  auto* pip = ActiveObjects::instance().pipeline();
  bool isFirstSource = pip && pip->nodes().isEmpty();

  addSourceToPipeline(source);
  completeSourceSetup(source, defaultModules);

  // Each pip->addNode() above auto-selects the node (to advance the tip
  // output port as nodes are added); opening a file would otherwise leave the
  // last sink showing in the properties panel. Clear the selection so nothing
  // is selected — the tip is unchanged (a sink has no output port, so it was
  // already derived via findTipOutputPort), only the properties panel empties.
  ActiveObjects::instance().clearActiveSelection();

  if (isFirstSource && createCameraOrbit && pip) {
    // Give the animation enough frames to show motion once the first
    // execution completes. The opening spin itself is made when Play is
    // pressed with nothing else set up (see AnimationSceneGuard), from
    // whatever the user is looking at then.
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = QObject::connect(
      pip, &pipeline::Pipeline::executionFinished, pip, [conn]() {
        QObject::disconnect(*conn);
        tomviz::setAnimationNumberOfFrames(200);
      });
  }

  // Defer so the event loop can process pending signals before executing.
  // ThreadedExecutor handles the case where it's already running.
  if (pip) {
    QTimer::singleShot(0, pip, [pip]() { pip->execute(); });
  }
}

QJsonObject LoadDataReaction::readerProperties(vtkSMProxy* reader)
{
  QStringList propNames({ "DataScalarType", "DataByteOrder",
                          "NumberOfScalarComponents", "DataExtent" });

  QJsonObject props;
  foreach (QString propName, propNames) {
    auto prop = reader->GetProperty(propName.toLatin1().data());
    if (prop != nullptr) {
      props[propName] = toJson(prop);
    }
  }

  // Special case file name related properties
  auto prop = reader->GetProperty("FileName");
  if (prop != nullptr) {
    props["fileName"] = toJson(prop);
  }
  prop = reader->GetProperty("FileNames");
  if (prop != nullptr) {
    auto fileNames = toJson(prop).toArray();
    if (fileNames.size() > 1) {
      props["fileNames"] = fileNames;
    }
    // Normalize to fileNames for single value.
    else if (fileNames.size() == 1) {
      props["fileName"] = fileNames[0];
    }
  }
  prop = reader->GetProperty("FilePrefix");
  if (prop != nullptr) {
    props["fileName"] = toJson(prop);
  }

  return props;
}

void LoadDataReaction::setFileNameProperties(const QJsonObject& props,
                                             vtkSMProxy* reader)
{
  auto prop = reader->GetProperty("FileName");
  if (prop != nullptr) {
    if (!props.contains("fileName")) {
      qCritical() << "Reader doesn't have 'fileName' property.";
      return;
    }
    tomviz::setProperty(props["fileName"], prop);
  }
  prop = reader->GetProperty("FileNames");
  if (prop != nullptr) {
    if (!props.contains("fileNames") && !props.contains("fileName")) {
      qCritical()
        << "Reader doesn't have 'fileName' or 'fileNames' property.";
      return;
    }

    if (props.contains("fileNames")) {
      tomviz::setProperty(props["fileNames"], prop);
    } else {
      QJsonArray fileNames;
      fileNames.append(props["fileName"]);
      tomviz::setProperty(fileNames, prop);
    }
  }
  prop = reader->GetProperty("FilePrefix");
  if (prop != nullptr) {
    if (!props.contains("fileName")) {
      qCritical() << "Reader doesn't have 'fileName' property.";
      return;
    }
    tomviz::setProperty(props["fileName"], prop);
  }
}

} // end of namespace tomviz
