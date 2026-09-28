/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeFactory.h"

#include "Node.h"
#include "SinkGroupNode.h"
#include "SourceNode.h"
#include "sinks/ClipSink.h"
#include "sinks/ContourSink.h"
#include "sinks/LabelMapSink.h"
#include "sinks/MoleculeSink.h"
#include "sinks/OutlineSink.h"
#include "sinks/PlotSink.h"
#include "sinks/RulerSink.h"
#include "sinks/ScaleCubeSink.h"
#include "sinks/SegmentSink.h"
#include "sinks/SliceSink.h"
#include "sinks/ThresholdSink.h"
#include "sinks/VolumeSink.h"
#include "sinks/VolumeStatsSink.h"
#include "sources/PythonSource.h"
#include "sources/ReaderSourceNode.h"
#include "sources/SphereSource.h"
#include "transforms/ArrayWranglerTransform.h"
#include "transforms/ConvertToFloatTransform.h"
#include "transforms/ConvertToVolumeTransform.h"
#include "transforms/CropTransform.h"
#include "transforms/LegacyPythonTransform.h"
#include "transforms/PythonTransform.h"
#include "transforms/ReconstructionTransform.h"
#include "transforms/SetTiltAnglesTransform.h"
#include "transforms/SnapshotTransform.h"
#include "transforms/ThresholdTransform.h"
#include "transforms/TransposeDataTransform.h"
#include "transforms/TranslateAlignTransform.h"

namespace tomviz {
namespace pipeline {

NodeFactory& NodeFactory::instance()
{
  static NodeFactory inst;
  return inst;
}

Node* NodeFactory::create(const QString& typeName)
{
  auto& inst = instance();
  auto it = inst.m_creators.constFind(typeName);
  if (it == inst.m_creators.constEnd()) {
    return nullptr;
  }
  return it.value()();
}

QString NodeFactory::typeName(const Node* node)
{
  if (!node) {
    return {};
  }
  auto& inst = instance();
  auto it = inst.m_typeNames.find(std::type_index(typeid(*node)));
  if (it == inst.m_typeNames.end()) {
    return {};
  }
  return it->second;
}

void NodeFactory::registerBuiltins()
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  // Base SourceNode — used by the legacy loader and several reactions
  // (LoadDataReaction, CloneDataReaction, MergeImagesReaction, ...) to
  // expose an already-materialized VolumeData that has no file-reading
  // counterpart. Ports are reconstructed from the "outputPorts" map in
  // SourceNode::deserialize.
  registerType<SourceNode>(QStringLiteral("source.generic"));
  registerType<ReaderSourceNode>(QStringLiteral("source.reader"));
  registerType<SphereSource>(QStringLiteral("source.sphere"));
  registerType<PythonSource>(QStringLiteral("source.python"));

  registerType<ArrayWranglerTransform>(
    QStringLiteral("transform.arrayWrangler"));
  registerType<ConvertToFloatTransform>(
    QStringLiteral("transform.convertToFloat"));
  registerType<ConvertToVolumeTransform>(
    QStringLiteral("transform.convertToVolume"));
  registerType<CropTransform>(QStringLiteral("transform.crop"));
  registerType<LegacyPythonTransform>(
    QStringLiteral("transform.legacyPython"));
  registerType<PythonTransform>(QStringLiteral("transform.python"));
  registerType<ReconstructionTransform>(
    QStringLiteral("transform.reconstruction"));
  registerType<SetTiltAnglesTransform>(
    QStringLiteral("transform.setTiltAngles"));
  registerType<SnapshotTransform>(QStringLiteral("transform.snapshot"));
  registerType<ThresholdTransform>(QStringLiteral("transform.threshold"));
  registerType<TransposeDataTransform>(
    QStringLiteral("transform.transposeData"));
  registerType<TranslateAlignTransform>(
    QStringLiteral("transform.translateAlign"));

  registerType<ClipSink>(QStringLiteral("sink.clip"));
  registerType<ContourSink>(QStringLiteral("sink.contour"));
  registerType<LabelMapSink>(QStringLiteral("sink.labelMap"));
  registerType<MoleculeSink>(QStringLiteral("sink.molecule"));
  registerType<OutlineSink>(QStringLiteral("sink.outline"));
  registerType<PlotSink>(QStringLiteral("sink.plot"));
  registerType<RulerSink>(QStringLiteral("sink.ruler"));
  registerType<ScaleCubeSink>(QStringLiteral("sink.scaleCube"));
  registerType<SegmentSink>(QStringLiteral("sink.segment"));
  registerType<SliceSink>(QStringLiteral("sink.slice"));
  registerType<ThresholdSink>(QStringLiteral("sink.threshold"));
  registerType<VolumeSink>(QStringLiteral("sink.volume"));
  registerType<VolumeStatsSink>(QStringLiteral("sink.volumeStats"));

  registerType<SinkGroupNode>(QStringLiteral("sinkGroup"));
}

} // namespace pipeline
} // namespace tomviz
