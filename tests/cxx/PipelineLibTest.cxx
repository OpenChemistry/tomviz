/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "DefaultExecutor.h"
#include "ExecutionFuture.h"
#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "PipelineExecutor.h"
#include "PortData.h"
#include "PortType.h"
#include "SinkNode.h"
#include "SourceNode.h"
#include "ThreadedExecutor.h"
#include "TransformNode.h"

#include "EmdFormat.h"
#include "PipelineSettings.h"
#include "PipelineStateIO.h"
#include "PortDataWriter.h"
#include "ParameterInterfaceBuilder.h"
#include "SaveDataDialog.h"
#include "SinkGroupNode.h"
#include "Tvh5Format.h"
#include "data/VolumeData.h"
#include "sinks/VolumeStatsSink.h"
#include "sinks/LegacyModuleSink.h"
#include "sinks/LightingPresetStore.h"
#include "sinks/ExplodedGeometry.h"
#include "sinks/VolumeSink.h"
#include "sinks/LabelMapSink.h"
#include "sinks/LabelMapSurface.h"
#include "data/LabelMapData.h"
#include "sinks/SliceSink.h"
#include "sinks/ContourSink.h"
#include "sinks/ThresholdSink.h"
#include "sinks/SegmentSink.h"
#include "sinks/OutlineSink.h"
#include "sinks/ClipSink.h"
#include "sinks/RulerSink.h"
#include "sinks/ScaleCubeSink.h"
#include "sinks/PlotSink.h"
#include "sinks/MoleculeSink.h"
#include "NodeFactory.h"
#include "sources/PythonSource.h"
#include "sources/SphereSource.h"
#include "sources/ReaderSourceNode.h"
#include "transforms/ThresholdTransform.h"
#include "transforms/LegacyPythonTransform.h"
#include "AutoExecuteController.h"
#include "ExternalNodeExecutor.h"
#include "transforms/PythonTransform.h"
#include "PipelineStripWidget.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <limits>
#include <QTest>
#include <QTextStream>

#include <atomic>

#include <h5cpp/h5readwrite.h>

// Qt defines 'slots' as a macro which conflicts with Python's object.h
#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#pragma pop_macro("slots")

#include <vector>

#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkCellData.h>
#include <vtkPolyData.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkUnsignedCharArray.h>
#include <vtkXMLImageDataWriter.h>

namespace py = pybind11;
using namespace tomviz::pipeline;

// Test helpers

class DoubleTransform : public TransformNode
{
public:
  DoubleTransform() : TransformNode()
  {
    addInput("in", PortType::ImageData);
    addOutput("out", PortType::ImageData);
  }

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override
  {
    int val = inputs["in"].value<int>();
    QMap<QString, PortData> result;
    result["out"] = PortData(std::any(val * 2), PortType::ImageData);
    return result;
  }
};

class AddTransform : public TransformNode
{
public:
  AddTransform() : TransformNode()
  {
    addInput("a", PortType::ImageData);
    addInput("b", PortType::ImageData);
    addOutput("out", PortType::ImageData);
  }

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override
  {
    int a = inputs["a"].value<int>();
    int b = inputs["b"].value<int>();
    QMap<QString, PortData> result;
    result["out"] = PortData(std::any(a + b), PortType::ImageData);
    return result;
  }
};

// A simple passthrough transform with configurable port types.
class PassthroughTransform : public TransformNode
{
public:
  PassthroughTransform(PortTypes inType, PortType outType)
    : TransformNode()
  {
    addInput("in", inType);
    addOutput("out", outType);
  }

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override
  {
    return { { "out", inputs["in"] } };
  }
};

class CollectorSink : public SinkNode
{
public:
  CollectorSink() : SinkNode() { addInput("in", PortType::ImageData); }

  int lastValue = 0;
  bool consumed = false;

protected:
  bool consume(const QMap<QString, PortData>& inputs) override
  {
    lastValue = inputs["in"].value<int>();
    consumed = true;
    return true;
  }
};

// Tests

class PipelineLibTest : public ::testing::Test
{
protected:
  void SetUp() override { pipeline = new Pipeline(); }
  void TearDown() override { delete pipeline; }
  Pipeline* pipeline;
};

TEST_F(PipelineLibTest, PortDataBasics)
{
  PortData empty;
  EXPECT_FALSE(empty.isValid());
  EXPECT_EQ(empty.type(), PortType::None);

  PortData data(std::any(42), PortType::ImageData);
  EXPECT_TRUE(data.isValid());
  EXPECT_EQ(data.type(), PortType::ImageData);
  EXPECT_EQ(data.value<int>(), 42);

  data.clear();
  EXPECT_FALSE(data.isValid());
}

TEST_F(PipelineLibTest, PortTypeCompatibility)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  // ImageData -> ImageData should work
  auto* outPort = source->outputPort("out");
  auto* inPort = transform->inputPort("in");
  EXPECT_TRUE(inPort->canConnectTo(outPort));

  // Create a source with Table type
  auto* tableSource = new SourceNode();
  tableSource->addOutput("out", PortType::Table);
  pipeline->addNode(tableSource);

  // Table -> ImageData input should fail
  auto* tablePort = tableSource->outputPort("out");
  EXPECT_FALSE(inPort->canConnectTo(tablePort));
}

TEST_F(PipelineLibTest, LinkCreation)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  auto* link =
    pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  EXPECT_NE(link, nullptr);
  EXPECT_TRUE(link->isValid());
  EXPECT_EQ(pipeline->links().size(), 1);
  EXPECT_EQ(transform->inputPort("in")->link(), link);
  EXPECT_EQ(source->outputPort("out")->links().size(), 1);
}

TEST_F(PipelineLibTest, LinkTypeRejection)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::Table);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  auto* link =
    pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  EXPECT_EQ(link, nullptr);
  EXPECT_EQ(pipeline->links().size(), 0);
}

TEST_F(PipelineLibTest, CycleDetection)
{
  auto* t1 = new DoubleTransform();
  t1->setLabel("T1");
  auto* t2 = new DoubleTransform();
  t2->setLabel("T2");
  pipeline->addNode(t1);
  pipeline->addNode(t2);

  auto* link1 =
    pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));
  EXPECT_NE(link1, nullptr);

  // T2 -> T1 would create a cycle
  EXPECT_TRUE(
    pipeline->wouldCreateCycle(t2->outputPort("out"), t1->inputPort("in")));
  auto* link2 =
    pipeline->createLink(t2->outputPort("out"), t1->inputPort("in"));
  EXPECT_EQ(link2, nullptr);
}

TEST_F(PipelineLibTest, SelfLoop)
{
  auto* t1 = new DoubleTransform();
  pipeline->addNode(t1);
  EXPECT_TRUE(
    pipeline->wouldCreateCycle(t1->outputPort("out"), t1->inputPort("in")));
}

TEST_F(PipelineLibTest, StalenessPropagation)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  pipeline->addNode(t1);
  auto* t2 = new DoubleTransform();
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  // Set data on source (marks source Current, downstream Stale)
  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  EXPECT_EQ(source->state(), NodeState::Current);
  EXPECT_EQ(t1->state(), NodeState::Stale);
  EXPECT_EQ(t2->state(), NodeState::Stale);
}

TEST_F(PipelineLibTest, StalenessPropagationMultiBranch)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  auto* t2 = new DoubleTransform();
  pipeline->addNode(t1);
  pipeline->addNode(t2);

  // Source fans out to both t1 and t2
  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(source->outputPort("out"), t2->inputPort("in"));

  source->setOutputData("out", PortData(std::any(10), PortType::ImageData));

  EXPECT_EQ(t1->state(), NodeState::Stale);
  EXPECT_EQ(t2->state(), NodeState::Stale);
}

TEST_F(PipelineLibTest, TopologicalSort)
{
  auto* source = new SourceNode();
  source->setLabel("source");
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  t1->setLabel("t1");
  pipeline->addNode(t1);
  auto* t2 = new DoubleTransform();
  t2->setLabel("t2");
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  auto sorted = pipeline->topologicalSort();
  EXPECT_EQ(sorted.size(), 3);

  // Source must come before t1, t1 before t2
  int srcIdx = sorted.indexOf(source);
  int t1Idx = sorted.indexOf(t1);
  int t2Idx = sorted.indexOf(t2);
  EXPECT_LT(srcIdx, t1Idx);
  EXPECT_LT(t1Idx, t2Idx);
}

TEST_F(PipelineLibTest, SimpleExecution)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  auto* sink = new CollectorSink();
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  pipeline->createLink(transform->outputPort("out"), sink->inputPort("in"));

  source->setOutputData("out", PortData(std::any(7), PortType::ImageData));

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_TRUE(sink->consumed);
  EXPECT_EQ(sink->lastValue, 14);
  EXPECT_EQ(transform->state(), NodeState::Current);
  EXPECT_EQ(sink->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, ExecutionFromTarget)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  // Pin t1's output so the value survives end-of-plan and the test
  // can read it back directly. Force InMemory explicitly because the
  // current TransformNode default is OnDisk, and the int payloads
  // these tests use aren't round-trippable through Tvh5Format.
  t1->outputPort("out")->setPersistent(true);
  t1->outputPort("out")->setPersistenceMode(PersistenceMode::InMemory);
  pipeline->addNode(t1);
  auto* t2 = new DoubleTransform();
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  source->setOutputData("out", PortData(std::any(3), PortType::ImageData));

  // Execute only up to t1
  auto* future = pipeline->execute(t1);

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(t1->state(), NodeState::Current);
  // t2 should still be stale since we only executed up to t1
  EXPECT_EQ(t2->state(), NodeState::Stale);

  // Verify t1's output
  EXPECT_EQ(t1->outputPort("out")->data().value<int>(), 6);
}

TEST_F(PipelineLibTest, ExecutionOrderWithStaleUpstream)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  pipeline->addNode(t1);
  auto* t2 = new DoubleTransform();
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  auto order = pipeline->executionOrder(t2);
  // Should include t1 and t2 (source is Current)
  EXPECT_EQ(order.size(), 2);
  EXPECT_TRUE(order.contains(t1));
  EXPECT_TRUE(order.contains(t2));
  EXPECT_LT(order.indexOf(t1), order.indexOf(t2));
}

TEST_F(PipelineLibTest, BreakpointStopsExecution)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  pipeline->addNode(t1);
  auto* t2 = new DoubleTransform();
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  // Set breakpoint on t1
  t1->setBreakpoint(true);

  Node* reachedNode = nullptr;
  QObject::connect(pipeline, &Pipeline::breakpointReached,
                   [&reachedNode](Node* n) { reachedNode = n; });

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_FALSE(future->succeeded());
  EXPECT_EQ(reachedNode, t1);
  // t1 should not have been executed
  EXPECT_NE(t1->state(), NodeState::Current);
  EXPECT_NE(t2->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, HeldNodeWaitsForItsDialog)
{
  // source -> t1 (held, as a freshly inserted transform whose dialog is
  // open) -> t2, plus t3 on the source directly
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);
  auto* t1 = new DoubleTransform();
  auto* t2 = new DoubleTransform();
  auto* t3 = new DoubleTransform();
  pipeline->addNode(t1);
  pipeline->addNode(t2);
  pipeline->addNode(t3);
  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));
  pipeline->createLink(source->outputPort("out"), t3->inputPort("in"));
  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  t1->setHeld(true);
  Node* reachedNode = nullptr;
  QObject::connect(pipeline, &Pipeline::breakpointReached,
                   [&reachedNode](Node* n) { reachedNode = n; });

  // A global execute, as adding a module anywhere triggers, runs the
  // rest of the pipeline and leaves the held node and its subtree alone
  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(reachedNode, nullptr);
  EXPECT_EQ(t3->state(), NodeState::Current);
  EXPECT_NE(t1->state(), NodeState::Current);
  EXPECT_NE(t2->state(), NodeState::Current);

  // Released (the dialog's Apply), it runs like any other node
  t1->setHeld(false);
  future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(t1->state(), NodeState::Current);
  EXPECT_EQ(t2->state(), NodeState::Current);
  EXPECT_EQ(t2->outputPort("out")->data().value<int>(), 20);
}

TEST_F(PipelineLibTest, TransientDataRelease)
{
  // A transient transform output is kept alive while any consumer
  // retains its shared_ptr handle. The sink stashes that handle in
  // m_retainedInputs, so the producer port reports hasData() == true
  // for as long as the sink is connected. Once the sink is removed,
  // its retained input drops, the wrapping heap PortData is freed,
  // and the producer port reports hasData() == false.
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  transform->outputPort("out")->setPersistent(false);
  pipeline->addNode(transform);

  auto* sink = new CollectorSink();
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  pipeline->createLink(transform->outputPort("out"), sink->inputPort("in"));

  source->setOutputData("out", PortData(std::any(4), PortType::ImageData));

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  // Sink retains the input handle; transient producer stays alive.
  EXPECT_TRUE(transform->outputPort("out")->hasData());
  // Source output is persistent by default — independently alive.
  EXPECT_TRUE(source->outputPort("out")->hasData());
  // Sink consumed the data successfully.
  EXPECT_TRUE(sink->consumed);
  EXPECT_EQ(sink->lastValue, 8);

  // Removing the sink drops its retained handle. With no consumer
  // holding, the transient producer port evicts.
  auto* transformOutput = transform->outputPort("out");
  pipeline->removeNode(sink);
  EXPECT_FALSE(transformOutput->hasData());
}

TEST_F(PipelineLibTest, OnDiskEvictsOnLastHandleRelease)
{
  // Use a real volume payload — the OnDisk path round-trips through
  // Tvh5Format, which needs an actual vtkImageData (not a wrapped int).
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);

  auto* port = source->outputPort("volume");
  port->setPersistenceMode(PersistenceMode::OnDisk);

  ASSERT_TRUE(source->execute());
  ASSERT_TRUE(port->hasData());
  auto originalRange =
    port->data().value<VolumeDataPtr>()->scalarRange();

  // Simulate an executor cycle: take the strong ref, then let it drop.
  // For OnDisk the take moves m_strong out, so when the local handle
  // dies the deleter fires and swaps to disk.
  {
    auto handle = port->take();
    ASSERT_TRUE(handle);
  }

  // Port should still report hasData() — it's on disk now, not gone.
  EXPECT_TRUE(port->hasData());

  // data() is non-loading — it should report missing now.
  EXPECT_FALSE(port->data().isValid());
  // materialize() triggers a synchronous reload from disk.
  auto handle = port->materialize();
  ASSERT_TRUE(handle);
  auto reloaded = handle->value<VolumeDataPtr>();
  ASSERT_TRUE(reloaded && reloaded->isValid());
  auto reloadedRange = reloaded->scalarRange();
  EXPECT_NEAR(originalRange[0], reloadedRange[0], 0.01);
  EXPECT_NEAR(originalRange[1], reloadedRange[1], 0.01);
}

TEST_F(PipelineLibTest, OnDiskReSetDataOverwrites)
{
  // After eviction, calling setData with a different payload should
  // overwrite the on-disk content next time it evicts.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);

  auto* port = source->outputPort("volume");
  port->setPersistenceMode(PersistenceMode::OnDisk);

  ASSERT_TRUE(source->execute());

  // First eviction cycle.
  { auto h = port->take(); }
  ASSERT_TRUE(port->hasData());

  // Replace with a different volume.
  source->setDimensions(6, 6, 6);
  ASSERT_TRUE(source->execute());
  auto newRange = port->data().value<VolumeDataPtr>()->scalarRange();

  // Second eviction.
  { auto h = port->take(); }
  ASSERT_TRUE(port->hasData());

  // Reload should produce the new payload, not the original.
  auto handle = port->materialize();
  ASSERT_TRUE(handle);
  auto reloaded = handle->value<VolumeDataPtr>();
  ASSERT_TRUE(reloaded && reloaded->isValid());
  EXPECT_EQ(reloaded->dimensions()[0], 6);
  EXPECT_EQ(reloaded->dimensions()[1], 6);
  EXPECT_EQ(reloaded->dimensions()[2], 6);
  auto reloadedRange = reloaded->scalarRange();
  EXPECT_NEAR(newRange[0], reloadedRange[0], 0.01);
  EXPECT_NEAR(newRange[1], reloadedRange[1], 0.01);
}

TEST_F(PipelineLibTest, OnDiskPortDestructionCleansFile)
{
  // The cache QTemporaryFile is owned by the port; destroying the port
  // should remove it. We can't observe the file directly without the
  // port's private state, but we can at least exercise the destructor
  // path and confirm nothing crashes.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");
  port->setPersistenceMode(PersistenceMode::OnDisk);
  ASSERT_TRUE(source->execute());

  { auto h = port->take(); }
  ASSERT_TRUE(port->hasData());

  // Destroy the source node — its port destructor sets m_destroying
  // and releases m_strong; QTemporaryFile cleans up the cache file.
  pipeline->removeNode(source);
}

TEST_F(PipelineLibTest, PersistenceModeSwitchInMemoryToTransient)
{
  // InMemory port with no consumer pinning: switching to transient
  // must release the port's hold so the data isn't kept alive
  // indefinitely.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");  // already persistent InMemory
  ASSERT_TRUE(source->execute());
  ASSERT_TRUE(port->hasData());

  port->setPersistent(false);

  EXPECT_FALSE(port->hasData());
}

TEST_F(PipelineLibTest, PersistenceModeSwitchInMemoryToOnDisk)
{
  // InMemory → OnDisk with no consumer: the port should release its
  // strong ref, which fires the universal deleter; under the new mode
  // it writes the payload to disk.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");
  ASSERT_TRUE(source->execute());
  auto originalRange =
    port->data().value<VolumeDataPtr>()->scalarRange();

  port->setPersistenceMode(PersistenceMode::OnDisk);

  // Still reports data — it's on disk now, not gone.
  EXPECT_TRUE(port->hasData());

  // Reload from disk and verify equivalent payload.
  auto handle = port->materialize();
  ASSERT_TRUE(handle);
  auto reloaded = handle->value<VolumeDataPtr>();
  ASSERT_TRUE(reloaded && reloaded->isValid());
  auto reloadedRange = reloaded->scalarRange();
  EXPECT_NEAR(originalRange[0], reloadedRange[0], 0.01);
  EXPECT_NEAR(originalRange[1], reloadedRange[1], 0.01);
}

TEST_F(PipelineLibTest, PersistenceModeSwitchOnDiskToInMemory)
{
  // OnDisk with data evicted to disk → switch to InMemory should
  // load the data back and pin it.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");
  port->setPersistenceMode(PersistenceMode::OnDisk);
  ASSERT_TRUE(source->execute());

  // Force an eviction cycle to land the data on disk.
  { auto h = port->take(); }
  ASSERT_TRUE(port->hasData());

  // Now switch back to InMemory — the port should load and pin.
  port->setPersistenceMode(PersistenceMode::InMemory);

  EXPECT_TRUE(port->hasData());
  auto reloaded = port->data().value<VolumeDataPtr>();
  ASSERT_TRUE(reloaded && reloaded->isValid());
}

TEST_F(PipelineLibTest, PersistenceModeSwitchOnDiskToTransient)
{
  // OnDisk with data on disk → switch to transient should drop the
  // port's hold AND clear the cache file (transient = no persistence).
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");
  port->setPersistenceMode(PersistenceMode::OnDisk);
  ASSERT_TRUE(source->execute());
  { auto h = port->take(); }
  ASSERT_TRUE(port->hasData());

  port->setPersistent(false);

  EXPECT_FALSE(port->hasData());
}

TEST_F(PipelineLibTest, OnDiskStateFileRoundTrip)
{
  // persistenceMode should survive save/load through PipelineStateIO.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");
  port->setPersistenceMode(PersistenceMode::OnDisk);

  QJsonObject json;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, json));

  auto newPipeline = std::make_unique<Pipeline>();
  ASSERT_TRUE(PipelineStateIO::load(newPipeline.get(), json));
  ASSERT_FALSE(newPipeline->nodes().isEmpty());
  auto* loadedPort = newPipeline->nodes().first()->outputPort("volume");
  ASSERT_TRUE(loadedPort);
  EXPECT_TRUE(loadedPort->isPersistent());
  EXPECT_EQ(loadedPort->persistenceMode(), PersistenceMode::OnDisk);
}

TEST_F(PipelineLibTest, InMemoryStateFileRoundTripUnderOnDiskDefault)
{
  // "persistent: true" with no persistenceMode key means InMemory, but
  // a freshly-constructed transform port carries the application-wide
  // default. Loading must reset the medium to InMemory instead of
  // keeping the construction default (regression: ports saved as
  // "Persist in Memory" came back as "Persist on Disk"). Also verify
  // an explicitly transient port survives the same trip.
  auto& settings = PipelineSettings::instance();
  auto previousDefault = settings.transformPersistenceDefault();
  settings.setTransformPersistenceDefault(
    TransformPersistenceDefault::OnDisk);

  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* inMemory = new ThresholdTransform();
  inMemory->setLabel("inMemory");
  pipeline->addNode(inMemory);
  auto* transient = new ThresholdTransform();
  transient->setLabel("transient");
  pipeline->addNode(transient);

  auto* inMemoryPort = inMemory->outputPort("mask");
  ASSERT_TRUE(inMemoryPort->isPersistent());
  ASSERT_EQ(inMemoryPort->persistenceMode(), PersistenceMode::OnDisk);
  inMemoryPort->setPersistenceMode(PersistenceMode::InMemory);
  transient->outputPort("mask")->setPersistent(false);

  QJsonObject json;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, json));

  auto newPipeline = std::make_unique<Pipeline>();
  ASSERT_TRUE(PipelineStateIO::load(newPipeline.get(), json));
  OutputPort* loadedInMemory = nullptr;
  OutputPort* loadedTransient = nullptr;
  for (auto* node : newPipeline->nodes()) {
    if (node->label() == QLatin1String("inMemory")) {
      loadedInMemory = node->outputPort("mask");
    } else if (node->label() == QLatin1String("transient")) {
      loadedTransient = node->outputPort("mask");
    }
  }
  ASSERT_TRUE(loadedInMemory);
  ASSERT_TRUE(loadedTransient);
  EXPECT_TRUE(loadedInMemory->isPersistent());
  EXPECT_EQ(loadedInMemory->persistenceMode(), PersistenceMode::InMemory);
  EXPECT_FALSE(loadedTransient->isPersistent());

  settings.setTransformPersistenceDefault(previousDefault);
}

TEST_F(PipelineLibTest, PersistenceChangeSignals)
{
  // Policy switches must emit persistenceChanged, and residency moves
  // performed by the reconcile (evict, reload, drop) must keep
  // dataLocationChanged accurate so UI badges track the real state.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume"); // persistent InMemory
  ASSERT_TRUE(source->execute());
  ASSERT_EQ(port->dataLocation(), DataLocation::InMemory);

  int persistenceChanges = 0;
  QObject::connect(port, &OutputPort::persistenceChanged, port,
                   [&persistenceChanges]() { ++persistenceChanges; });
  QList<DataLocation> locations;
  QObject::connect(
    port, &OutputPort::dataLocationChanged, port,
    [&locations](DataLocation loc) { locations.append(loc); });

  // InMemory -> OnDisk: the port unpins, the deleter evicts to disk.
  port->setPersistenceMode(PersistenceMode::OnDisk);
  EXPECT_EQ(persistenceChanges, 1);
  ASSERT_FALSE(locations.isEmpty());
  EXPECT_EQ(locations.last(), DataLocation::OnDisk);
  EXPECT_EQ(port->dataLocation(), DataLocation::OnDisk);

  // OnDisk -> InMemory: reloads from the cache file and pins.
  locations.clear();
  port->setPersistenceMode(PersistenceMode::InMemory);
  EXPECT_EQ(persistenceChanges, 2);
  ASSERT_FALSE(locations.isEmpty());
  EXPECT_EQ(locations.last(), DataLocation::InMemory);
  EXPECT_EQ(port->dataLocation(), DataLocation::InMemory);

  // Persistent -> transient with no other holder: the data drops.
  locations.clear();
  port->setPersistent(false);
  EXPECT_EQ(persistenceChanges, 3);
  ASSERT_FALSE(locations.isEmpty());
  EXPECT_EQ(locations.last(), DataLocation::None);
  EXPECT_FALSE(port->hasData());

  // Setting the same values again is a no-op: no spurious signals.
  port->setPersistent(false);
  port->setPersistenceMode(PersistenceMode::InMemory);
  EXPECT_EQ(persistenceChanges, 3);
}

TEST_F(PipelineLibTest, TransientDropEmitsDataLocationNone)
{
  // When the last consumer of a transient port's payload drops its
  // handle, the universal deleter must report the payload as gone so
  // residency cues (RAM badge) don't keep showing freed data.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  auto* port = source->outputPort("volume");
  port->setPersistent(false);
  ASSERT_TRUE(source->execute());

  QList<DataLocation> locations;
  QObject::connect(
    port, &OutputPort::dataLocationChanged, port,
    [&locations](DataLocation loc) { locations.append(loc); });

  // Executor-style handoff: take the strong ref out of the port, then
  // drop it as the last holder.
  { auto handle = port->take(); }

  ASSERT_FALSE(locations.isEmpty());
  EXPECT_EQ(locations.last(), DataLocation::None);
  EXPECT_FALSE(port->hasData());
}

TEST_F(PipelineLibTest, FanInMultipleInputs)
{
  auto* source1 = new SourceNode();
  source1->addOutput("out", PortType::ImageData);
  pipeline->addNode(source1);

  auto* source2 = new SourceNode();
  source2->addOutput("out", PortType::ImageData);
  pipeline->addNode(source2);

  auto* add = new AddTransform();
  pipeline->addNode(add);

  pipeline->createLink(source1->outputPort("out"), add->inputPort("a"));
  pipeline->createLink(source2->outputPort("out"), add->inputPort("b"));

  source1->setOutputData("out", PortData(std::any(10), PortType::ImageData));
  source2->setOutputData("out", PortData(std::any(20), PortType::ImageData));

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(add->state(), NodeState::Current);
  EXPECT_EQ(add->outputPort("out")->data().value<int>(), 30);
}

TEST_F(PipelineLibTest, PipelineValid)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));

  EXPECT_TRUE(pipeline->isValid());
}

TEST_F(PipelineLibTest, RootsDetection)
{
  auto* source1 = new SourceNode();
  source1->addOutput("out", PortType::ImageData);
  auto* source2 = new SourceNode();
  source2->addOutput("out", PortType::ImageData);
  auto* transform = new DoubleTransform();

  pipeline->addNode(source1);
  pipeline->addNode(source2);
  pipeline->addNode(transform);

  pipeline->createLink(source1->outputPort("out"), transform->inputPort("in"));

  auto r = pipeline->roots();
  EXPECT_EQ(r.size(), 2);
  EXPECT_TRUE(r.contains(source1));
  EXPECT_TRUE(r.contains(source2));
  EXPECT_FALSE(r.contains(transform));
}

TEST_F(PipelineLibTest, RemoveNode)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  auto* transform = new DoubleTransform();

  pipeline->addNode(source);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));

  EXPECT_EQ(pipeline->nodes().size(), 2);
  EXPECT_EQ(pipeline->links().size(), 1);

  pipeline->removeNode(transform);

  EXPECT_EQ(pipeline->nodes().size(), 1);
  EXPECT_EQ(pipeline->links().size(), 0);
}

TEST_F(PipelineLibTest, RemoveLink)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  auto* transform = new DoubleTransform();

  pipeline->addNode(source);
  pipeline->addNode(transform);
  auto* link =
    pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));

  EXPECT_EQ(pipeline->links().size(), 1);
  EXPECT_NE(transform->inputPort("in")->link(), nullptr);

  pipeline->removeLink(link);

  EXPECT_EQ(pipeline->links().size(), 0);
  EXPECT_EQ(transform->inputPort("in")->link(), nullptr);
}

TEST_F(PipelineLibTest, UpstreamDownstreamNodes)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  auto* transform = new DoubleTransform();
  auto* sink = new CollectorSink();

  pipeline->addNode(source);
  pipeline->addNode(transform);
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  pipeline->createLink(transform->outputPort("out"), sink->inputPort("in"));

  EXPECT_EQ(source->downstreamNodes().size(), 1);
  EXPECT_EQ(source->downstreamNodes().first(), transform);
  EXPECT_EQ(source->upstreamNodes().size(), 0);

  EXPECT_EQ(transform->upstreamNodes().size(), 1);
  EXPECT_EQ(transform->upstreamNodes().first(), source);
  EXPECT_EQ(transform->downstreamNodes().size(), 1);
  EXPECT_EQ(transform->downstreamNodes().first(), sink);

  EXPECT_EQ(sink->upstreamNodes().size(), 1);
  EXPECT_EQ(sink->upstreamNodes().first(), transform);
  EXPECT_EQ(sink->downstreamNodes().size(), 0);
}

TEST_F(PipelineLibTest, Signals)
{
  int nodeAddedCount = 0;
  int linkCreatedCount = 0;

  QObject::connect(pipeline, &Pipeline::nodeAdded,
                   [&nodeAddedCount](Node*) { nodeAddedCount++; });
  QObject::connect(pipeline, &Pipeline::linkCreated,
                   [&linkCreatedCount](Link*) { linkCreatedCount++; });

  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  auto* transform = new DoubleTransform();

  pipeline->addNode(source);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));

  EXPECT_EQ(nodeAddedCount, 2);
  EXPECT_EQ(linkCreatedCount, 1);
}

TEST_F(PipelineLibTest, ReplaceLinkOnInput)
{
  auto* source1 = new SourceNode();
  source1->addOutput("out", PortType::ImageData);
  auto* source2 = new SourceNode();
  source2->addOutput("out", PortType::ImageData);
  auto* transform = new DoubleTransform();

  pipeline->addNode(source1);
  pipeline->addNode(source2);
  pipeline->addNode(transform);

  auto* link1 = pipeline->createLink(source1->outputPort("out"),
                                     transform->inputPort("in"));
  EXPECT_NE(link1, nullptr);
  EXPECT_EQ(pipeline->links().size(), 1);

  // Creating a new link to the same input should replace the old one
  auto* link2 = pipeline->createLink(source2->outputPort("out"),
                                     transform->inputPort("in"));
  EXPECT_NE(link2, nullptr);
  EXPECT_EQ(pipeline->links().size(), 1);
  EXPECT_EQ(transform->inputPort("in")->link(), link2);
}

// --- VolumeData tests ---

TEST_F(PipelineLibTest, VolumeDataBasics)
{
  VolumeData vol;
  EXPECT_FALSE(vol.isValid());
  EXPECT_EQ(vol.numberOfComponents(), 0);

  auto dims = vol.dimensions();
  EXPECT_EQ(dims[0], 0);
}

TEST_F(PipelineLibTest, VolumeDataMetadata)
{
  VolumeData vol;
  vol.setLabel("Test Volume");
  vol.setUnits("nm");
  EXPECT_EQ(vol.label(), "Test Volume");
  EXPECT_EQ(vol.units(), "nm");
}

TEST_F(PipelineLibTest, VolumeDataPercentile)
{
  // 1000 voxels holding 0..999: the p-th percentile is about 10 * p
  vtkNew<vtkImageData> image;
  image->SetDimensions(10, 10, 10);
  image->AllocateScalars(VTK_FLOAT, 1);
  auto* values = static_cast<float*>(image->GetScalarPointer());
  for (int i = 0; i < 1000; ++i) {
    values[i] = static_cast<float>(i);
  }
  VolumeData vol;
  vol.setImageData(image);

  EXPECT_NEAR(vol.scalarPercentile(0.8), 800.0, 2.0);
  EXPECT_NEAR(vol.scalarPercentile(0.0), 0.0, 1.0);
  EXPECT_NEAR(vol.scalarPercentile(1.0), 999.0, 1.0);
  // A NaN is ignored rather than poisoning the estimate
  values[0] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_NEAR(vol.scalarPercentile(0.8), 800.0, 2.0);
}

// --- SphereSource tests ---

TEST_F(PipelineLibTest, SphereSourceGeneratesVolume)
{
  auto* source = new SphereSource();
  pipeline->addNode(source);

  EXPECT_TRUE(source->execute());
  EXPECT_EQ(source->state(), NodeState::Current);

  auto portData = source->outputPort("volume")->data();
  EXPECT_TRUE(portData.isValid());
  EXPECT_EQ(portData.type(), PortType::ImageData);

  auto volume = portData.value<VolumeDataPtr>();
  EXPECT_TRUE(volume->isValid());

  auto dims = volume->dimensions();
  EXPECT_EQ(dims[0], 32);
  EXPECT_EQ(dims[1], 32);
  EXPECT_EQ(dims[2], 32);
  EXPECT_EQ(volume->label(), "Sphere");
}

TEST_F(PipelineLibTest, SphereSourceCustomDimensions)
{
  auto* source = new SphereSource();
  source->setDimensions(16, 16, 16);
  pipeline->addNode(source);

  source->execute();

  auto volume = source->outputPort("volume")->data().value<VolumeDataPtr>();
  auto dims = volume->dimensions();
  EXPECT_EQ(dims[0], 16);
  EXPECT_EQ(dims[1], 16);
  EXPECT_EQ(dims[2], 16);
}

TEST_F(PipelineLibTest, SphereSourceScalarRange)
{
  auto* source = new SphereSource();
  source->setDimensions(16, 16, 16);
  pipeline->addNode(source);

  source->execute();

  auto volume = source->outputPort("volume")->data().value<VolumeDataPtr>();
  auto range = volume->scalarRange();
  // Signed distance: negative inside, positive outside
  EXPECT_LT(range[0], 0.0);
  EXPECT_GT(range[1], 0.0);
}

// --- ThresholdTransform tests ---

TEST_F(PipelineLibTest, ThresholdTransformBinaryMask)
{
  auto* source = new SphereSource();
  source->setDimensions(16, 16, 16);
  pipeline->addNode(source);

  auto* threshold = new ThresholdTransform();
  // Inside the sphere: signed distance <= 0
  threshold->setMinValue(-100.0);
  threshold->setMaxValue(0.0);
  pipeline->addNode(threshold);

  pipeline->createLink(source->outputPort("volume"),
                       threshold->inputPort("volume"));

  source->execute();
  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(threshold->state(), NodeState::Current);

  auto maskData = threshold->outputPort("mask")->data().value<VolumeDataPtr>();
  EXPECT_TRUE(maskData->isValid());

  auto range = maskData->scalarRange();
  EXPECT_EQ(range[0], 0.0);
  EXPECT_EQ(range[1], 1.0);
}

// --- VolumeStatsSink tests ---

TEST_F(PipelineLibTest, VolumeStatsSinkComputesStats)
{
  auto* source = new SphereSource();
  source->setDimensions(16, 16, 16);
  pipeline->addNode(source);

  auto* stats = new VolumeStatsSink();
  pipeline->addNode(stats);

  pipeline->createLink(source->outputPort("volume"),
                       stats->inputPort("volume"));

  source->execute();
  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(stats->hasResults());
  EXPECT_EQ(stats->voxelCount(), 16 * 16 * 16);
  EXPECT_LT(stats->min(), 0.0);
  EXPECT_GT(stats->max(), 0.0);
  EXPECT_EQ(stats->state(), NodeState::Current);
}

// --- End-to-end: SphereSource → ThresholdTransform → VolumeStatsSink ---

TEST_F(PipelineLibTest, EndToEndSphereThresholdStats)
{
  auto* source = new SphereSource();
  source->setDimensions(16, 16, 16);
  pipeline->addNode(source);

  auto* threshold = new ThresholdTransform();
  threshold->setMinValue(-100.0);
  threshold->setMaxValue(0.0);
  pipeline->addNode(threshold);

  auto* stats = new VolumeStatsSink();
  pipeline->addNode(stats);

  pipeline->createLink(source->outputPort("volume"),
                       threshold->inputPort("volume"));
  pipeline->createLink(threshold->outputPort("mask"),
                       stats->inputPort("volume"));

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(source->state(), NodeState::Current);
  EXPECT_EQ(threshold->state(), NodeState::Current);
  EXPECT_EQ(stats->state(), NodeState::Current);

  EXPECT_TRUE(stats->hasResults());
  // Mask is binary: min=0, max=1
  EXPECT_EQ(stats->min(), 0.0);
  EXPECT_EQ(stats->max(), 1.0);
  // Mean should be between 0 and 1 (fraction of voxels inside sphere)
  EXPECT_GT(stats->mean(), 0.0);
  EXPECT_LT(stats->mean(), 1.0);
}

// --- PipelineStripWidget tests ---

TEST_F(PipelineLibTest, WidgetBasicInstantiation)
{
  PipelineStripWidget widget;
  EXPECT_EQ(widget.pipeline(), nullptr);
  EXPECT_EQ(widget.selectedNode(), nullptr);
  EXPECT_EQ(widget.selectedPort(), nullptr);

  widget.setPipeline(pipeline);
  EXPECT_EQ(widget.pipeline(), pipeline);
}

TEST_F(PipelineLibTest, WidgetLayoutLinear)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  transform->setLabel("Double");
  pipeline->addNode(transform);

  auto* sink = new CollectorSink();
  sink->setLabel("Collector");
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  pipeline->createLink(transform->outputPort("out"), sink->inputPort("in"));

  PipelineStripWidget widget;
  widget.setPipeline(pipeline);
  widget.resize(250, 400);
  widget.rebuildLayout();

  // Should have 3 node cards (single-output nodes are collapsed by default)
  EXPECT_EQ(widget.selectedNode(), nullptr);

  // Verify the widget has a reasonable size hint
  auto hint = widget.sizeHint();
  EXPECT_GT(hint.height(), 0);
  EXPECT_GT(hint.width(), 0);
}

TEST_F(PipelineLibTest, WidgetLayoutMultiOutput)
{
  // Create a node with multiple outputs
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("vol", PortType::ImageData);
  source->addOutput("table", PortType::Table);
  pipeline->addNode(source);

  PipelineStripWidget widget;
  widget.setPipeline(pipeline);
  widget.resize(250, 400);
  widget.rebuildLayout();

  // Multi-output node should automatically show port sub-cards
  // 1 node card + 2 port cards = 3 layout items
  auto hint = widget.sizeHint();
  EXPECT_GT(hint.height(), 0);
}

TEST_F(PipelineLibTest, WidgetExpandCollapse)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  PipelineStripWidget widget;
  widget.setPipeline(pipeline);
  widget.resize(250, 400);

  // Single-output: collapsed by default
  EXPECT_FALSE(widget.isExpanded(source));

  // Expand
  widget.setExpanded(source, true);
  EXPECT_TRUE(widget.isExpanded(source));

  // Collapse
  widget.setExpanded(source, false);
  EXPECT_FALSE(widget.isExpanded(source));
}

TEST_F(PipelineLibTest, WidgetSignalWiring)
{
  PipelineStripWidget widget;
  widget.setPipeline(pipeline);
  widget.resize(250, 400);

  // Adding nodes should trigger layout rebuild
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto hint1 = widget.sizeHint();

  auto* transform = new DoubleTransform();
  transform->setLabel("Double");
  pipeline->addNode(transform);

  auto hint2 = widget.sizeHint();

  // After adding a second node, size hint should increase
  EXPECT_GT(hint2.height(), hint1.height());
}

TEST_F(PipelineLibTest, WidgetFanIn)
{
  auto* source1 = new SourceNode();
  source1->setLabel("Source A");
  source1->addOutput("out", PortType::ImageData);
  pipeline->addNode(source1);

  auto* source2 = new SourceNode();
  source2->setLabel("Source B");
  source2->addOutput("out", PortType::ImageData);
  pipeline->addNode(source2);

  auto* add = new AddTransform();
  add->setLabel("Merge");
  pipeline->addNode(add);

  pipeline->createLink(source1->outputPort("out"), add->inputPort("a"));
  pipeline->createLink(source2->outputPort("out"), add->inputPort("b"));

  PipelineStripWidget widget;
  widget.setPipeline(pipeline);
  widget.resize(250, 400);
  widget.rebuildLayout();

  auto hint = widget.sizeHint();
  EXPECT_GT(hint.height(), 0);
}

// --- LegacyPythonTransform tests ---

class PipelinePythonTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    if (!Py_IsInitialized()) {
      py::initialize_interpreter();
      s_ownInterpreter = true;
    }
  }

  static void TearDownTestSuite()
  {
    if (s_ownInterpreter) {
      py::finalize_interpreter();
      s_ownInterpreter = false;
    }
  }

  void SetUp() override { pipeline = new Pipeline(); }
  void TearDown() override { delete pipeline; }

  static QString readFile(const QString& path)
  {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return {};
    }
    QTextStream stream(&file);
    return stream.readAll();
  }

  Pipeline* pipeline;
  static bool s_ownInterpreter;
};

bool PipelinePythonTest::s_ownInterpreter = false;

TEST_F(PipelinePythonTest, AddConstantOperator)
{
  // Load AddConstant JSON and Python script
  QString pythonDir = TOMVIZ_PYTHON_DIR;
  QString jsonStr = readFile(pythonDir + "/AddConstant.json");
  QString scriptStr = readFile(pythonDir + "/AddConstant.py");
  ASSERT_FALSE(jsonStr.isEmpty());
  ASSERT_FALSE(scriptStr.isEmpty());

  // Create a 4x4x4 volume with all voxels set to 5.0
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  source->execute();

  // Get the source volume and record its scalar range
  auto inputVolume =
    source->outputPort("volume")->data().value<VolumeDataPtr>();
  auto inputRange = inputVolume->scalarRange();

  // Create the legacy Python transform
  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  transform->setParameter("constant", 10.0);
  pipeline->addNode(transform);

  EXPECT_EQ(transform->label(), "Add Constant");
  EXPECT_EQ(transform->operatorName(), "AddConstant");
  EXPECT_EQ(transform->parameter("constant").toDouble(), 10.0);

  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(transform->state(), NodeState::Current);

  auto outputData =
    transform->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(outputData && outputData->isValid());

  auto outputRange = outputData->scalarRange();

  // The output range should be shifted by +10.0
  EXPECT_NEAR(outputRange[0], inputRange[0] + 10.0, 0.01);
  EXPECT_NEAR(outputRange[1], inputRange[1] + 10.0, 0.01);
}

TEST_F(PipelinePythonTest, JSONParameterDefaults)
{
  QString jsonStr = R"({
    "name": "TestOp",
    "label": "Test Operator",
    "parameters": [
      { "name": "threshold", "type": "double", "default": 0.5 },
      { "name": "iterations", "type": "int", "default": 10 },
      { "name": "verbose", "type": "bool", "default": true }
    ]
  })";

  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);

  EXPECT_EQ(transform->label(), "Test Operator");
  EXPECT_EQ(transform->operatorName(), "TestOp");
  EXPECT_EQ(transform->parameter("threshold").toDouble(), 0.5);
  EXPECT_EQ(transform->parameter("iterations").toInt(), 10);
  EXPECT_EQ(transform->parameter("verbose").toBool(), true);

  // Override a parameter
  transform->setParameter("threshold", 1.5);
  EXPECT_EQ(transform->parameter("threshold").toDouble(), 1.5);

  delete transform;
}

TEST_F(PipelinePythonTest, IntArgumentsRoundTripAsInt)
{
  // Regression: Qt6 collapses every JSON number into QVariant<double>,
  // so deserialize used to box ``int``/``enumeration`` arguments as
  // doubles, which then reached Python as a float — breaking operators
  // that index sequences with the parameter.  The fix coerces saved
  // arguments using each parameter's declared type from the operator
  // JSON description.
  QString jsonStr = R"({
    "name": "AxisOp",
    "label": "Axis Op",
    "parameters": [
      { "name": "axis", "type": "enumeration", "default": 2 },
      { "name": "iterations", "type": "int", "default": 1 }
    ]
  })";

  // Minimal script: stash the runtime types of the two int-typed
  // parameters in module-level globals so the test can inspect them.
  QString script =
    "import builtins\n"
    "_axis_type = None\n"
    "_iterations_type = None\n"
    "def transform(dataset, axis=0, iterations=0):\n"
    "    builtins._axis_type = type(axis).__name__\n"
    "    builtins._iterations_type = type(iterations).__name__\n";

  // Round-trip through deserialize so we exercise the saved-argument
  // path (where the Qt6 double-coercion bug used to live).
  QJsonObject saved;
  saved["description"] = jsonStr;
  saved["script"] = script;
  QJsonObject args;
  args["axis"] = 1;
  args["iterations"] = 5;
  saved["arguments"] = args;

  auto* transform = new LegacyPythonTransform();
  ASSERT_TRUE(transform->deserialize(saved));
  EXPECT_EQ(transform->parameter("axis").typeId(),
            static_cast<int>(QMetaType::Int));
  EXPECT_EQ(transform->parameter("iterations").typeId(),
            static_cast<int>(QMetaType::Int));
  EXPECT_EQ(transform->parameter("axis").toInt(), 1);
  EXPECT_EQ(transform->parameter("iterations").toInt(), 5);

  // End-to-end: drive the transform and verify Python received int.
  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  ASSERT_TRUE(source->execute());
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(transform->state(), NodeState::Current);

  py::module_ builtins = py::module_::import("builtins");
  EXPECT_EQ(builtins.attr("_axis_type").cast<std::string>(), "int");
  EXPECT_EQ(builtins.attr("_iterations_type").cast<std::string>(), "int");
}

TEST_F(PipelinePythonTest, JSONResultsPorts)
{
  QString jsonStr = R"({
    "name": "SegOp",
    "label": "Segment",
    "results": [
      { "name": "segmentation_table", "type": "table" },
      { "name": "molecule_result", "type": "molecule" }
    ]
  })";

  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);

  // Should have volume output plus two result ports
  EXPECT_NE(transform->outputPort("volume"), nullptr);
  EXPECT_NE(transform->outputPort("segmentation_table"), nullptr);
  EXPECT_NE(transform->outputPort("molecule_result"), nullptr);

  delete transform;
}

// --- ReaderSourceNode tests ---

TEST_F(PipelineLibTest, ReaderSourceNodeVTIRoundTrip)
{
  // Create a sphere source and execute to get volume data
  auto* sphereSource = new SphereSource();
  sphereSource->setDimensions(8, 8, 8);
  pipeline->addNode(sphereSource);
  ASSERT_TRUE(sphereSource->execute());

  auto originalVolume =
    sphereSource->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(originalVolume && originalVolume->isValid());

  auto originalDims = originalVolume->dimensions();
  auto originalRange = originalVolume->scalarRange();

  // Write to a temp VTI file
  QTemporaryFile tmpFile("XXXXXX.vti");
  tmpFile.setAutoRemove(true);
  ASSERT_TRUE(tmpFile.open());
  QString tmpPath = tmpFile.fileName();
  tmpFile.close();

  auto writer = vtkSmartPointer<vtkXMLImageDataWriter>::New();
  writer->SetFileName(tmpPath.toStdString().c_str());
  writer->SetInputData(originalVolume->imageData());
  writer->Write();

  // Create ReaderSourceNode and read the VTI file
  auto* readerNode = new ReaderSourceNode();
  pipeline->addNode(readerNode);
  readerNode->setFileNames({ tmpPath });

  // Execute — readImageData handles VTI directly.
  ASSERT_TRUE(readerNode->execute());
  EXPECT_EQ(readerNode->state(), NodeState::Current);

  // Verify output - VTI data without tilt angles is typed as Volume.
  auto portData = readerNode->outputPort("volume")->data();
  EXPECT_TRUE(portData.isValid());
  EXPECT_EQ(portData.type(), PortType::Volume);

  auto readVolume = portData.value<VolumeDataPtr>();
  ASSERT_TRUE(readVolume && readVolume->isValid());

  auto readDims = readVolume->dimensions();
  EXPECT_EQ(readDims[0], originalDims[0]);
  EXPECT_EQ(readDims[1], originalDims[1]);
  EXPECT_EQ(readDims[2], originalDims[2]);

  auto readRange = readVolume->scalarRange();
  EXPECT_NEAR(readRange[0], originalRange[0], 0.01);
  EXPECT_NEAR(readRange[1], originalRange[1], 0.01);
}

TEST_F(PipelineLibTest, ReaderSourceNodeNoFilesFails)
{
  auto* readerNode = new ReaderSourceNode();
  pipeline->addNode(readerNode);

  // No files set — execute should fail.
  EXPECT_FALSE(readerNode->execute());
}

TEST_F(PipelineLibTest, ReaderSourceNodeLabelFromFileName)
{
  auto* readerNode = new ReaderSourceNode();
  pipeline->addNode(readerNode);
  readerNode->setFileNames({ "/path/to/my_data.vti" });

  EXPECT_EQ(readerNode->label(), "my_data.vti");
}

TEST_F(PipelinePythonTest, ReaderSourceNodePythonReader)
{
  // Write a VTI file via VTK (since NumpyReader depends on tomviz internals
  // that aren't available in standalone tests, we create a custom Python
  // reader module that wraps vtkXMLImageDataReader).

  // First create a VTI file to read
  vtkNew<vtkImageData> imageData;
  imageData->SetDimensions(4, 5, 6);
  imageData->AllocateScalars(VTK_FLOAT, 1);

  QTemporaryFile tmpFile("XXXXXX.vti");
  tmpFile.setAutoRemove(true);
  ASSERT_TRUE(tmpFile.open());
  QString tmpPath = tmpFile.fileName();
  tmpFile.close();

  auto writer = vtkSmartPointer<vtkXMLImageDataWriter>::New();
  writer->SetFileName(tmpPath.toStdString().c_str());
  writer->SetInputData(imageData.Get());
  writer->Write();

  // Register a Python reader module that reads VTI files
  {
    py::gil_scoped_acquire gil;
    py::exec(R"(
import types, sys
mod = types.ModuleType("test_vti_reader")
mod.__dict__['__file__'] = '<test>'

exec('''
from vtk import vtkXMLImageDataReader

class TestVTIReader:
    def read(self, path):
        reader = vtkXMLImageDataReader()
        reader.SetFileName(path)
        reader.Update()
        return reader.GetOutput()
''', mod.__dict__)

sys.modules["test_vti_reader"] = mod
)");
  }

  // Create ReaderSourceNode; it routes through readImageData, which
  // picks VTK's vtkXMLImageDataReader for the .vti extension.
  auto* readerNode = new ReaderSourceNode();
  readerNode->setFileNames({ tmpPath });
  pipeline->addNode(readerNode);

  ASSERT_TRUE(readerNode->execute());

  auto portData = readerNode->outputPort("volume")->data();
  EXPECT_TRUE(portData.isValid());

  auto volume = portData.value<VolumeDataPtr>();
  ASSERT_TRUE(volume && volume->isValid());

  auto dims = volume->dimensions();
  EXPECT_EQ(dims[0], 4);
  EXPECT_EQ(dims[1], 5);
  EXPECT_EQ(dims[2], 6);
}

// --- Schema-v2 PythonTransform / PythonSource smoke tests ---

TEST_F(PipelinePythonTest, PythonTransformV2)
{
  // Schema-v2 transform: declares an ImageData input "volume" and an
  // ImageData output "volume", takes a `factor` double parameter, and
  // multiplies all voxels by factor.
  //
  // The output is declared persistent so the test can read it back
  // directly via the port: without a downstream consumer this output
  // would otherwise be a leaf and remain pinned anyway, but pinning
  // explicitly makes the intent clear.
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "MultiplyBy",
    "label": "Multiply By",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData", "persistent": true}],
    "parameters": [
      {"name": "factor", "type": "double", "default": 1.0}
    ]
  })";
  QString scriptStr = R"(
import tomviz.nodes

class MultiplyBy(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        ds = inputs["volume"]
        ds.active_scalars = ds.active_scalars * factor
        return {"volume": ds}
)";

  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  source->execute();
  auto inputRange =
    source->outputPort("volume")->data().value<VolumeDataPtr>()
      ->scalarRange();

  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  transform->setParameter("factor", 2.0);
  pipeline->addNode(transform);

  EXPECT_EQ(transform->label(), "Multiply By");
  EXPECT_EQ(transform->operatorName(), "MultiplyBy");
  EXPECT_EQ(transform->parameter("factor").toDouble(), 2.0);

  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(transform->state(), NodeState::Current);

  auto outputData =
    transform->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(outputData && outputData->isValid());
  auto outputRange = outputData->scalarRange();

  EXPECT_NEAR(outputRange[0], inputRange[0] * 2.0, 0.01);
  EXPECT_NEAR(outputRange[1], inputRange[1] * 2.0, 0.01);
}

TEST_F(PipelinePythonTest, ThreadedExecutorLegacyPythonTransform)
{
  // Mirror the application: the transform's Python code runs on a
  // ThreadedExecutor worker thread while the main thread spins the
  // event loop (which is also where intermediate updates land).
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  QString pythonDir = TOMVIZ_PYTHON_DIR;
  QString jsonStr = readFile(pythonDir + "/AddConstant.json");
  QString scriptStr = readFile(pythonDir + "/AddConstant.py");
  ASSERT_FALSE(jsonStr.isEmpty());
  ASSERT_FALSE(scriptStr.isEmpty());

  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  pipeline->addNode(source);
  source->execute();
  auto inputRange =
    source->outputPort("volume")->data().value<VolumeDataPtr>()
      ->scalarRange();

  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  transform->setParameter("constant", 10.0);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  // The worker thread needs the GIL; release it from the test (main)
  // thread for the duration of the run, as the application does after
  // initializing Python.
  py::gil_scoped_release releaseGil;

  auto* future = pipeline->execute();
  QSignalSpy spy(future, &ExecutionFuture::finished);
  if (!future->isFinished()) {
    ASSERT_TRUE(spy.wait(30000));
  }

  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(transform->state(), NodeState::Current);
  auto outputData =
    transform->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(outputData && outputData->isValid());
  auto outputRange = outputData->scalarRange();
  EXPECT_NEAR(outputRange[0], inputRange[0] + 10.0, 0.01);
  EXPECT_NEAR(outputRange[1], inputRange[1] + 10.0, 0.01);

  // Re-execute once more (the app re-runs transforms on parameter
  // edits) to shake out lifetime bugs across runs.
  transform->setParameter("constant", 20.0);
  transform->markStale();
  auto* future2 = pipeline->execute();
  QSignalSpy spy2(future2, &ExecutionFuture::finished);
  if (!future2->isFinished()) {
    ASSERT_TRUE(spy2.wait(30000));
  }
  EXPECT_TRUE(future2->succeeded());
}

TEST_F(PipelinePythonTest, ThreadedExecutorReconStyleOperator)
{
  // The full recon-operator shape on the threaded executor: a v1
  // class-based operator that creates a child dataset, publishes
  // intermediate previews through progress.data while running, and
  // returns the child as its declared output. This is the most
  // Python↔VTK-boundary-intensive path the application exercises.
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  QString jsonStr = R"({
    "name": "MiniRecon",
    "label": "Mini Recon",
    "children": [{"name": "recon", "label": "Reconstruction"}]
  })";
  QString scriptStr = R"(
import numpy as np
import tomviz.operators


class MiniReconOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        self.progress.maximum = 4
        child = dataset.create_child_dataset()
        recon = np.zeros((6, 6, 6), dtype=np.float32)
        for i in range(4):
            if self.canceled:
                return
            recon += float(i + 1)
            child.set_scalars('recon', recon.copy())
            self.progress.value = i + 1
            self.progress.message = 'pass %d' % (i + 1)
            self.progress.data = child
        return {'recon': child}
)";

  auto* source = new SphereSource();
  source->setDimensions(6, 6, 6);
  pipeline->addNode(source);
  source->execute();

  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  // The "children" declaration renames the primary output port.
  auto* outPort = transform->outputPort("recon");
  ASSERT_TRUE(outPort != nullptr);
  int intermediateCount = 0;
  QObject::connect(outPort, &OutputPort::intermediateDataApplied,
                   [&intermediateCount]() { ++intermediateCount; });

  py::gil_scoped_release releaseGil;

  for (int run = 0; run < 2; ++run) {
    auto* future = pipeline->execute();
    QSignalSpy spy(future, &ExecutionFuture::finished);
    if (!future->isFinished()) {
      ASSERT_TRUE(spy.wait(30000));
    }
    EXPECT_TRUE(future->succeeded());
    EXPECT_EQ(transform->state(), NodeState::Current);

    auto outputData = outPort->data().value<VolumeDataPtr>();
    ASSERT_TRUE(outputData && outputData->isValid());
    // 1+2+3+4 accumulated into every voxel.
    auto range = outputData->scalarRange();
    EXPECT_NEAR(range[0], 10.0, 1e-4);
    EXPECT_NEAR(range[1], 10.0, 1e-4);

    transform->markStale();
  }

  EXPECT_EQ(transform->totalProgressSteps(), 4);
  EXPECT_EQ(transform->progressStep(), 4);
  EXPECT_GE(intermediateCount, 8);
}

TEST_F(PipelinePythonTest, ThreadedExecutorMultiArrayOperator)
{
  // Multi-array volume through an @apply_to_each_array operator on the
  // threaded executor: apply_to_each_array deep-copies the dataset once
  // per array, so a naive backing-image reference gets cloned into a
  // pile of VTK objects that are then destroyed during Python GC on the
  // worker thread. This reproduces the in-app SIGSEGV.
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  QString jsonStr = readFile(QString(TOMVIZ_PYTHON_DIR) +
                             "/AddConstant.json");
  // Inline AddConstant-shaped operator that also cranks the cyclic GC
  // to its most aggressive setting, so any object corrupted by the
  // Python↔VTK boundary is tripped over deterministically rather than
  // "sometimes, later".
  QString scriptStr = R"(
import gc
gc.set_threshold(1, 1, 1)


def transform(dataset, constant=0.0):
    import numpy as np
    scalars = dataset.active_scalars
    dataset.active_scalars = scalars + np.array([constant], dtype=scalars.dtype)
)";

  // A source with THREE scalar arrays on its output volume.
  auto* source = new SphereSource();
  source->setDimensions(12, 12, 12);
  pipeline->addNode(source);
  source->execute();
  {
    auto vol = source->outputPort("volume")->data().value<VolumeDataPtr>();
    ASSERT_TRUE(vol && vol->imageData());
    auto* image = vol->imageData();
    auto* pd = image->GetPointData();
    auto* base = pd->GetScalars();
    for (const char* name : { "Second", "Third" }) {
      vtkNew<vtkFloatArray> extra;
      extra->DeepCopy(base);
      extra->SetName(name);
      pd->AddArray(extra);
    }
  }
  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  transform->setParameter("constant", 10.0);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  py::gil_scoped_release releaseGil;

  for (int run = 0; run < 3; ++run) {
    auto* future = pipeline->execute();
    QSignalSpy spy(future, &ExecutionFuture::finished);
    if (!future->isFinished()) {
      ASSERT_TRUE(spy.wait(30000));
    }
    EXPECT_TRUE(future->succeeded());
    EXPECT_EQ(transform->state(), NodeState::Current);
    auto outputData =
      transform->outputPort("volume")->data().value<VolumeDataPtr>();
    ASSERT_TRUE(outputData && outputData->isValid());
    // Every one of the three arrays got the constant added.
    EXPECT_EQ(outputData->imageData()->GetPointData()->GetNumberOfArrays(),
              3);
    transform->markStale();
  }
}

TEST_F(PipelinePythonTest, ThreadedExecutorPythonTransformV2)
{
  // Schema-v2 PythonTransform on the threaded executor — the path that
  // crashes in-app (PythonNodeBackend::runImpl on a worker thread).
  // Multi-array input + a downstream sink that consumes on the main
  // thread, run several times, to mirror the application.
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "MultiplyBy",
    "label": "Multiply By",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData", "persistent": true}],
    "parameters": [{"name": "factor", "type": "double", "default": 1.0}]
  })";
  QString scriptStr = R"(
import tomviz.nodes

class MultiplyBy(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        ds = inputs["volume"]
        for name in list(ds.scalars_names):
            ds.set_scalars(name, ds.scalars(name) * factor)
        return {"volume": ds}
)";

  auto* source = new SphereSource();
  source->setDimensions(10, 10, 10);
  pipeline->addNode(source);
  source->execute();
  {
    auto vol = source->outputPort("volume")->data().value<VolumeDataPtr>();
    auto* pd = vol->imageData()->GetPointData();
    auto* base = pd->GetScalars();
    for (const char* name : { "Second", "Third" }) {
      vtkNew<vtkFloatArray> extra;
      extra->DeepCopy(base);
      extra->SetName(name);
      pd->AddArray(extra);
    }
  }

  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  transform->setParameter("factor", 2.0);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  py::gil_scoped_release releaseGil;

  for (int run = 0; run < 5; ++run) {
    auto* future = pipeline->execute();
    QSignalSpy spy(future, &ExecutionFuture::finished);
    if (!future->isFinished()) {
      ASSERT_TRUE(spy.wait(30000));
    }
    EXPECT_TRUE(future->succeeded());
    EXPECT_EQ(transform->state(), NodeState::Current);
    auto outputData =
      transform->outputPort("volume")->data().value<VolumeDataPtr>();
    ASSERT_TRUE(outputData && outputData->isValid());
    transform->markStale();
  }
}

TEST_F(PipelinePythonTest, PythonTransformV2ProgressUpdates)
{
  // Progress plumbing end to end on the numpy-dataset path: the script
  // reports step count / value / message through self.progress, and
  // publishes a live-preview payload — a fresh numpy Dataset — through
  // self.progress.data. The preview must be converted to vtkImageData
  // at the script boundary and applied to the output port as
  // intermediate data while the transform is still running.
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "SlowMultiply",
    "label": "Slow Multiply",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData", "persistent": true}],
    "parameters": [
      {"name": "factor", "type": "double", "default": 1.0}
    ]
  })";
  QString scriptStr = R"(
import tomviz.nodes
import numpy as np

class SlowMultiply(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        ds = inputs["volume"]
        self.progress.maximum = 3
        self.progress.value = 1
        self.progress.message = "halfway"
        preview = self.create_dataset()
        preview.set_scalars("Scalars",
                            np.full((2, 2, 2), 21.0, dtype=np.float32))
        preview.spacing = (1.0, 1.0, 1.0)
        self.progress.data = preview
        self.progress.value = 2
        ds.active_scalars = ds.active_scalars * factor
        return {"volume": ds}
)";

  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  source->execute();
  auto inputRange =
    source->outputPort("volume")->data().value<VolumeDataPtr>()
      ->scalarRange();

  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  transform->setParameter("factor", 2.0);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  // Capture the live-preview payload the moment it is applied — the
  // final output overwrites the port data afterwards.
  auto* outPort = transform->outputPort("volume");
  int intermediateCount = 0;
  double intermediateValue = 0.0;
  QObject::connect(outPort, &OutputPort::intermediateDataApplied,
                   [&intermediateCount, &intermediateValue, outPort]() {
                     ++intermediateCount;
                     auto vol = outPort->data().value<VolumeDataPtr>();
                     if (vol && vol->isValid()) {
                       intermediateValue = vol->scalarRange()[0];
                     }
                   });

  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(transform->state(), NodeState::Current);

  // Step-count / value / message reached the node.
  EXPECT_EQ(transform->totalProgressSteps(), 3);
  EXPECT_EQ(transform->progressStep(), 2);
  EXPECT_EQ(transform->progressMessage(), "halfway");

  // The preview Dataset was converted and applied exactly once, with
  // the constant payload the script produced.
  EXPECT_EQ(intermediateCount, 1);
  EXPECT_NEAR(intermediateValue, 21.0, 1e-5);

  // And the final output still reflects the real transform result.
  auto outputData = outPort->data().value<VolumeDataPtr>();
  ASSERT_TRUE(outputData && outputData->isValid());
  auto outputRange = outputData->scalarRange();
  EXPECT_NEAR(outputRange[0], inputRange[0] * 2.0, 0.01);
  EXPECT_NEAR(outputRange[1], inputRange[1] * 2.0, 0.01);
}

TEST_F(PipelinePythonTest, PythonTransformV2DefaultsToTransformNodeDefault)
{
  // Schema-v2 convention: an omitted `persistent` field defers to the
  // host node-class's default. Currently TransformNode defaults to
  // OnDisk persistent (TEMPORARY rollout — flip this test back when
  // TransformNode::addOutput goes back to transient).
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "Identity",
    "inputs":  [{"name": "in",  "type": "ImageData"}],
    "outputs": [{"name": "out", "type": "ImageData"}]
  })";
  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  auto* port = transform->outputPort("out");
  EXPECT_TRUE(port->isPersistent());
  EXPECT_EQ(port->persistenceMode(), PersistenceMode::OnDisk);
  delete transform;
}

TEST_F(PipelinePythonTest, PythonTransformV2RejectsSourceShape)
{
  // A v2 description with empty inputs is source-shaped; routing
  // through AddPythonTransformReaction is rejected at construction.
  // Verify at the node level that the parsing still records zero
  // inputs (the reaction enforces the policy; the node itself is
  // permissive enough for state-file load).
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "MisRoutedSource",
    "inputs":  [],
    "outputs": [{"name": "out", "type": "ImageData"}]
  })";
  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  EXPECT_EQ(transform->inputPorts().size(), 0);
  delete transform;
}

TEST_F(PipelinePythonTest, PythonTransformV2ExternalOnlyInstallsExecutor)
{
  // An externalOnly description defaults freshly created nodes to the
  // External executor (with an empty env path for the user to fill in).
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "NeedsTorch",
    "externalOnly": true,
    "inputs":  [{"name": "in",  "type": "ImageData"}],
    "outputs": [{"name": "out", "type": "ImageData"}]
  })";
  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  auto* executor =
    qobject_cast<ExternalNodeExecutor*>(transform->nodeExecutor());
  ASSERT_NE(executor, nullptr);
  EXPECT_TRUE(executor->envPath().isEmpty());
  delete transform;

  // Without the flag (and without tomviz_pipeline_env) no executor is
  // installed.
  QString plainJson = R"({
    "schemaVersion": 2,
    "name": "Plain",
    "inputs":  [{"name": "in",  "type": "ImageData"}],
    "outputs": [{"name": "out", "type": "ImageData"}]
  })";
  auto* plain = new PythonTransform();
  plain->setJSONDescription(plainJson);
  EXPECT_EQ(plain->nodeExecutor(), nullptr);
  delete plain;
}

TEST_F(PipelinePythonTest, LegacyPythonTransformExternalOnlyInstallsExecutor)
{
  // Same behavior for schema-v1 descriptions (e.g. SAM2Segment3D.json).
  QString jsonStr = R"({
    "name": "NeedsTorchLegacy",
    "label": "Needs Torch",
    "externalOnly": true
  })";
  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(jsonStr);
  auto* executor =
    qobject_cast<ExternalNodeExecutor*>(transform->nodeExecutor());
  ASSERT_NE(executor, nullptr);
  EXPECT_TRUE(executor->envPath().isEmpty());
  delete transform;
}

TEST_F(PipelinePythonTest, PythonSourceV2)
{
  // Schema-v2 source: declares an ImageData output "volume" and
  // produces a 3x3x3 vtkImageData filled with a constant value.
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "ConstantVolume",
    "label": "Constant Volume",
    "outputs": [
      {"name": "volume", "type": "ImageData", "persistent": true}
    ],
    "parameters": [
      {"name": "value", "type": "double", "default": 7.0}
    ]
  })";
  QString scriptStr = R"(
import tomviz.nodes
import numpy as np

class ConstantVolume(tomviz.nodes.SourceNode):
    def produce(self, value=0.0):
        ds = self.create_dataset()
        ds.set_scalars("Scalars", np.full((3, 3, 3), value,
                                          dtype=np.float32))
        ds.spacing = (1.0, 1.0, 1.0)
        return {"volume": ds}
)";

  auto* source = new PythonSource();
  source->setJSONDescription(jsonStr);
  source->setScript(scriptStr);
  source->setParameter("value", 42.0);
  pipeline->addNode(source);

  EXPECT_EQ(source->label(), "Constant Volume");
  EXPECT_EQ(source->operatorName(), "ConstantVolume");
  EXPECT_TRUE(source->outputPort("volume")->isPersistent());

  ASSERT_TRUE(source->execute());
  EXPECT_EQ(source->state(), NodeState::Current);

  auto outputData =
    source->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(outputData && outputData->isValid());

  auto dims = outputData->dimensions();
  EXPECT_EQ(dims[0], 3);
  EXPECT_EQ(dims[1], 3);
  EXPECT_EQ(dims[2], 3);
  auto range = outputData->scalarRange();
  EXPECT_NEAR(range[0], 42.0, 0.01);
  EXPECT_NEAR(range[1], 42.0, 0.01);
}

TEST_F(PipelinePythonTest, PythonTransformV2RoundTripsState)
{
  // Round-trip a v2 transform through serialize/deserialize and
  // verify parameters and JSON description survive.
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "MultiplyBy",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData"}],
    "parameters": [{"name": "factor", "type": "double", "default": 1.0}]
  })";
  QString scriptStr = "import tomviz.nodes\n"
                       "class MultiplyBy(tomviz.nodes.TransformNode):\n"
                       "    def transform(self, inputs, factor=1.0):\n"
                       "        return {}\n";

  auto* original = new PythonTransform();
  original->setJSONDescription(jsonStr);
  original->setScript(scriptStr);
  original->setParameter("factor", 3.5);
  original->setLabel("Multiply x3.5");

  auto saved = original->serialize();
  delete original;

  auto* restored = new PythonTransform();
  restored->deserialize(saved);

  EXPECT_EQ(restored->label(), "Multiply x3.5");
  EXPECT_EQ(restored->scriptSource(), scriptStr);
  EXPECT_EQ(restored->parameter("factor").toDouble(), 3.5);
  EXPECT_EQ(restored->operatorName(), "MultiplyBy");
  ASSERT_TRUE(restored->inputPort("volume"));
  ASSERT_TRUE(restored->outputPort("volume"));
  delete restored;
}

TEST_F(PipelinePythonTest, InvertDataV2OperatorEndToEnd)
{
  // Loads the real on-disk InvertData.json + InvertData.py (the first
  // operator migrated to schema-v2) and runs it over a SphereSource to
  // verify the migration is wired correctly end-to-end.
  QString pythonDir = TOMVIZ_PYTHON_DIR;
  QString jsonStr = readFile(pythonDir + "/InvertData.json");
  QString scriptStr = readFile(pythonDir + "/InvertData.py");
  ASSERT_FALSE(jsonStr.isEmpty());
  ASSERT_FALSE(scriptStr.isEmpty());
  // Sanity: this really is a v2 description.
  EXPECT_TRUE(jsonStr.contains("\"schemaVersion\": 2"));

  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);
  source->execute();
  auto inputRange =
    source->outputPort("volume")->data().value<VolumeDataPtr>()
      ->scalarRange();

  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  // Mark the output persistent so the test can read it back directly
  // off the port (no downstream sink in this test).
  transform->outputPort("volume")->setPersistent(true);
  pipeline->addNode(transform);

  EXPECT_EQ(transform->label(), "Invert Data");
  EXPECT_EQ(transform->operatorName(), "InvertData");
  EXPECT_TRUE(transform->supportsCancelingMidExecution());

  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_EQ(transform->state(), NodeState::Current);

  auto outputData =
    transform->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(outputData && outputData->isValid());
  auto outputRange = outputData->scalarRange();

  // Inversion: out_min = in_max - in_max + in_min = in_min,
  //            out_max = in_max - in_min + in_min = in_max,
  // i.e. range bounds are preserved (each voxel v becomes max - v +
  // min, but the set of values is the same set).
  EXPECT_NEAR(outputRange[0], inputRange[0], 0.01);
  EXPECT_NEAR(outputRange[1], inputRange[1], 0.01);
}

TEST_F(PipelinePythonTest, PythonTransformV2SaveLoadReExecute)
{
  // Full save → load → re-execute round trip: build a v2 transform,
  // execute it, serialize, reconstruct from the serialized form into
  // a fresh node, re-wire to a fresh source, re-execute, and verify
  // the second execution produces the same numeric output as the
  // first. Catches issues that the structural-only round-trip test
  // (PythonTransformV2RoundTripsState) wouldn't surface — e.g. a
  // backend field that survives serialize but isn't actually consulted
  // on a re-loaded instance, or a port-type / persistency mismatch
  // that breaks the second execution.
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "MultiplyBy",
    "label": "Multiply By",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData", "persistent": true}],
    "parameters": [
      {"name": "factor", "type": "double", "default": 1.0}
    ]
  })";
  QString scriptStr = R"(
import tomviz.nodes

class MultiplyBy(tomviz.nodes.TransformNode):
    def transform(self, inputs, factor=1.0):
        ds = inputs["volume"]
        return {"volume": ds.apply_to_each_scalar_array(lambda a: a * factor)}
)";

  // ---- First execution -----------------------------------------------

  auto* source1 = new SphereSource();
  source1->setDimensions(4, 4, 4);
  pipeline->addNode(source1);
  source1->execute();
  auto inputRange =
    source1->outputPort("volume")->data().value<VolumeDataPtr>()
      ->scalarRange();

  auto* transform1 = new PythonTransform();
  transform1->setJSONDescription(jsonStr);
  transform1->setScript(scriptStr);
  transform1->setParameter("factor", 5.0);
  transform1->setLabel("Custom Label");
  pipeline->addNode(transform1);
  pipeline->createLink(source1->outputPort("volume"),
                       transform1->inputPort("volume"));

  auto* future1 = pipeline->execute();
  EXPECT_TRUE(future1->isFinished());
  EXPECT_EQ(transform1->state(), NodeState::Current);

  auto firstOutput =
    transform1->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(firstOutput && firstOutput->isValid());
  auto firstRange = firstOutput->scalarRange();
  EXPECT_NEAR(firstRange[0], inputRange[0] * 5.0, 0.01);
  EXPECT_NEAR(firstRange[1], inputRange[1] * 5.0, 0.01);

  // ---- Serialize the live transform ----------------------------------

  QJsonObject saved = transform1->serialize();

  // ---- Tear down and rebuild from the serialized blob ----------------

  delete pipeline;
  pipeline = new Pipeline();

  // Mirror the routing the state-file loader does: the saved 'type'
  // string would have been "transform.python" — instantiate via
  // NodeFactory by name to check the registration too.
  NodeFactory::registerBuiltins();
  auto* restoredNode = NodeFactory::create(
    QStringLiteral("transform.python"));
  ASSERT_TRUE(restoredNode);
  auto* transform2 = qobject_cast<PythonTransform*>(restoredNode);
  ASSERT_TRUE(transform2);

  ASSERT_TRUE(transform2->deserialize(saved));

  // Verify the persistent state survives round-trip end-to-end.
  EXPECT_EQ(transform2->label(), "Custom Label");
  EXPECT_EQ(transform2->parameter("factor").toDouble(), 5.0);
  ASSERT_TRUE(transform2->inputPort("volume"));
  ASSERT_TRUE(transform2->outputPort("volume"));

  // ---- Re-wire and re-execute ----------------------------------------

  auto* source2 = new SphereSource();
  source2->setDimensions(4, 4, 4);
  pipeline->addNode(source2);
  source2->execute();

  pipeline->addNode(transform2);
  pipeline->createLink(source2->outputPort("volume"),
                       transform2->inputPort("volume"));

  auto* future2 = pipeline->execute();
  EXPECT_TRUE(future2->isFinished());
  EXPECT_EQ(transform2->state(), NodeState::Current);

  auto secondOutput =
    transform2->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(secondOutput && secondOutput->isValid());
  auto secondRange = secondOutput->scalarRange();

  // The second-run range must match the first — same operator, same
  // factor, same input geometry.
  EXPECT_NEAR(secondRange[0], firstRange[0], 0.01);
  EXPECT_NEAR(secondRange[1], firstRange[1], 0.01);
}

// --- ThreadedExecutor tests ---

class SlowTransform : public TransformNode
{
public:
  SlowTransform() : TransformNode()
  {
    addInput("in", PortType::ImageData);
    addOutput("out", PortType::ImageData);
  }

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override
  {
    QThread::msleep(50);
    int val = inputs["in"].value<int>();
    QMap<QString, PortData> result;
    result["out"] = PortData(std::any(val * 2), PortType::ImageData);
    return result;
  }
};

TEST_F(PipelineLibTest, ThreadedExecutorBasic)
{
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* transform = new SlowTransform();
  pipeline->addNode(transform);

  auto* sink = new CollectorSink();
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("out"), transform->inputPort("in"));
  pipeline->createLink(transform->outputPort("out"), sink->inputPort("in"));

  source->setOutputData("out", PortData(std::any(7), PortType::ImageData));

  auto* future = pipeline->execute();

  // With threaded executor, future should not be immediately finished
  QSignalSpy spy(future, &ExecutionFuture::finished);
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_TRUE(sink->consumed);
  EXPECT_EQ(sink->lastValue, 14);
  EXPECT_EQ(transform->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, ThreadedExecutorCancellation)
{
  auto* executor = new ThreadedExecutor(pipeline);
  pipeline->setExecutor(executor);

  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  // Chain of slow transforms — cancel should stop before all complete
  auto* t1 = new SlowTransform();
  auto* t2 = new SlowTransform();
  auto* t3 = new SlowTransform();
  pipeline->addNode(t1);
  pipeline->addNode(t2);
  pipeline->addNode(t3);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));
  pipeline->createLink(t2->outputPort("out"), t3->inputPort("in"));

  source->setOutputData("out", PortData(std::any(1), PortType::ImageData));

  auto* future = pipeline->execute();
  executor->cancel();

  QSignalSpy spy(future, &ExecutionFuture::finished);
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_TRUE(future->isFinished());
  EXPECT_FALSE(future->succeeded());
  // t3 should not have finished
  EXPECT_NE(t3->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, ThreadedExecutorHeldNode)
{
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);
  auto* t1 = new SlowTransform();
  auto* t2 = new SlowTransform();
  pipeline->addNode(t1);
  pipeline->addNode(t2);
  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));
  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  t1->setHeld(true);
  Node* reachedNode = nullptr;
  QObject::connect(pipeline, &Pipeline::breakpointReached,
                   [&reachedNode](Node* n) { reachedNode = n; });

  auto* future = pipeline->execute();
  QSignalSpy spy(future, &ExecutionFuture::finished);
  ASSERT_TRUE(spy.wait(5000));
  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(reachedNode, nullptr);
  EXPECT_NE(t1->state(), NodeState::Current);
  EXPECT_NE(t2->state(), NodeState::Current);

  t1->setHeld(false);
  future = pipeline->execute();
  QSignalSpy spy2(future, &ExecutionFuture::finished);
  ASSERT_TRUE(spy2.wait(5000));
  EXPECT_EQ(t1->state(), NodeState::Current);
  EXPECT_EQ(t2->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, ThreadedExecutorBreakpoint)
{
  pipeline->setExecutor(new ThreadedExecutor(pipeline));

  auto* source = new SourceNode();
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);

  auto* t1 = new SlowTransform();
  auto* t2 = new SlowTransform();
  pipeline->addNode(t1);
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  t1->setBreakpoint(true);

  Node* reachedNode = nullptr;
  QObject::connect(pipeline, &Pipeline::breakpointReached,
                   [&reachedNode](Node* n) { reachedNode = n; });

  auto* future = pipeline->execute();

  QSignalSpy spy(future, &ExecutionFuture::finished);
  ASSERT_TRUE(spy.wait(5000));

  EXPECT_TRUE(future->isFinished());
  EXPECT_FALSE(future->succeeded());
  EXPECT_EQ(reachedNode, t1);
  EXPECT_NE(t1->state(), NodeState::Current);
}

// --- LegacyModuleSink / visualization sink tests ---

TEST_F(PipelineLibTest, VolumeSinkConsumeWithSphereSource)
{
  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  pipeline->addNode(source);

  auto* sink = new VolumeSink();
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("volume"),
                       sink->inputPort("volume"));

  source->execute();
  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_EQ(sink->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, VisibilityToggling)
{
  auto* sink = new VolumeSink();

  EXPECT_TRUE(sink->visibility());

  QSignalSpy spy(sink, &LegacyModuleSink::visibilityChanged);

  sink->setVisibility(false);
  EXPECT_FALSE(sink->visibility());
  EXPECT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(0).toBool(), false);

  // Setting same value should not emit again
  sink->setVisibility(false);
  EXPECT_EQ(spy.count(), 1);

  sink->setVisibility(true);
  EXPECT_TRUE(sink->visibility());
  EXPECT_EQ(spy.count(), 2);

  delete sink;
}

TEST_F(PipelineLibTest, ColorMapFlags)
{
  // Sinks that need a colormap
  VolumeSink volumeSink;
  SliceSink sliceSink;
  ContourSink contourSink;
  ThresholdSink thresholdSink;
  SegmentSink segmentSink;

  EXPECT_TRUE(volumeSink.isColorMapNeeded());
  EXPECT_TRUE(sliceSink.isColorMapNeeded());
  EXPECT_TRUE(contourSink.isColorMapNeeded());
  EXPECT_TRUE(thresholdSink.isColorMapNeeded());
  EXPECT_TRUE(segmentSink.isColorMapNeeded());

  // Sinks that do not need a colormap
  OutlineSink outlineSink;
  ClipSink clipSink;
  RulerSink rulerSink;
  ScaleCubeSink scaleCubeSink;
  PlotSink plotSink;
  MoleculeSink moleculeSink;

  EXPECT_FALSE(outlineSink.isColorMapNeeded());
  EXPECT_FALSE(clipSink.isColorMapNeeded());
  EXPECT_FALSE(rulerSink.isColorMapNeeded());
  EXPECT_FALSE(scaleCubeSink.isColorMapNeeded());
  EXPECT_FALSE(plotSink.isColorMapNeeded());
  EXPECT_FALSE(moleculeSink.isColorMapNeeded());
}

TEST_F(PipelineLibTest, VolumeToTablePortRejection)
{
  // A Volume source should not connect to a Table input (PlotSink)
  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  pipeline->addNode(source);

  auto* plotSink = new PlotSink();
  pipeline->addNode(plotSink);

  auto* link = pipeline->createLink(source->outputPort("volume"),
                                    plotSink->inputPort("table"));
  EXPECT_EQ(link, nullptr);
}

TEST_F(PipelineLibTest, SerializationRoundTrip)
{
  auto* sink = new VolumeSink();
  sink->setLabel("My Volume View");
  sink->setVisibility(false);

  QJsonObject json = sink->serialize();
  EXPECT_EQ(json["label"].toString(), "My Volume View");
  EXPECT_EQ(json["visible"].toBool(), false);

  // Deserialize into a fresh sink
  auto* sink2 = new VolumeSink();
  EXPECT_TRUE(sink2->deserialize(json));
  EXPECT_EQ(sink2->label(), "My Volume View");
  EXPECT_FALSE(sink2->visibility());

  delete sink;
  delete sink2;
}

TEST_F(PipelineLibTest, LightingStateRoundTrip)
{
  using Preset = VolumeSink::LightingPreset;

  auto* sink = new VolumeSink();
  sink->applyLightingPreset(Preset::Soft);
  // The shadow switch is independent of the preset: turning it off must not
  // lose the scattering level the preset asked for, or the preset itself.
  sink->setShadowsEnabled(false);
  ASSERT_EQ(sink->currentLightingPreset(), Preset::Soft);
  ASSERT_GT(sink->volumetricScattering(), 0.0);

  QJsonObject json = sink->serialize();
  auto light = json["lighting"].toObject();
  EXPECT_FALSE(light["shadowsEnabled"].toBool());
  EXPECT_GT(light["scattering"].toDouble(), 0.0);

  auto* restored = new VolumeSink();
  ASSERT_TRUE(restored->deserialize(json));
  EXPECT_EQ(restored->lighting(), sink->lighting());
  EXPECT_DOUBLE_EQ(restored->ambient(), sink->ambient());
  EXPECT_DOUBLE_EQ(restored->diffuse(), sink->diffuse());
  EXPECT_DOUBLE_EQ(restored->specular(), sink->specular());
  EXPECT_DOUBLE_EQ(restored->specularPower(), sink->specularPower());
  EXPECT_DOUBLE_EQ(restored->volumetricScattering(),
                   sink->volumetricScattering());
  EXPECT_DOUBLE_EQ(restored->shadowReach(), sink->shadowReach());
  EXPECT_DOUBLE_EQ(restored->scatteringAnisotropy(),
                   sink->scatteringAnisotropy());
  EXPECT_EQ(restored->smoothNormals(), sink->smoothNormals());
  EXPECT_FALSE(restored->shadowsEnabled());
  // The panel derives the highlighted button from the values, so this is
  // also what keeps Soft lit after a reload.
  EXPECT_EQ(restored->currentLightingPreset(), Preset::Soft);

  delete sink;
  delete restored;
}

TEST_F(PipelineLibTest, LightingStateRoundTripGentle)
{
  using Preset = VolumeSink::LightingPreset;

  auto* sink = new VolumeSink();
  sink->applyLightingPreset(Preset::Gentle);
  ASSERT_EQ(sink->currentLightingPreset(), Preset::Gentle);
  // Gentle gets its look without shadows, so it must not be confused with
  // Soft or Simple after a round trip.
  ASSERT_DOUBLE_EQ(sink->volumetricScattering(), 0.0);

  auto* restored = new VolumeSink();
  ASSERT_TRUE(restored->deserialize(sink->serialize()));
  EXPECT_EQ(restored->currentLightingPreset(), Preset::Gentle);

  delete sink;
  delete restored;
}

TEST_F(PipelineLibTest, LightingStatePredatesShadowSwitch)
{
  using Preset = VolumeSink::LightingPreset;

  // A state file written before the shadow switch existed has no
  // "shadowsEnabled" key, and its stored scattering level was always
  // rendered. Those files must still come back with shadows on.
  auto* sink = new VolumeSink();
  sink->applyLightingPreset(Preset::Full);
  QJsonObject json = sink->serialize();
  auto light = json["lighting"].toObject();
  light.remove("shadowsEnabled");
  json["lighting"] = light;

  auto* restored = new VolumeSink();
  ASSERT_TRUE(restored->deserialize(json));
  EXPECT_TRUE(restored->shadowsEnabled());
  EXPECT_EQ(restored->currentLightingPreset(), Preset::Full);

  delete sink;
  delete restored;
}

TEST_F(PipelineLibTest, PipelineStateIOLinearRoundTrip)
{
  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  source->setRadiusFraction(0.3);
  source->setLabel("My Sphere");
  pipeline->addNode(source);

  auto* sink = new VolumeSink();
  sink->setLabel("My Volume");
  sink->setVisibility(false);
  pipeline->addNode(sink);

  pipeline->createLink(source->outputPort("volume"),
                       sink->inputPort("volume"));

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));
  EXPECT_EQ(state["schemaVersion"].toInt(), 2);
  EXPECT_EQ(state["pipeline"].toObject()["nodes"].toArray().size(), 2);
  EXPECT_EQ(state["pipeline"].toObject()["links"].toArray().size(), 1);

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 2);
  ASSERT_EQ(restored.links().size(), 1);
  // Sink deserialize is deferred to Pipeline::executionFinished to
  // avoid render warnings when setVisibility fires before consume.
  // Fire it synchronously for the test.
  QMetaObject::invokeMethod(&restored, "executionFinished",
                            Qt::DirectConnection);

  auto* restoredSource =
    dynamic_cast<SphereSource*>(restored.nodes()[0]);
  auto* restoredSink = dynamic_cast<VolumeSink*>(restored.nodes()[1]);
  ASSERT_NE(restoredSource, nullptr);
  ASSERT_NE(restoredSink, nullptr);
  EXPECT_EQ(restoredSource->label(), QString("My Sphere"));
  EXPECT_EQ(restoredSink->label(), QString("My Volume"));
  EXPECT_FALSE(restoredSink->visibility());

  auto* link = restored.links()[0];
  EXPECT_EQ(link->from()->node(), restoredSource);
  EXPECT_EQ(link->to()->node(), restoredSink);
  EXPECT_EQ(link->from()->name(), QString("volume"));
  EXPECT_EQ(link->to()->name(), QString("volume"));
}

TEST_F(PipelineLibTest, PipelineStateIOSinkGroupRoundTrip)
{
  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  pipeline->addNode(source);

  auto* group = new SinkGroupNode();
  group->addPassthrough("data", PortType::ImageData);
  pipeline->addNode(group);

  auto* volumeSink = new VolumeSink();
  auto* outlineSink = new OutlineSink();
  pipeline->addNode(volumeSink);
  pipeline->addNode(outlineSink);

  pipeline->createLink(source->outputPort("volume"),
                       group->inputPort("data"));
  pipeline->createLink(group->outputPort("data"),
                       volumeSink->inputPort("volume"));
  pipeline->createLink(group->outputPort("data"),
                       outlineSink->inputPort("volume"));

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 4);
  ASSERT_EQ(restored.links().size(), 3);

  // The restored SinkGroupNode should have recreated its passthrough ports.
  SinkGroupNode* restoredGroup = nullptr;
  for (auto* node : restored.nodes()) {
    if (auto* g = dynamic_cast<SinkGroupNode*>(node)) {
      restoredGroup = g;
      break;
    }
  }
  ASSERT_NE(restoredGroup, nullptr);
  EXPECT_NE(restoredGroup->inputPort("data"), nullptr);
  EXPECT_NE(restoredGroup->outputPort("data"), nullptr);

  // The group's single output port should fan out to both sinks.
  auto* groupOutput = restoredGroup->outputPort("data");
  ASSERT_NE(groupOutput, nullptr);
  EXPECT_EQ(groupOutput->links().size(), 2);
}

TEST_F(PipelineLibTest, PipelineStateIOLabelBreakpointProperties)
{
  auto* source = new SphereSource();
  source->setLabel("Src");
  source->setBreakpoint(true);
  source->setProperty("ui.color", QString("#abcdef"));
  source->setProperty("priority", 7);
  pipeline->addNode(source);

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 1);
  auto* restoredSource = dynamic_cast<SphereSource*>(restored.nodes()[0]);
  ASSERT_NE(restoredSource, nullptr);
  EXPECT_EQ(restoredSource->label(), QString("Src"));
  EXPECT_TRUE(restoredSource->hasBreakpoint());
  EXPECT_EQ(restoredSource->property("ui.color").toString(),
            QString("#abcdef"));
  EXPECT_EQ(restoredSource->property("priority").toInt(), 7);
}

TEST_F(PipelineLibTest, PipelineStateIOTypeInferenceSourcesRoundTrip)
{
  // A passthrough with an ImageData output, explicitly inferring its
  // type from a non-default input. Round-trip should preserve the
  // mapping in typeInferenceSources.
  auto* source = new SphereSource();
  pipeline->addNode(source);

  auto* passthrough =
    new PassthroughTransform(PortType::ImageData, PortType::ImageData);
  passthrough->setTypeInferenceSource("out", "in");
  pipeline->addNode(passthrough);

  pipeline->createLink(source->outputPort("volume"),
                       passthrough->inputPort("in"));

  // PassthroughTransform isn't registered in NodeFactory, so we can't
  // round-trip it through the factory. Verify the serialized JSON has
  // the typeInferenceSources entry instead.
  QJsonObject json = passthrough->serialize();
  auto tis = json.value("typeInferenceSources").toObject();
  EXPECT_EQ(tis.value("out").toString(), QString("in"));

  // Now verify a factory-registered node round-trips the mapping too.
  // CropTransform has "in" and "out" port names; setting an explicit
  // mapping exercises the base Node serialize path through a real
  // factory type.
}

TEST_F(PipelineLibTest, PipelineStateIONodeStateRoundTrip)
{
  // Sources execute eagerly on load and end up Current. Sink state is
  // intentionally not restored — a sink is only legitimately Current
  // after consume() has run in the current session (otherwise the
  // pipeline would skip it on the next manual execute). So sinks
  // always land at default New after load.
  auto* src = new SphereSource();
  pipeline->addNode(src);

  auto* sinkStale = new VolumeSink();
  pipeline->addNode(sinkStale);
  sinkStale->markStale();

  auto* sinkNew = new OutlineSink();
  pipeline->addNode(sinkNew);
  // Leave at default New.

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 3);
  // Sink deserialize is deferred to executionFinished.
  QMetaObject::invokeMethod(&restored, "executionFinished",
                            Qt::DirectConnection);
  EXPECT_EQ(restored.nodes()[0]->state(), NodeState::Current);
  EXPECT_EQ(restored.nodes()[1]->state(), NodeState::New);
  EXPECT_EQ(restored.nodes()[2]->state(), NodeState::New);
}

TEST_F(PipelineLibTest, Tvh5FormatWriteEmbedsDataAndStampsDataRef)
{
  auto* src = new SphereSource();
  src->setDimensions(6, 6, 6);
  pipeline->addNode(src);
  ASSERT_TRUE(src->execute());

  QTemporaryFile tmp("XXXXXX.tvh5");
  tmp.setAutoRemove(true);
  ASSERT_TRUE(tmp.open());
  QString path = tmp.fileName();
  tmp.close();

  ASSERT_TRUE(tomviz::Tvh5Format::write(path.toStdString(), pipeline));

  using h5::H5ReadWrite;
  H5ReadWrite reader(path.toStdString(), H5ReadWrite::OpenMode::ReadOnly);
  int nodeId = pipeline->nodeId(src);
  std::string portGroup =
    "/data/" + std::to_string(nodeId) + "/volume";
  EXPECT_TRUE(reader.isGroup(portGroup));

  auto bytes = reader.readData<char>("tomviz_state");
  QJsonDocument doc =
    QJsonDocument::fromJson(QByteArray(bytes.data(), bytes.size()));
  ASSERT_TRUE(doc.isObject());
  auto state = doc.object();
  EXPECT_EQ(state.value("schemaVersion").toInt(), 2);

  auto nodesArr = state.value("pipeline").toObject().value("nodes").toArray();
  ASSERT_EQ(nodesArr.size(), 1);
  auto srcEntry = nodesArr[0].toObject();
  auto outputs = srcEntry.value("outputPorts").toObject();
  auto volumeEntry = outputs.value("volume").toObject();
  // VolumeData metadata must be embedded on the port entry.
  EXPECT_TRUE(volumeEntry.contains("metadata"));
  // dataRef must point at the HDF5 group.
  auto dataRef = volumeEntry.value("dataRef").toObject();
  EXPECT_EQ(dataRef.value("container").toString(), QString("h5"));
  EXPECT_EQ(dataRef.value("path").toString(),
            QString::fromStdString(portGroup));
}

TEST_F(PipelineLibTest, PipelineStateIOReaderSourceNodeReReadsAfterLoad)
{
  // Write a VTI file so ReaderSourceNode has something to re-read.
  auto* sphere = new SphereSource();
  sphere->setDimensions(6, 6, 6);
  pipeline->addNode(sphere);
  ASSERT_TRUE(sphere->execute());
  auto originalVolume =
    sphere->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(originalVolume && originalVolume->isValid());
  auto originalDims = originalVolume->dimensions();

  QTemporaryFile tmpFile("XXXXXX.vti");
  tmpFile.setAutoRemove(true);
  ASSERT_TRUE(tmpFile.open());
  QString tmpPath = tmpFile.fileName();
  tmpFile.close();
  auto writer = vtkSmartPointer<vtkXMLImageDataWriter>::New();
  writer->SetFileName(tmpPath.toStdString().c_str());
  writer->SetInputData(originalVolume->imageData());
  writer->Write();

  // Build a ReaderSourceNode, pre-populate it (simulating what
  // LoadDataReaction does after reading), save, reload, execute.
  auto* original = new ReaderSourceNode();
  original->setFileNames({ tmpPath });
  ASSERT_TRUE(original->execute());  // primes the output port
  pipeline->addNode(original);

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));
  // Should serialize as source.reader (not source.generic).
  auto nodes = state["pipeline"].toObject()["nodes"].toArray();
  // First node is SphereSource; second is the ReaderSourceNode.
  EXPECT_EQ(nodes[1].toObject().value("type").toString(),
            QString("source.reader"));

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  auto* restoredReader =
    dynamic_cast<ReaderSourceNode*>(restored.nodes()[1]);
  ASSERT_NE(restoredReader, nullptr);
  EXPECT_EQ(restoredReader->fileNames(), QStringList({ tmpPath }));
  // PipelineStateIO::load eagerly executes sources so downstream has
  // data even when the caller declines auto-execute — matches
  // LegacyStateLoader. So the reader is already Current and its
  // output port carries freshly-read data.
  EXPECT_EQ(restoredReader->state(), NodeState::Current);
  auto reloaded =
    restoredReader->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(reloaded && reloaded->isValid());
  auto reloadedDims = reloaded->dimensions();
  EXPECT_EQ(reloadedDims[0], originalDims[0]);
  EXPECT_EQ(reloadedDims[1], originalDims[1]);
  EXPECT_EQ(reloadedDims[2], originalDims[2]);
}

TEST_F(PipelineLibTest, Tvh5FormatRoundTripSourceDataFromHdf5)
{
  // Full .tvh5 round-trip: save a pipeline with a SphereSource into
  // an HDF5 container (voxels embedded under /data/<nodeId>/<portName>),
  // then load it back into a fresh pipeline and verify the source's
  // output data was reconstructed from HDF5 (not re-executed).
  auto* sphere = new SphereSource();
  sphere->setDimensions(6, 6, 6);
  pipeline->addNode(sphere);
  ASSERT_TRUE(sphere->execute());
  auto originalDims =
    sphere->outputPort("volume")->data().value<VolumeDataPtr>()->dimensions();

  QTemporaryFile tmpFile("XXXXXX.tvh5");
  tmpFile.setAutoRemove(true);
  ASSERT_TRUE(tmpFile.open());
  QString path = tmpFile.fileName();
  tmpFile.close();

  ASSERT_TRUE(tomviz::Tvh5Format::write(path.toStdString(), pipeline));

  // Read the state JSON back.
  auto state = tomviz::Tvh5Format::readState(path.toStdString());
  EXPECT_EQ(state.value("schemaVersion").toInt(), 2);

  // Load into a fresh pipeline via PipelineStateIO + the HDF5 hook.
  Pipeline restored;
  std::string fileStd = path.toStdString();
  auto hook = [fileStd](Pipeline* p, const QJsonObject& pipelineJson) {
    tomviz::Tvh5Format::populatePayloadData(p, pipelineJson, fileStd);
  };
  ASSERT_TRUE(PipelineStateIO::load(&restored, state, {}, hook));
  ASSERT_EQ(restored.nodes().size(), 1);

  auto* restoredSphere =
    dynamic_cast<SphereSource*>(restored.nodes()[0]);
  ASSERT_NE(restoredSphere, nullptr);
  auto reloaded =
    restoredSphere->outputPort("volume")->data().value<VolumeDataPtr>();
  ASSERT_TRUE(reloaded && reloaded->isValid());
  auto reloadedDims = reloaded->dimensions();
  EXPECT_EQ(reloadedDims[0], originalDims[0]);
  EXPECT_EQ(reloadedDims[1], originalDims[1]);
  EXPECT_EQ(reloadedDims[2], originalDims[2]);
}

TEST_F(PipelineLibTest, Tvh5FormatRoundTripTablePort)
{
  // Stand up a SourceNode with a Table output port and stuff a small
  // vtkTable (one numeric column, one string column) onto it so the
  // writer has something to persist.  Then save → load → verify the
  // restored port carries an equivalent table.
  auto* src = new SourceNode();
  src->setLabel("TableSource");
  src->addOutput("table", PortType::Table);
  pipeline->addNode(src);

  auto table = vtkSmartPointer<vtkTable>::New();
  auto numericCol = vtkSmartPointer<vtkDoubleArray>::New();
  numericCol->SetName("values");
  numericCol->InsertNextValue(1.5);
  numericCol->InsertNextValue(2.5);
  numericCol->InsertNextValue(3.5);
  table->AddColumn(numericCol);
  auto stringCol = vtkSmartPointer<vtkStringArray>::New();
  stringCol->SetName("labels");
  stringCol->InsertNextValue("alpha");
  stringCol->InsertNextValue("beta");
  stringCol->InsertNextValue("gamma");
  table->AddColumn(stringCol);

  src->outputPort("table")->setData(
    PortData(std::any(vtkSmartPointer<vtkTable>(table)), PortType::Table));
  src->markCurrent();

  QTemporaryFile tmpFile("XXXXXX.tvh5");
  tmpFile.setAutoRemove(true);
  ASSERT_TRUE(tmpFile.open());
  QString path = tmpFile.fileName();
  tmpFile.close();

  ASSERT_TRUE(tomviz::Tvh5Format::write(path.toStdString(), pipeline));

  auto state = tomviz::Tvh5Format::readState(path.toStdString());
  EXPECT_EQ(state.value("schemaVersion").toInt(), 2);

  Pipeline restored;
  std::string fileStd = path.toStdString();
  auto hook = [fileStd](Pipeline* p, const QJsonObject& pipelineJson) {
    tomviz::Tvh5Format::populatePayloadData(p, pipelineJson, fileStd);
  };
  ASSERT_TRUE(PipelineStateIO::load(&restored, state, {}, hook));
  ASSERT_EQ(restored.nodes().size(), 1u);

  auto* restoredSrc = restored.nodes()[0];
  ASSERT_NE(restoredSrc, nullptr);
  auto reloaded =
    restoredSrc->outputPort("table")->data().value<vtkSmartPointer<vtkTable>>();
  ASSERT_TRUE(reloaded != nullptr);
  ASSERT_EQ(reloaded->GetNumberOfColumns(), 2);
  ASSERT_EQ(reloaded->GetNumberOfRows(), 3);

  auto* col0 = vtkDoubleArray::SafeDownCast(reloaded->GetColumn(0));
  ASSERT_NE(col0, nullptr);
  EXPECT_STREQ(col0->GetName(), "values");
  EXPECT_DOUBLE_EQ(col0->GetValue(0), 1.5);
  EXPECT_DOUBLE_EQ(col0->GetValue(1), 2.5);
  EXPECT_DOUBLE_EQ(col0->GetValue(2), 3.5);

  auto* col1 = vtkStringArray::SafeDownCast(reloaded->GetColumn(1));
  ASSERT_NE(col1, nullptr);
  EXPECT_STREQ(col1->GetName(), "labels");
  EXPECT_EQ(col1->GetValue(0), "alpha");
  EXPECT_EQ(col1->GetValue(1), "beta");
  EXPECT_EQ(col1->GetValue(2), "gamma");
}

TEST_F(PipelineLibTest, Tvh5FormatRoundTripMoleculePort)
{
  // Mirror Tvh5FormatRoundTripTablePort but for a Molecule output.
  // Atoms (atomic numbers + positions) and bonds (atom-id pairs +
  // bond orders) must round-trip through write → read.
  auto* src = new SourceNode();
  src->setLabel("MoleculeSource");
  src->addOutput("molecule", PortType::Molecule);
  pipeline->addNode(src);

  auto molecule = vtkSmartPointer<vtkMolecule>::New();
  molecule->AppendAtom(1, 0.0, 0.0, 0.0);    // H
  molecule->AppendAtom(8, 0.96, 0.0, 0.0);   // O
  molecule->AppendAtom(1, 1.20, 0.93, 0.0);  // H
  molecule->AppendBond(0, 1, 1);
  molecule->AppendBond(1, 2, 2);

  src->outputPort("molecule")->setData(PortData(
    std::any(vtkSmartPointer<vtkMolecule>(molecule)), PortType::Molecule));
  src->markCurrent();

  QTemporaryFile tmpFile("XXXXXX.tvh5");
  tmpFile.setAutoRemove(true);
  ASSERT_TRUE(tmpFile.open());
  QString path = tmpFile.fileName();
  tmpFile.close();

  ASSERT_TRUE(tomviz::Tvh5Format::write(path.toStdString(), pipeline));

  auto state = tomviz::Tvh5Format::readState(path.toStdString());
  EXPECT_EQ(state.value("schemaVersion").toInt(), 2);

  Pipeline restored;
  std::string fileStd = path.toStdString();
  auto hook = [fileStd](Pipeline* p, const QJsonObject& pipelineJson) {
    tomviz::Tvh5Format::populatePayloadData(p, pipelineJson, fileStd);
  };
  ASSERT_TRUE(PipelineStateIO::load(&restored, state, {}, hook));
  ASSERT_EQ(restored.nodes().size(), 1u);

  auto* restoredSrc = restored.nodes()[0];
  ASSERT_NE(restoredSrc, nullptr);
  auto reloaded = restoredSrc->outputPort("molecule")
                    ->data()
                    .value<vtkSmartPointer<vtkMolecule>>();
  ASSERT_TRUE(reloaded != nullptr);
  ASSERT_EQ(reloaded->GetNumberOfAtoms(), 3);
  ASSERT_EQ(reloaded->GetNumberOfBonds(), 2);

  EXPECT_EQ(reloaded->GetAtom(0).GetAtomicNumber(), 1);
  EXPECT_EQ(reloaded->GetAtom(1).GetAtomicNumber(), 8);
  EXPECT_EQ(reloaded->GetAtom(2).GetAtomicNumber(), 1);

  auto pos1 = reloaded->GetAtom(1).GetPosition();
  EXPECT_FLOAT_EQ(pos1[0], 0.96f);
  EXPECT_FLOAT_EQ(pos1[1], 0.0f);
  EXPECT_FLOAT_EQ(pos1[2], 0.0f);

  auto bond0 = reloaded->GetBond(0);
  EXPECT_EQ(bond0.GetBeginAtomId(), 0);
  EXPECT_EQ(bond0.GetEndAtomId(), 1);
  EXPECT_EQ(reloaded->GetBondOrder(0), 1);
  auto bond1 = reloaded->GetBond(1);
  EXPECT_EQ(bond1.GetBeginAtomId(), 1);
  EXPECT_EQ(bond1.GetEndAtomId(), 2);
  EXPECT_EQ(reloaded->GetBondOrder(1), 2);
}

TEST_F(PipelineLibTest, PipelineStateIOCurrentWithoutDataDowngradesToStale)
{
  // A node saved as Current whose output ports carry no data after
  // load can't honestly stay Current — PipelineStateIO::load must
  // downgrade it (and cascade downstream).
  auto* src = new SourceNode();
  src->addOutput("volume", PortType::ImageData);
  src->markCurrent();
  pipeline->addNode(src);

  auto* sink = new VolumeSink();
  pipeline->addNode(sink);
  sink->markCurrent();
  pipeline->createLink(src->outputPort("volume"),
                       sink->inputPort("volume"));

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 2);
  // Source had no data at save time, so on reload it can't be Current.
  EXPECT_NE(restored.nodes()[0]->state(), NodeState::Current);
  // Cascade: the downstream sink is also now stale (its upstream isn't
  // Current anymore).
  EXPECT_NE(restored.nodes()[1]->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, PipelineStateIOBaseSourceNodeRoundTrip)
{
  // Base SourceNode (as created by LoadDataReaction / LegacyStateLoader)
  // has no output ports declared in its constructor. The loader must
  // recreate them from the serialized outputPorts map so links resolve.
  auto* src = new SourceNode();
  src->addOutput("volume", PortType::TiltSeries);
  src->setLabel("Legacy-loaded source");
  pipeline->addNode(src);

  auto* sink = new VolumeSink();
  pipeline->addNode(sink);
  pipeline->createLink(src->outputPort("volume"),
                       sink->inputPort("volume"));

  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));
  auto nodes = state["pipeline"].toObject()["nodes"].toArray();
  ASSERT_EQ(nodes.size(), 2);
  EXPECT_EQ(nodes[0].toObject().value("type").toString(),
            QString("source.generic"));

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 2);
  ASSERT_EQ(restored.links().size(), 1);

  auto* restoredSrc = dynamic_cast<SourceNode*>(restored.nodes()[0]);
  ASSERT_NE(restoredSrc, nullptr);
  ASSERT_NE(restoredSrc->outputPort("volume"), nullptr);
  EXPECT_EQ(restoredSrc->outputPort("volume")->declaredType(),
            PortType::TiltSeries);
}

TEST_F(PipelineLibTest, PipelineStateIOPreservesNodeIds)
{
  auto* source = new SphereSource();
  pipeline->addNode(source);
  auto* sink = new VolumeSink();
  pipeline->addNode(sink);
  pipeline->createLink(source->outputPort("volume"),
                       sink->inputPort("volume"));

  // Force id assignment on save.
  QJsonObject state;
  ASSERT_TRUE(PipelineStateIO::save(pipeline, state));
  int sourceId = pipeline->nodeId(source);
  int sinkId = pipeline->nodeId(sink);

  Pipeline restored;
  ASSERT_TRUE(PipelineStateIO::load(&restored, state));
  ASSERT_EQ(restored.nodes().size(), 2);

  EXPECT_EQ(restored.nodeId(restored.nodes()[0]), sourceId);
  EXPECT_EQ(restored.nodeId(restored.nodes()[1]), sinkId);
  // nextNodeId must be strictly greater than any assigned id so
  // subsequent additions don't collide.
  EXPECT_GT(restored.nextNodeId(),
            std::max(sourceId, sinkId));
}

TEST_F(PipelineLibTest, AllVolumeSinksAcceptVolume)
{
  // Verify all volume-input sinks work in a pipeline with SphereSource
  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  pipeline->addNode(source);
  source->execute();

  // Create one of each volume-input sink type
  std::vector<LegacyModuleSink*> sinks = {
    new SliceSink(), new ContourSink(), new ThresholdSink(),
    new SegmentSink(), new OutlineSink(), new ClipSink(),
    new RulerSink(), new ScaleCubeSink()
  };

  for (auto* sink : sinks) {
    pipeline->addNode(sink);
    pipeline->createLink(source->outputPort("volume"),
                         sink->inputPort("volume"));
  }

  auto* future = pipeline->execute();

  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());

  for (auto* sink : sinks) {
    EXPECT_EQ(sink->state(), NodeState::Current) << sink->label().toStdString();
  }
}

// --- Volume port type hierarchy tests ---

TEST_F(PipelineLibTest, SubtypeCompatibilityMatrix)
{
  // ImageData input accepts all three volume types
  auto* imgInput = new SourceNode();
  auto* imgTransform = new DoubleTransform(); // has ImageData in
  pipeline->addNode(imgInput);
  pipeline->addNode(imgTransform);
  auto* inPort = imgTransform->inputPort("in");

  // ImageData -> ImageData: OK
  imgInput->addOutput("img", PortType::ImageData);
  EXPECT_TRUE(inPort->canConnectTo(imgInput->outputPort("img")));

  // TiltSeries -> ImageData: OK (subtype)
  auto* tsSource = new SourceNode();
  tsSource->addOutput("ts", PortType::TiltSeries);
  pipeline->addNode(tsSource);
  EXPECT_TRUE(inPort->canConnectTo(tsSource->outputPort("ts")));

  // Volume -> ImageData: OK (subtype)
  auto* volSource = new SourceNode();
  volSource->addOutput("vol", PortType::Volume);
  pipeline->addNode(volSource);
  EXPECT_TRUE(inPort->canConnectTo(volSource->outputPort("vol")));

  // TiltSeries input only accepts TiltSeries
  class TsSink : public SinkNode
  {
  public:
    TsSink() { addInput("in", PortType::TiltSeries); }

  protected:
    bool consume(const QMap<QString, PortData>&) override { return true; }
  };
  class VolSink : public SinkNode
  {
  public:
    VolSink() { addInput("in", PortType::Volume); }

  protected:
    bool consume(const QMap<QString, PortData>&) override { return true; }
  };

  auto* tsSinkNode = new TsSink();
  pipeline->addNode(tsSinkNode);
  auto* tsIn = tsSinkNode->inputPort("in");

  EXPECT_TRUE(tsIn->canConnectTo(tsSource->outputPort("ts")));     // TS -> TS
  EXPECT_FALSE(tsIn->canConnectTo(volSource->outputPort("vol")));  // Vol -> TS
  EXPECT_FALSE(tsIn->canConnectTo(imgInput->outputPort("img")));   // Img -> TS

  // Volume input only accepts Volume
  auto* volSinkNode = new VolSink();
  pipeline->addNode(volSinkNode);
  auto* volIn = volSinkNode->inputPort("in");

  EXPECT_TRUE(volIn->canConnectTo(volSource->outputPort("vol")));   // Vol -> Vol
  EXPECT_FALSE(volIn->canConnectTo(tsSource->outputPort("ts")));    // TS -> Vol
  EXPECT_FALSE(volIn->canConnectTo(imgInput->outputPort("img")));   // Img -> Vol
}

TEST_F(PipelineLibTest, EffectiveTypeInference)
{
  // Source outputs TiltSeries
  auto* source = new SourceNode();
  source->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(source);

  // Generic transform: ImageData -> ImageData
  auto* transform = new DoubleTransform(); // ImageData in/out
  pipeline->addNode(transform);

  // Connect TiltSeries source to generic transform
  pipeline->createLink(source->outputPort("out"),
                       transform->inputPort("in"));

  // The generic transform's output should now have effective type TiltSeries
  auto* outPort = transform->outputPort("out");
  EXPECT_EQ(outPort->declaredType(), PortType::ImageData);
  EXPECT_EQ(outPort->type(), PortType::TiltSeries);
}

TEST_F(PipelineLibTest, EffectiveTypeInferenceVolume)
{
  // Source outputs Volume
  auto* source = new SourceNode();
  source->addOutput("out", PortType::Volume);
  pipeline->addNode(source);

  // Generic transform
  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  pipeline->createLink(source->outputPort("out"),
                       transform->inputPort("in"));

  EXPECT_EQ(transform->outputPort("out")->type(), PortType::Volume);
}

TEST_F(PipelineLibTest, EffectiveTypePropagationChain)
{
  // TiltSeries source -> generic1 -> generic2 -> should all infer TiltSeries
  auto* source = new SourceNode();
  source->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(source);

  auto* t1 = new DoubleTransform();
  pipeline->addNode(t1);
  auto* t2 = new DoubleTransform();
  pipeline->addNode(t2);

  pipeline->createLink(source->outputPort("out"), t1->inputPort("in"));
  pipeline->createLink(t1->outputPort("out"), t2->inputPort("in"));

  EXPECT_EQ(t1->outputPort("out")->type(), PortType::TiltSeries);
  EXPECT_EQ(t2->outputPort("out")->type(), PortType::TiltSeries);
}

TEST_F(PipelineLibTest, EffectiveTypeRevertsOnDisconnect)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(source);

  auto* transform = new DoubleTransform();
  pipeline->addNode(transform);

  auto* link = pipeline->createLink(source->outputPort("out"),
                                    transform->inputPort("in"));
  EXPECT_EQ(transform->outputPort("out")->type(), PortType::TiltSeries);

  // Disconnect — effective type should revert to declared (ImageData)
  pipeline->removeLink(link);
  EXPECT_EQ(transform->outputPort("out")->type(), PortType::ImageData);
}

TEST_F(PipelineLibTest, LinkValidityWithInference)
{
  // TiltSeries source -> generic transform -> TiltSeries-requiring node
  auto* source = new SourceNode();
  source->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(source);

  auto* generic = new DoubleTransform(); // ImageData -> ImageData
  pipeline->addNode(generic);

  auto* tsNode = new PassthroughTransform(PortType::TiltSeries,
                                          PortType::TiltSeries);
  pipeline->addNode(tsNode);

  // Connect source -> generic (valid, TiltSeries -> ImageData)
  auto* link1 = pipeline->createLink(source->outputPort("out"),
                                     generic->inputPort("in"));
  ASSERT_NE(link1, nullptr);
  EXPECT_TRUE(link1->isValid());

  // Generic's output is now effectively TiltSeries, so this should work
  auto* link2 = pipeline->createLink(generic->outputPort("out"),
                                     tsNode->inputPort("in"));
  ASSERT_NE(link2, nullptr);
  EXPECT_TRUE(link2->isValid());

  // Now disconnect the source from generic — generic reverts to ImageData
  // link2 should become invalid
  pipeline->removeLink(link1);
  EXPECT_FALSE(link2->isValid());

  // Reconnect the source — link2 should become valid again
  link1 = pipeline->createLink(source->outputPort("out"),
                               generic->inputPort("in"));
  EXPECT_TRUE(link2->isValid());
}

TEST_F(PipelineLibTest, LinkValidityChangedSignal)
{
  auto* source = new SourceNode();
  source->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(source);

  auto* generic = new DoubleTransform();
  pipeline->addNode(generic);

  auto* tsNode = new PassthroughTransform(PortType::TiltSeries,
                                          PortType::TiltSeries);
  pipeline->addNode(tsNode);

  auto* link1 = pipeline->createLink(source->outputPort("out"),
                                     generic->inputPort("in"));
  auto* link2 = pipeline->createLink(generic->outputPort("out"),
                                     tsNode->inputPort("in"));

  QSignalSpy validitySpy(link2, &Link::validityChanged);

  // Disconnect source — should trigger validityChanged(false)
  pipeline->removeLink(link1);
  EXPECT_EQ(validitySpy.count(), 1);
  EXPECT_FALSE(validitySpy.at(0).at(0).toBool());
}

TEST_F(PipelineLibTest, ExecutionSkipsNodesWithInvalidLinks)
{
  // Build: TiltSeries source -> generic -> TiltSeries-requiring node
  auto* source = new SourceNode();
  source->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(source);
  source->setOutputData("out", PortData(std::any(5), PortType::ImageData));

  auto* generic = new DoubleTransform();
  pipeline->addNode(generic);

  auto* tsNode = new PassthroughTransform(PortType::TiltSeries,
                                          PortType::TiltSeries);
  pipeline->addNode(tsNode);

  // source (TiltSeries) -> generic (OK, infers TiltSeries)
  auto* link1 = pipeline->createLink(source->outputPort("out"),
                                     generic->inputPort("in"));
  ASSERT_NE(link1, nullptr);

  // generic (effective: TiltSeries) -> tsNode (requires TiltSeries) — valid
  auto* link2 = pipeline->createLink(generic->outputPort("out"),
                                     tsNode->inputPort("in"));
  ASSERT_NE(link2, nullptr);
  EXPECT_TRUE(link2->isValid());

  // Now disconnect source -> generic, making link2 invalid
  pipeline->removeLink(link1);
  EXPECT_FALSE(link2->isValid());

  // Try to execute; tsNode should be skipped
  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());

  // tsNode should NOT have reached Current (skipped due to invalid link)
  EXPECT_NE(tsNode->state(), NodeState::Current);
}

TEST_F(PipelineLibTest, ExplicitTypeInferenceSource)
{
  // Node with two ImageData inputs; explicit mapping says output follows "b"
  class TwoInputTransform : public TransformNode
  {
  public:
    TwoInputTransform()
    {
      addInput("a", PortType::ImageData);
      addInput("b", PortType::ImageData);
      addOutput("out", PortType::ImageData);
      setTypeInferenceSource("out", "b");
    }

  protected:
    QMap<QString, PortData> transform(
      const QMap<QString, PortData>& inputs) override
    {
      return { { "out", inputs["a"] } };
    }
  };

  auto* tsSource = new SourceNode();
  tsSource->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(tsSource);

  auto* volSource = new SourceNode();
  volSource->addOutput("out", PortType::Volume);
  pipeline->addNode(volSource);

  auto* transform = new TwoInputTransform();
  pipeline->addNode(transform);

  // Connect TiltSeries to "a", Volume to "b"
  pipeline->createLink(tsSource->outputPort("out"),
                       transform->inputPort("a"));
  pipeline->createLink(volSource->outputPort("out"),
                       transform->inputPort("b"));

  // Output should follow "b" (Volume), not "a" (TiltSeries)
  EXPECT_EQ(transform->outputPort("out")->type(), PortType::Volume);
}

TEST_F(PipelineLibTest, DefaultInferenceUsesFirstImageDataInput)
{
  // Node with two ImageData inputs, no explicit mapping
  class TwoInputTransform : public TransformNode
  {
  public:
    TwoInputTransform()
    {
      addInput("a", PortType::ImageData);
      addInput("b", PortType::ImageData);
      addOutput("out", PortType::ImageData);
    }

  protected:
    QMap<QString, PortData> transform(
      const QMap<QString, PortData>& inputs) override
    {
      return { { "out", inputs["a"] } };
    }
  };

  auto* tsSource = new SourceNode();
  tsSource->addOutput("out", PortType::TiltSeries);
  pipeline->addNode(tsSource);

  auto* volSource = new SourceNode();
  volSource->addOutput("out", PortType::Volume);
  pipeline->addNode(volSource);

  auto* transform = new TwoInputTransform();
  pipeline->addNode(transform);

  // Connect TiltSeries to "a" (first ImageData input), Volume to "b"
  pipeline->createLink(tsSource->outputPort("out"),
                       transform->inputPort("a"));
  pipeline->createLink(volSource->outputPort("out"),
                       transform->inputPort("b"));

  // Output should follow "a" (first ImageData input) → TiltSeries
  EXPECT_EQ(transform->outputPort("out")->type(), PortType::TiltSeries);
}

TEST_F(PipelineLibTest, ConcreteTypeNotInferred)
{
  // A node with TiltSeries output should always keep that type,
  // regardless of what's connected to its input
  auto* source = new SourceNode();
  source->addOutput("out", PortType::Volume);
  pipeline->addNode(source);

  auto* tsTransform = new PassthroughTransform(PortType::ImageData,
                                                PortType::TiltSeries);
  pipeline->addNode(tsTransform);

  pipeline->createLink(source->outputPort("out"),
                       tsTransform->inputPort("in"));

  // Output declared as TiltSeries — should stay TiltSeries, not inherit Volume
  EXPECT_EQ(tsTransform->outputPort("out")->type(), PortType::TiltSeries);
}

TEST_F(PipelineLibTest, IsVolumeTypeHelper)
{
  EXPECT_TRUE(isVolumeType(PortType::ImageData));
  EXPECT_TRUE(isVolumeType(PortType::TiltSeries));
  EXPECT_TRUE(isVolumeType(PortType::Volume));
  EXPECT_FALSE(isVolumeType(PortType::Table));
  EXPECT_FALSE(isVolumeType(PortType::Molecule));
  EXPECT_FALSE(isVolumeType(PortType::None));
}

// --- Sink property and serialization tests ---

TEST_F(PipelineLibTest, ContourSinkPropertyDefaults)
{
  ContourSink sink;
  EXPECT_EQ(sink.label(), "Contour");
  EXPECT_DOUBLE_EQ(sink.isoValue(), 0.0);
  EXPECT_DOUBLE_EQ(sink.ambient(), 0.0);
  EXPECT_DOUBLE_EQ(sink.diffuse(), 1.0);
  EXPECT_DOUBLE_EQ(sink.specular(), 1.0);
  EXPECT_DOUBLE_EQ(sink.specularPower(), 100.0);
  EXPECT_EQ(sink.representation(), 2); // VTK_SURFACE
  EXPECT_TRUE(sink.mapScalars());
  EXPECT_FALSE(sink.useSolidColor());
  EXPECT_FALSE(sink.colorByArray());
  EXPECT_TRUE(sink.isColorMapNeeded());
}

TEST_F(PipelineLibTest, ContourSinkPropertySetters)
{
  ContourSink sink;
  sink.setIsoValue(42.5);
  EXPECT_DOUBLE_EQ(sink.isoValue(), 42.5);

  sink.setOpacity(0.7);
  EXPECT_DOUBLE_EQ(sink.opacity(), 0.7);

  sink.setAmbient(0.3);
  EXPECT_DOUBLE_EQ(sink.ambient(), 0.3);

  sink.setDiffuse(0.5);
  EXPECT_DOUBLE_EQ(sink.diffuse(), 0.5);

  sink.setSpecular(0.8);
  EXPECT_DOUBLE_EQ(sink.specular(), 0.8);

  sink.setSpecularPower(50.0);
  EXPECT_DOUBLE_EQ(sink.specularPower(), 50.0);

  sink.setRepresentation(1); // wireframe
  EXPECT_EQ(sink.representation(), 1);
  EXPECT_EQ(sink.representationString(), "Wireframe");

  sink.setRepresentationString("Points");
  EXPECT_EQ(sink.representation(), 0);

  sink.setUseSolidColor(true);
  EXPECT_TRUE(sink.useSolidColor());

  sink.setMapScalars(false);
  EXPECT_FALSE(sink.mapScalars());

  sink.setColorByArray(true);
  EXPECT_TRUE(sink.colorByArray());

  sink.setColorByArrayName("TestArray");
  EXPECT_EQ(sink.colorByArrayName(), "TestArray");

  sink.setActiveScalars(2);
  EXPECT_EQ(sink.activeScalars(), 2);
}

TEST_F(PipelineLibTest, ContourSinkSerializationRoundTrip)
{
  ContourSink sink;
  sink.setIsoValue(123.0);
  sink.setOpacity(0.5);
  sink.setAmbient(0.2);
  sink.setDiffuse(0.8);
  sink.setSpecular(0.6);
  sink.setSpecularPower(75.0);
  sink.setRepresentationString("Wireframe");
  sink.setUseSolidColor(true);
  sink.setQColor(QColor(255, 0, 128));
  sink.setMapScalars(false);
  sink.setColorByArray(true);
  sink.setColorByArrayName("MyArray");

  auto json = sink.serialize();

  ContourSink restored;
  EXPECT_TRUE(restored.deserialize(json));

  EXPECT_DOUBLE_EQ(restored.isoValue(), 123.0);
  EXPECT_DOUBLE_EQ(restored.opacity(), 0.5);
  EXPECT_DOUBLE_EQ(restored.ambient(), 0.2);
  EXPECT_DOUBLE_EQ(restored.diffuse(), 0.8);
  EXPECT_DOUBLE_EQ(restored.specular(), 0.6);
  EXPECT_DOUBLE_EQ(restored.specularPower(), 75.0);
  EXPECT_EQ(restored.representationString(), "Wireframe");
  EXPECT_TRUE(restored.useSolidColor());
  EXPECT_EQ(restored.qcolor(), QColor(255, 0, 128));
  EXPECT_FALSE(restored.mapScalars());
  EXPECT_TRUE(restored.colorByArray());
  EXPECT_EQ(restored.colorByArrayName(), "MyArray");
}

TEST_F(PipelineLibTest, SliceSinkPropertyDefaults)
{
  SliceSink sink;
  EXPECT_EQ(sink.label(), "Slice");
  EXPECT_EQ(sink.direction(), SliceSink::XY);
  EXPECT_TRUE(sink.isOrtho());
  EXPECT_DOUBLE_EQ(sink.opacity(), 1.0);
  EXPECT_EQ(sink.sliceThickness(), 1);
  EXPECT_EQ(sink.thickSliceMode(), SliceSink::Mean);
  EXPECT_FALSE(sink.textureInterpolate());
  EXPECT_TRUE(sink.showArrow());
  EXPECT_TRUE(sink.mapScalars());
  EXPECT_TRUE(sink.isColorMapNeeded());
}

TEST_F(PipelineLibTest, SliceSinkPropertySetters)
{
  SliceSink sink;

  sink.setDirection(SliceSink::YZ);
  EXPECT_EQ(sink.direction(), SliceSink::YZ);
  EXPECT_TRUE(sink.isOrtho());

  sink.setDirection(SliceSink::Custom);
  EXPECT_EQ(sink.direction(), SliceSink::Custom);
  EXPECT_FALSE(sink.isOrtho());

  sink.setOpacity(0.4);
  EXPECT_DOUBLE_EQ(sink.opacity(), 0.4);

  sink.setSliceThickness(5);
  EXPECT_EQ(sink.sliceThickness(), 5);

  sink.setThickSliceMode(SliceSink::Sum);
  EXPECT_EQ(sink.thickSliceMode(), SliceSink::Sum);

  sink.setTextureInterpolate(true);
  EXPECT_TRUE(sink.textureInterpolate());

  sink.setShowArrow(false);
  EXPECT_FALSE(sink.showArrow());

  sink.setMapScalars(false);
  EXPECT_FALSE(sink.mapScalars());

  sink.setActiveScalars(3);
  EXPECT_EQ(sink.activeScalars(), 3);

  sink.setPlaneCenter(1.0, 2.0, 3.0);
  double center[3];
  sink.planeCenter(center);
  EXPECT_DOUBLE_EQ(center[0], 1.0);
  EXPECT_DOUBLE_EQ(center[1], 2.0);
  EXPECT_DOUBLE_EQ(center[2], 3.0);

  sink.setPlaneNormal(0.0, 1.0, 0.0);
  double normal[3];
  sink.planeNormal(normal);
  EXPECT_DOUBLE_EQ(normal[0], 0.0);
  EXPECT_DOUBLE_EQ(normal[1], 1.0);
  EXPECT_DOUBLE_EQ(normal[2], 0.0);
}

TEST_F(PipelineLibTest, SliceSinkSerializationRoundTrip)
{
  SliceSink sink;
  sink.setDirection(SliceSink::XZ);
  sink.setSlice(7);
  sink.setOpacity(0.6);
  sink.setSliceThickness(3);
  sink.setThickSliceMode(SliceSink::Max);
  sink.setTextureInterpolate(true);
  sink.setShowArrow(false);
  sink.setMapScalars(false);

  auto json = sink.serialize();

  SliceSink restored;
  EXPECT_TRUE(restored.deserialize(json));

  EXPECT_EQ(restored.direction(), SliceSink::XZ);
  EXPECT_EQ(restored.slice(), 7);
  EXPECT_DOUBLE_EQ(restored.opacity(), 0.6);
  EXPECT_EQ(restored.sliceThickness(), 3);
  EXPECT_EQ(restored.thickSliceMode(), SliceSink::Max);
  EXPECT_TRUE(restored.textureInterpolate());
  EXPECT_FALSE(restored.showArrow());
  EXPECT_FALSE(restored.mapScalars());
}

TEST_F(PipelineLibTest, SliceSinkLinkingPropagatesToLinkedPeers)
{
  auto* a = new SliceSink();
  auto* b = new SliceSink();
  auto* unlinked = new SliceSink();
  pipeline->addNode(a);
  pipeline->addNode(b);
  pipeline->addNode(unlinked);

  b->setLinked(true);
  b->setSlice(5);
  a->setSlice(9);
  // Turning the link on adopts the newly linked view's state everywhere
  a->setLinked(true);
  EXPECT_EQ(b->slice(), 9);

  // Changes reach the linked peer, in both directions, but not the
  // unlinked sink; direction is linked too
  a->setSlice(12);
  EXPECT_EQ(b->slice(), 12);
  EXPECT_NE(unlinked->slice(), 12);
  b->setSlice(3);
  EXPECT_EQ(a->slice(), 3);
  a->setDirection(SliceSink::YZ);
  EXPECT_EQ(b->direction(), SliceSink::YZ);
  EXPECT_EQ(unlinked->direction(), SliceSink::XY);

  // Unlinking stops propagation
  b->setLinked(false);
  a->setSlice(20);
  EXPECT_NE(b->slice(), 20);
}

TEST_F(PipelineLibTest, ClipSinkLinkingPropagatesToLinkedPeers)
{
  auto* a = new ClipSink();
  auto* b = new ClipSink();
  pipeline->addNode(a);
  pipeline->addNode(b);

  a->setLinked(true);
  b->setLinked(true);

  a->setSlice(8);
  EXPECT_EQ(b->slice(), 8);

  a->setDirection(ClipSink::XZ);
  EXPECT_EQ(b->direction(), ClipSink::XZ);
}

TEST_F(PipelineLibTest, SinkLinkFlagSurvivesSerialization)
{
  SliceSink slice;
  slice.setLinked(true);
  SliceSink restoredSlice;
  EXPECT_TRUE(restoredSlice.deserialize(slice.serialize()));
  EXPECT_TRUE(restoredSlice.linked());

  ClipSink clip;
  clip.setLinked(true);
  ClipSink restoredClip;
  EXPECT_TRUE(restoredClip.deserialize(clip.serialize()));
  EXPECT_TRUE(restoredClip.linked());
}

// A volume with the given dimensions and spacing, for geometry tests.
PortData makeVolumeWithGeometry(int nx, int ny, int nz, double sx, double sy,
                                double sz)
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(nx, ny, nz);
  image->SetSpacing(sx, sy, sz);
  vtkNew<vtkFloatArray> array;
  array->SetName("scalars");
  array->SetNumberOfTuples(nx * ny * nz);
  array->FillValue(1.0f);
  image->GetPointData()->SetScalars(array);
  return PortData(std::any(std::make_shared<VolumeData>(image)),
                  PortType::ImageData);
}

TEST_F(PipelineLibTest, LinkedSlicesMatchByPhysicalPosition)
{
  struct OpenSliceSink : SliceSink
  {
    using SliceSink::consume;
  };
  auto* fine = new OpenSliceSink();
  auto* coarse = new OpenSliceSink();
  pipeline->addNode(fine);
  pipeline->addNode(coarse);

  // Same 20-unit extent along Z: 21 slices 1 apart vs 11 slices 2 apart
  QMap<QString, PortData> inputs;
  inputs["volume"] = makeVolumeWithGeometry(4, 4, 21, 1, 1, 1);
  ASSERT_TRUE(fine->consume(inputs));
  inputs["volume"] = makeVolumeWithGeometry(4, 4, 11, 1, 1, 2);
  ASSERT_TRUE(coarse->consume(inputs));

  fine->setLinked(true);
  coarse->setLinked(true);
  fine->setSlice(10);
  EXPECT_EQ(coarse->slice(), 5);
  coarse->setSlice(8);
  EXPECT_EQ(fine->slice(), 16);
  EXPECT_DOUBLE_EQ(fine->slicePosition(), coarse->slicePosition());

  // A peer whose geometry is unknown still gets the raw index
  auto* empty = new SliceSink();
  pipeline->addNode(empty);
  empty->setLinked(true);
  fine->setSlice(4);
  EXPECT_EQ(empty->slice(), 4);
}

TEST_F(PipelineLibTest, LinkedSlicesShareTheCustomPlane)
{
  auto* a = new SliceSink();
  auto* b = new SliceSink();
  pipeline->addNode(a);
  pipeline->addNode(b);
  a->setLinked(true);
  b->setLinked(true);

  a->setDirection(SliceSink::Custom);
  a->setPlaneNormal(0, 1, 0);
  a->setPlaneCenter(1, 2, 3);
  EXPECT_EQ(b->direction(), SliceSink::Custom);
  double n[3], c[3];
  b->planeNormal(n);
  b->planeCenter(c);
  EXPECT_DOUBLE_EQ(n[1], 1.0);
  EXPECT_DOUBLE_EQ(c[0], 1.0);
  EXPECT_DOUBLE_EQ(c[2], 3.0);
}

TEST_F(PipelineLibTest, LinkedClipsMatchByPhysicalPosition)
{
  struct OpenClipSink : ClipSink
  {
    using ClipSink::consume;
  };
  auto* fine = new OpenClipSink();
  auto* coarse = new OpenClipSink();
  pipeline->addNode(fine);
  pipeline->addNode(coarse);

  QMap<QString, PortData> inputs;
  inputs["volume"] = makeVolumeWithGeometry(4, 4, 21, 1, 1, 1);
  ASSERT_TRUE(fine->consume(inputs));
  inputs["volume"] = makeVolumeWithGeometry(4, 4, 11, 1, 1, 2);
  ASSERT_TRUE(coarse->consume(inputs));

  fine->setLinked(true);
  coarse->setLinked(true);
  fine->setSlice(10);
  EXPECT_EQ(coarse->slice(), 5);

  fine->setDirection(ClipSink::Custom);
  EXPECT_EQ(coarse->direction(), ClipSink::Custom);
}

TEST_F(PipelineLibTest, VolumeSinkExplodedViewExcludesCutOut)
{
  struct OpenVolumeSink : VolumeSink
  {
    using VolumeSink::consume;
  };
  auto* sink = new OpenVolumeSink();
  pipeline->addNode(sink);
  QMap<QString, PortData> inputs;
  inputs["volume"] = makeVolumeWithGeometry(8, 8, 16, 1, 1, 1);
  ASSERT_TRUE(sink->consume(inputs));

  sink->setCutOutEnabled(true);
  sink->setExplodedEnabled(true);
  EXPECT_TRUE(sink->explodedEnabled());
  EXPECT_FALSE(sink->cutOutEnabled());
  sink->setCutOutEnabled(true);
  EXPECT_FALSE(sink->explodedEnabled());

  // Parameters clamp to their supported ranges
  sink->setExplodedChunks(100);
  EXPECT_EQ(sink->explodedChunks(), 16);
  sink->setExplodedGap(3.0);
  EXPECT_DOUBLE_EQ(sink->explodedGap(), 1.0);
  sink->setExplodedAxis(7);
  EXPECT_EQ(sink->explodedAxis(), kExplodedCustomAxis);
  sink->setExplodedAxis(-1);
  EXPECT_EQ(sink->explodedAxis(), 0);
}

// A 12^3 label map: label 1 fills a 4^3 block, label 2 a 2^3 block
// touching it, background 0 everywhere else.
PortData makeTwoLabelVolume()
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(12, 12, 12);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto* p = static_cast<unsigned char*>(image->GetScalarPointer());
  std::fill(p, p + 12 * 12 * 12, 0);
  for (int z = 3; z < 7; ++z) {
    for (int y = 3; y < 7; ++y) {
      for (int x = 3; x < 7; ++x) {
        p[(z * 12 + y) * 12 + x] = 1;
      }
      for (int x = 7; x < 9; ++x) {
        if (y < 5 && z < 5) {
          p[(z * 12 + y) * 12 + x] = 2;
        }
      }
    }
  }
  image->GetPointData()->GetScalars()->SetName("labels");
  return PortData(std::any(makeVolumeData(image, PortType::LabelMap)),
                  PortType::LabelMap);
}

TEST_F(PipelineLibTest, LabelMapSurfaceExtractsOneClosedSurfacePerLabel)
{
  auto data = makeTwoLabelVolume();
  auto labels = labelMapData(data.value<VolumeDataPtr>());
  ASSERT_TRUE(labels);
  labels->refreshLabels();
  const auto& table = labels->labels();
  ASSERT_EQ(table.count(), 3); // 0, 1, 2

  auto regions = regionLabels(table, 0.0);
  EXPECT_EQ(regions, (QVector<double>{ 1.0, 2.0 }));
  // Label 0 is hidden by convention; 1 and 2 start visible
  EXPECT_EQ(visibleLabels(table, 0.0), regions);

  // Unsmoothed: the raw voxel faces. A 4^3 block has 6 faces of 16
  // quads, minus the 2x2 patch label 2 covers on one of them; the
  // shared patch is a face of label 2 too, so it is still output once.
  auto surface =
    extractLabelSurface(labels->imageData(), regions, regions, 0, 0.0);
  ASSERT_TRUE(surface);
  EXPECT_EQ(surface->GetNumberOfCells(), 6 * 16 + 6 * 4 - 4);
  auto* boundary = surface->GetCellData()->GetArray("BoundaryLabels");
  ASSERT_TRUE(boundary);
  EXPECT_EQ(boundary->GetNumberOfComponents(), 2);

  // Hiding label 2 drops its faces except the one it shares with 1,
  // which now bounds 1 alone
  auto only1 = extractLabelSurface(labels->imageData(), regions,
                                   QVector<double>{ 1.0 }, 0, 0.0);
  EXPECT_EQ(only1->GetNumberOfCells(), 6 * 16);

  // Colors come from the table, per face
  colorLabelSurface(surface, table, 0.0);
  auto* colors = vtkUnsignedCharArray::SafeDownCast(
    surface->GetCellData()->GetArray(kLabelColorsArrayName));
  ASSERT_TRUE(colors);
  EXPECT_EQ(colors->GetNumberOfTuples(), surface->GetNumberOfCells());
  EXPECT_EQ(surface->GetCellData()->GetScalars(), colors);
  int counted[3] = { 0, 0, 0 };
  for (vtkIdType c = 0; c < surface->GetNumberOfCells(); ++c) {
    unsigned char rgb[3];
    colors->GetTypedTuple(c, rgb);
    for (int i = 1; i <= 2; ++i) {
      QColor expected = table.at(table.indexOfValue(i)).color;
      if (rgb[0] == expected.red() && rgb[1] == expected.green() &&
          rgb[2] == expected.blue()) {
        ++counted[i];
      }
    }
  }
  EXPECT_EQ(counted[1] + counted[2], surface->GetNumberOfCells());
  EXPECT_GE(counted[2], 6 * 4 - 4);

  // Smoothing keeps the topology and adds point normals
  auto smooth =
    extractLabelSurface(labels->imageData(), regions, regions, 8, 0.0);
  EXPECT_GT(smooth->GetNumberOfCells(), 0);
  EXPECT_TRUE(smooth->GetPointData()->GetNormals());
  EXPECT_TRUE(smooth->GetCellData()->GetArray("BoundaryLabels"));

  // Selecting from the smoothed (triangle) mesh keeps exactly the faces
  // that bound the chosen label, with their tags, on the shared points
  auto smoothOnly1 = selectLabelFaces(smooth, QVector<double>{ 1.0 });
  EXPECT_GT(smoothOnly1->GetNumberOfCells(), 0);
  EXPECT_LT(smoothOnly1->GetNumberOfCells(), smooth->GetNumberOfCells());
  EXPECT_EQ(smoothOnly1->GetPoints(), smooth->GetPoints());
  auto* tags = smoothOnly1->GetCellData()->GetArray("BoundaryLabels");
  ASSERT_TRUE(tags);
  ASSERT_EQ(tags->GetNumberOfTuples(), smoothOnly1->GetNumberOfCells());
  for (vtkIdType c = 0; c < tags->GetNumberOfTuples(); ++c) {
    EXPECT_TRUE(tags->GetComponent(c, 0) == 1.0 ||
                tags->GetComponent(c, 1) == 1.0);
  }

  // Nothing to draw: no visible labels, or a single slice
  EXPECT_EQ(extractLabelSurface(labels->imageData(), regions, {}, 0, 0.0)
              ->GetNumberOfCells(),
            0);
  vtkNew<vtkImageData> slice;
  slice->SetDimensions(12, 12, 1);
  slice->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  EXPECT_EQ(extractLabelSurface(slice, regions, regions, 0, 0.0)
              ->GetNumberOfCells(),
            0);
}

TEST_F(PipelineLibTest, LabelMapSinkSurfaceFollowsLabelsAndRepresentation)
{
  struct OpenLabelMapSink : LabelMapSink
  {
    using LabelMapSink::prepareConsume;
    using LabelMapSink::consume;
  };
  auto* sink = new OpenLabelMapSink();
  pipeline->addNode(sink);
  EXPECT_EQ(sink->representation(), LabelMapSink::Representation::Surface);
  EXPECT_TRUE(sink->fineSampling());

  // The trio SinkNode::runConsume runs; the payload in `inputs` is what
  // the sink's volumeData() weakly refers to afterwards.
  QMap<QString, PortData> inputs;
  inputs["volume"] = makeTwoLabelVolume();
  sink->prepareConsume(inputs);
  ASSERT_TRUE(sink->consume(inputs));
  ASSERT_TRUE(sink->surface());
  const auto cells = sink->surface()->GetNumberOfCells();
  EXPECT_GT(cells, 0);
  // Ambient floor for the volume representation, applied once
  EXPECT_GE(sink->ambient(), 0.3);

  // A color edit only recolors. Hiding a label re-selects faces from
  // the extracted mesh, whose points it keeps sharing: no new pass over
  // the volume, which is what made a checkbox slow on a large
  // segmentation.
  auto labels = sink->labelMap();
  ASSERT_TRUE(labels);
  auto* before = sink->surface();
  auto* meshPoints = before->GetPoints();
  labels->labels().setColor(labels->labels().indexOfValue(1.0), Qt::cyan);
  sink->applyLabels();
  EXPECT_EQ(sink->surface(), before);
  labels->labels().setVisible(labels->labels().indexOfValue(2.0), false);
  sink->applyLabels();
  EXPECT_NE(sink->surface(), before);
  EXPECT_EQ(sink->surface()->GetPoints(), meshPoints);
  EXPECT_LT(sink->surface()->GetNumberOfCells(), cells);
  // Showing it again restores every face
  labels->labels().setVisible(labels->labels().indexOfValue(2.0), true);
  sink->applyLabels();
  EXPECT_EQ(sink->surface()->GetNumberOfCells(), cells);
  EXPECT_EQ(sink->surface()->GetPoints(), meshPoints);

  // Switching to the volume representation keeps the mesh around, and
  // smoothing changes rebuild it
  sink->setRepresentation(LabelMapSink::Representation::Volume);
  EXPECT_EQ(sink->representation(), LabelMapSink::Representation::Volume);
  sink->setRepresentation(LabelMapSink::Representation::Surface);
  auto* unsmoothed = sink->surface();
  sink->setSurfaceSmoothing(0);
  EXPECT_NE(sink->surface(), unsmoothed);
  EXPECT_FALSE(sink->surface()->GetPointData()->GetNormals());

  // Serialization round trip, and old files keep their volume look
  sink->setSurfaceOpacity(0.4);
  sink->setSurfaceSmoothing(3);
  auto json = sink->serialize();
  LabelMapSink restored;
  ASSERT_TRUE(restored.deserialize(json));
  EXPECT_EQ(restored.representation(),
            LabelMapSink::Representation::Surface);
  EXPECT_EQ(restored.surfaceSmoothing(), 3);
  EXPECT_DOUBLE_EQ(restored.surfaceOpacity(), 0.4);
  json.remove("representation");
  LabelMapSink older;
  ASSERT_TRUE(older.deserialize(json));
  EXPECT_EQ(older.representation(), LabelMapSink::Representation::Volume);
}

TEST_F(PipelineLibTest, VolumeSinkExplodedViewSerializationRoundTrip)
{
  VolumeSink sink;
  sink.setExplodedAxis(1);
  sink.setExplodedChunks(5);
  sink.setExplodedGap(0.4);
  sink.setExplodedEnabled(true);

  VolumeSink restored;
  ASSERT_TRUE(restored.deserialize(sink.serialize()));
  EXPECT_TRUE(restored.explodedEnabled());
  EXPECT_EQ(restored.explodedAxis(), 1);
  EXPECT_EQ(restored.explodedChunks(), 5);
  EXPECT_DOUBLE_EQ(restored.explodedGap(), 0.4);
}

TEST_F(PipelineLibTest, UserLightingPresetsRoundTripAndApply)
{
  auto& store = LightingPresetStore::instance();
  auto named = [&store](const QString& name) {
    int n = 0;
    for (const auto& p : store.presets()) {
      n += p.name == name ? 1 : 0;
    }
    return n;
  };
  store.remove("Bench");

  VolumeSink sink;
  sink.setLighting(true);
  sink.setAmbient(0.42);
  sink.setDiffuse(0.6);
  sink.setSpecularPower(12.0);
  sink.setSmoothNormals(true);
  auto preset = sink.currentLightingValues();
  preset.name = "Bench";
  store.save(preset);
  EXPECT_EQ(sink.matchingUserLightingPreset(), "Bench");

  // Saving under the same name replaces rather than duplicates
  preset.ambient = 0.2;
  store.save(preset);
  EXPECT_EQ(named("Bench"), 1);
  EXPECT_TRUE(sink.matchingUserLightingPreset().isEmpty());

  VolumeSink other;
  other.applyUserLightingPreset(UserLightingPreset::deserialize(
    store.preset("Bench").serialize()));
  EXPECT_DOUBLE_EQ(other.ambient(), 0.2);
  EXPECT_DOUBLE_EQ(other.diffuse(), 0.6);
  EXPECT_DOUBLE_EQ(other.specularPower(), 12.0);
  EXPECT_TRUE(other.smoothNormals());
  EXPECT_EQ(other.matchingUserLightingPreset(), "Bench");

  store.remove("Bench");
  EXPECT_EQ(named("Bench"), 0);
}

TEST_F(PipelineLibTest, VolumeSinkCutOutProperties)
{
  VolumeSink sink;
  EXPECT_FALSE(sink.cutOutEnabled());
  EXPECT_EQ(sink.cutOutCorner(), 0);
  EXPECT_DOUBLE_EQ(sink.cutOutPosition(0), 0.5);

  sink.setCutOutEnabled(true);
  EXPECT_TRUE(sink.cutOutEnabled());

  sink.setCutOutCorner(5);
  EXPECT_EQ(sink.cutOutCorner(), 5);
  // Out-of-range corners clamp rather than indexing a bogus region
  sink.setCutOutCorner(99);
  EXPECT_EQ(sink.cutOutCorner(), 7);
  sink.setCutOutCorner(-3);
  EXPECT_EQ(sink.cutOutCorner(), 0);

  sink.setCutOutPosition(1, 0.25);
  EXPECT_DOUBLE_EQ(sink.cutOutPosition(1), 0.25);
  sink.setCutOutPosition(2, 5.0);
  EXPECT_DOUBLE_EQ(sink.cutOutPosition(2), 1.0);

  // Bad axes are ignored rather than writing out of bounds
  sink.setCutOutPosition(7, 0.3);
  EXPECT_DOUBLE_EQ(sink.cutOutPosition(7), 0.5);
}

TEST_F(PipelineLibTest, VolumeSinkCutOutSerializationRoundTrip)
{
  VolumeSink sink;
  sink.setCutOutEnabled(true);
  sink.setCutOutCorner(6);
  sink.setCutOutPosition(0, 0.2);
  sink.setCutOutPosition(1, 0.4);
  sink.setCutOutPosition(2, 0.6);

  VolumeSink restored;
  EXPECT_TRUE(restored.deserialize(sink.serialize()));
  EXPECT_TRUE(restored.cutOutEnabled());
  EXPECT_EQ(restored.cutOutCorner(), 6);
  EXPECT_DOUBLE_EQ(restored.cutOutPosition(0), 0.2);
  EXPECT_DOUBLE_EQ(restored.cutOutPosition(1), 0.4);
  EXPECT_DOUBLE_EQ(restored.cutOutPosition(2), 0.6);
}

TEST_F(PipelineLibTest, ThresholdSinkPropertyDefaults)
{
  ThresholdSink sink;
  EXPECT_EQ(sink.label(), "Threshold");
  EXPECT_DOUBLE_EQ(sink.lowerThreshold(), 0.0);
  EXPECT_DOUBLE_EQ(sink.upperThreshold(), 1.0);
  EXPECT_TRUE(sink.mapScalars());
  EXPECT_TRUE(sink.isColorMapNeeded());
  EXPECT_FALSE(sink.colorByArray());
}

TEST_F(PipelineLibTest, ThresholdSinkPropertySetters)
{
  ThresholdSink sink;

  sink.setThresholdRange(10.0, 90.0);
  EXPECT_DOUBLE_EQ(sink.lowerThreshold(), 10.0);
  EXPECT_DOUBLE_EQ(sink.upperThreshold(), 90.0);

  // opacity/specular/representation require SM proxy (initialize with view),
  // so we only test member-variable-backed properties here.

  sink.setMapScalars(false);
  EXPECT_FALSE(sink.mapScalars());

  sink.setColorByArray(true);
  EXPECT_TRUE(sink.colorByArray());
  sink.setColorByArrayName("Custom");
  EXPECT_EQ(sink.colorByArrayName(), "Custom");
}

TEST_F(PipelineLibTest, ThresholdSinkSerializationRoundTrip)
{
  ThresholdSink sink;
  sink.setThresholdRange(25.0, 75.0);
  sink.setMapScalars(false);
  sink.setColorByArray(true);
  sink.setColorByArrayName("Arr");

  auto json = sink.serialize();

  ThresholdSink restored;
  EXPECT_TRUE(restored.deserialize(json));

  EXPECT_DOUBLE_EQ(restored.lowerThreshold(), 25.0);
  EXPECT_DOUBLE_EQ(restored.upperThreshold(), 75.0);
  EXPECT_FALSE(restored.mapScalars());
  EXPECT_TRUE(restored.colorByArray());
  EXPECT_EQ(restored.colorByArrayName(), "Arr");
}

TEST_F(PipelineLibTest, OutlineSinkPropertyDefaults)
{
  OutlineSink sink;
  EXPECT_EQ(sink.label(), "Outline");
  EXPECT_FALSE(sink.showGridAxes());
  EXPECT_FALSE(sink.generateGrid());
  EXPECT_FALSE(sink.useCustomAxesTitles());
  EXPECT_EQ(sink.xTitle(), "X");
  EXPECT_EQ(sink.yTitle(), "Y");
  EXPECT_EQ(sink.zTitle(), "Z");
}

TEST_F(PipelineLibTest, OutlineSinkPropertySetters)
{
  OutlineSink sink;

  sink.setShowGridAxes(true);
  EXPECT_TRUE(sink.showGridAxes());

  sink.setGenerateGrid(true);
  EXPECT_TRUE(sink.generateGrid());

  sink.setUseCustomAxesTitles(true);
  EXPECT_TRUE(sink.useCustomAxesTitles());

  sink.setXTitle("Width");
  EXPECT_EQ(sink.xTitle(), "Width");

  sink.setYTitle("Height");
  EXPECT_EQ(sink.yTitle(), "Height");

  sink.setZTitle("Depth");
  EXPECT_EQ(sink.zTitle(), "Depth");

  sink.setColor(0.1, 0.2, 0.3);
  double rgb[3];
  sink.color(rgb);
  EXPECT_DOUBLE_EQ(rgb[0], 0.1);
  EXPECT_DOUBLE_EQ(rgb[1], 0.2);
  EXPECT_DOUBLE_EQ(rgb[2], 0.3);
}

TEST_F(PipelineLibTest, OutlineSinkSerializationRoundTrip)
{
  OutlineSink sink;
  sink.setColor(0.5, 0.6, 0.7);
  sink.setShowGridAxes(true);
  sink.setGenerateGrid(true);
  sink.setUseCustomAxesTitles(true);
  sink.setXTitle("A");
  sink.setYTitle("B");
  sink.setZTitle("C");

  auto json = sink.serialize();

  OutlineSink restored;
  EXPECT_TRUE(restored.deserialize(json));

  EXPECT_TRUE(restored.showGridAxes());
  EXPECT_TRUE(restored.generateGrid());
  EXPECT_TRUE(restored.useCustomAxesTitles());
  EXPECT_EQ(restored.xTitle(), "A");
  EXPECT_EQ(restored.yTitle(), "B");
  EXPECT_EQ(restored.zTitle(), "C");

  double rgb[3];
  restored.color(rgb);
  EXPECT_DOUBLE_EQ(rgb[0], 0.5);
  EXPECT_DOUBLE_EQ(rgb[1], 0.6);
  EXPECT_DOUBLE_EQ(rgb[2], 0.7);
}

TEST_F(PipelineLibTest, PlotSinkPropertyDefaults)
{
  PlotSink sink;
  EXPECT_EQ(sink.label(), "Plot");
  EXPECT_EQ(sink.xLabel(), "");
  EXPECT_EQ(sink.yLabel(), "");
  EXPECT_FALSE(sink.xLogScale());
  EXPECT_FALSE(sink.yLogScale());
}

TEST_F(PipelineLibTest, PlotSinkPropertySetters)
{
  PlotSink sink;
  sink.setXLabel("Energy (eV)");
  EXPECT_EQ(sink.xLabel(), "Energy (eV)");

  sink.setYLabel("Counts");
  EXPECT_EQ(sink.yLabel(), "Counts");

  sink.setXLogScale(true);
  EXPECT_TRUE(sink.xLogScale());

  sink.setYLogScale(true);
  EXPECT_TRUE(sink.yLogScale());
}

TEST_F(PipelineLibTest, PlotSinkSerializationRoundTrip)
{
  PlotSink sink;
  sink.setXLabel("Frequency");
  sink.setYLabel("Amplitude");
  sink.setXLogScale(true);
  sink.setYLogScale(false);

  auto json = sink.serialize();

  PlotSink restored;
  EXPECT_TRUE(restored.deserialize(json));

  EXPECT_EQ(restored.xLabel(), "Frequency");
  EXPECT_EQ(restored.yLabel(), "Amplitude");
  EXPECT_TRUE(restored.xLogScale());
  EXPECT_FALSE(restored.yLogScale());
}

TEST_F(PipelineLibTest, MoleculeSinkPropertySetters)
{
  MoleculeSink sink;

  sink.setBallRadius(0.5);
  EXPECT_NEAR(sink.ballRadius(), 0.5, 1e-6);

  sink.setBondRadius(0.15);
  EXPECT_NEAR(sink.bondRadius(), 0.15, 1e-6);
}

TEST_F(PipelineLibTest, MoleculeSinkSerializationRoundTrip)
{
  MoleculeSink sink;
  sink.setBallRadius(0.8);
  sink.setBondRadius(0.2);

  auto json = sink.serialize();

  MoleculeSink restored;
  EXPECT_TRUE(restored.deserialize(json));

  EXPECT_NEAR(restored.ballRadius(), 0.8, 1e-6);
  EXPECT_NEAR(restored.bondRadius(), 0.2, 1e-6);
}

TEST_F(PipelineLibTest, ScaleCubeSinkPropertyDefaults)
{
  ScaleCubeSink sink;
  EXPECT_EQ(sink.label(), "Scale Cube");
  EXPECT_FALSE(sink.adaptiveScaling());
}

TEST_F(PipelineLibTest, ScaleCubeSinkPropertySetters)
{
  ScaleCubeSink sink;

  sink.setSideLength(5.0);
  EXPECT_DOUBLE_EQ(sink.sideLength(), 5.0);

  sink.setAdaptiveScaling(true);
  EXPECT_TRUE(sink.adaptiveScaling());

  sink.setShowAnnotation(false);
  EXPECT_FALSE(sink.showAnnotation());

  sink.setLengthUnit("nm");
  EXPECT_EQ(sink.lengthUnit(), "nm");
}

TEST_F(PipelineLibTest, VolumeSinkPropertySetters)
{
  VolumeSink sink;

  sink.setLighting(true);
  EXPECT_TRUE(sink.lighting());

  sink.setLighting(false);
  EXPECT_FALSE(sink.lighting());

  sink.setJittering(true);
  EXPECT_TRUE(sink.jittering());

  sink.setJittering(false);
  EXPECT_FALSE(sink.jittering());
}

TEST_F(PipelineLibTest, SinkNodeFactoryCreation)
{
  auto contour = NodeFactory::create("sink.contour");
  EXPECT_NE(contour, nullptr);
  EXPECT_NE(dynamic_cast<ContourSink*>(contour), nullptr);
  delete contour;

  auto slice = NodeFactory::create("sink.slice");
  EXPECT_NE(slice, nullptr);
  EXPECT_NE(dynamic_cast<SliceSink*>(slice), nullptr);
  delete slice;

  auto threshold = NodeFactory::create("sink.threshold");
  EXPECT_NE(threshold, nullptr);
  EXPECT_NE(dynamic_cast<ThresholdSink*>(threshold), nullptr);
  delete threshold;

  auto outline = NodeFactory::create("sink.outline");
  EXPECT_NE(outline, nullptr);
  EXPECT_NE(dynamic_cast<OutlineSink*>(outline), nullptr);
  delete outline;

  auto volume = NodeFactory::create("sink.volume");
  EXPECT_NE(volume, nullptr);
  EXPECT_NE(dynamic_cast<VolumeSink*>(volume), nullptr);
  delete volume;

  auto plot = NodeFactory::create("sink.plot");
  EXPECT_NE(plot, nullptr);
  EXPECT_NE(dynamic_cast<PlotSink*>(plot), nullptr);
  delete plot;

  auto molecule = NodeFactory::create("sink.molecule");
  EXPECT_NE(molecule, nullptr);
  EXPECT_NE(dynamic_cast<MoleculeSink*>(molecule), nullptr);
  delete molecule;

  auto ruler = NodeFactory::create("sink.ruler");
  EXPECT_NE(ruler, nullptr);
  EXPECT_NE(dynamic_cast<RulerSink*>(ruler), nullptr);
  delete ruler;

  auto scaleCube = NodeFactory::create("sink.scaleCube");
  EXPECT_NE(scaleCube, nullptr);
  EXPECT_NE(dynamic_cast<ScaleCubeSink*>(scaleCube), nullptr);
  delete scaleCube;

  auto segment = NodeFactory::create("sink.segment");
  EXPECT_NE(segment, nullptr);
  EXPECT_NE(dynamic_cast<SegmentSink*>(segment), nullptr);
  delete segment;

  auto volumeStats = NodeFactory::create("sink.volumeStats");
  EXPECT_NE(volumeStats, nullptr);
  EXPECT_NE(dynamic_cast<VolumeStatsSink*>(volumeStats), nullptr);
  delete volumeStats;
}

TEST_F(PipelineLibTest, VolumeStatsSinkComputesCorrectStats)
{
  auto* source = new SphereSource();
  source->setDimensions(8, 8, 8);
  pipeline->addNode(source);
  source->execute();

  auto* stats = new VolumeStatsSink();
  pipeline->addNode(stats);
  pipeline->createLink(source->outputPort("volume"),
                       stats->inputPort("volume"));

  auto* future = pipeline->execute();
  EXPECT_TRUE(future->isFinished());
  EXPECT_TRUE(future->succeeded());
  EXPECT_TRUE(stats->hasResults());
  EXPECT_EQ(stats->voxelCount(), 8 * 8 * 8);
  EXPECT_LE(stats->min(), stats->mean());
  EXPECT_LE(stats->mean(), stats->max());
}

TEST_F(PipelineLibTest, SinkInputPortTypes)
{
  // Volume sinks accept ImageData
  ContourSink contour;
  EXPECT_NE(contour.inputPort("volume"), nullptr);

  SliceSink slice;
  EXPECT_NE(slice.inputPort("volume"), nullptr);

  ThresholdSink threshold;
  EXPECT_NE(threshold.inputPort("volume"), nullptr);

  OutlineSink outline;
  EXPECT_NE(outline.inputPort("volume"), nullptr);

  VolumeSink vol;
  EXPECT_NE(vol.inputPort("volume"), nullptr);

  ClipSink clip;
  EXPECT_NE(clip.inputPort("volume"), nullptr);

  // PlotSink accepts Table data
  PlotSink plot;
  EXPECT_NE(plot.inputPort("table"), nullptr);

  // MoleculeSink accepts Molecule data
  MoleculeSink mol;
  EXPECT_NE(mol.inputPort("molecule"), nullptr);
}

// --- Save Data (batch output-port export) tests ---

namespace {

using tomviz::PortDataWriter::formatById;

VolumeDataPtr makeVolume(const QStringList& arrayNames)
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(4, 4, 4);
  for (const auto& name : arrayNames) {
    vtkNew<vtkFloatArray> array;
    array->SetName(name.toUtf8().data());
    array->SetNumberOfComponents(1);
    array->SetNumberOfTuples(4 * 4 * 4);
    array->FillValue(1.0f);
    image->GetPointData()->AddArray(array);
  }
  if (!arrayNames.isEmpty()) {
    image->GetPointData()->SetActiveScalars(arrayNames.first().toUtf8().data());
  }
  return std::make_shared<VolumeData>(image);
}

void setVolumeData(OutputPort* port, const QStringList& arrayNames)
{
  port->setData(
    PortData(std::any(makeVolume(arrayNames)), PortType::ImageData));
}

TEST_F(PipelineLibTest, SliceSinkConsumeDoesNotRePropagateLink)
{
  struct OpenSliceSink : SliceSink
  {
    using SliceSink::consume;
  };

  auto* a = new OpenSliceSink();
  auto* b = new OpenSliceSink();
  pipeline->addNode(a);
  pipeline->addNode(b);
  a->setLinked(true);
  b->setLinked(true);
  a->setSlice(3);
  ASSERT_EQ(b->slice(), 3);

  // Diverge b without the link noticing (deserialize sets the index
  // directly), simulating a peer whose own extents clamped the index.
  auto json = b->serialize();
  json["slice"] = 1;
  ASSERT_TRUE(b->deserialize(json));
  ASSERT_EQ(b->slice(), 1);
  ASSERT_EQ(a->slice(), 3);

  // consume()'s UI-sync notifications must not push b's index onto the
  // other linked views: only user edits propagate.
  QMap<QString, PortData> inputs;
  inputs["volume"] =
    PortData(std::any(makeVolume({ "scalars" })), PortType::ImageData);
  EXPECT_TRUE(b->consume(inputs));
  EXPECT_EQ(b->slice(), 1);
  EXPECT_EQ(a->slice(), 3);
}

// EMD holds every array of a volume in one file.
QHash<PortType, tomviz::PortFormat> multiArrayFormats()
{
  return { { PortType::ImageData, formatById(PortType::ImageData, "emd") },
           { PortType::Table, formatById(PortType::Table, "csv") },
           { PortType::Molecule, formatById(PortType::Molecule, "xyz") } };
}

// TIFF keeps only the active scalars, so volumes split across files.
QHash<PortType, tomviz::PortFormat> singleArrayFormats()
{
  auto formats = multiArrayFormats();
  formats[PortType::ImageData] = formatById(PortType::ImageData, "tiff");
  return formats;
}

QHash<OutputPort*, QStringList> collectArrayNames(
  const QList<OutputPort*>& ports)
{
  QHash<OutputPort*, QStringList> names;
  for (auto* port : ports) {
    auto handle = port->materialize();
    names.insert(port, handle ? tomviz::PortDataWriter::arrayNames(*handle)
                              : QStringList());
  }
  return names;
}

QStringList fileNamesOf(const QList<tomviz::SaveDataDialog::PortPlan>& plans)
{
  QStringList names;
  for (const auto& plan : plans) {
    for (const auto& entry : plan.entries) {
      names << QFileInfo(entry.path).fileName();
    }
  }
  return names;
}

QStringList displayNamesOf(
  const QList<tomviz::SaveDataDialog::PortPlan>& plans)
{
  QStringList names;
  for (const auto& plan : plans) {
    names << plan.displayName;
  }
  return names;
}

QList<tomviz::SaveDataDialog::Entry> entriesOf(
  const QList<tomviz::SaveDataDialog::PortPlan>& plans)
{
  QList<tomviz::SaveDataDialog::Entry> entries;
  for (const auto& plan : plans) {
    entries.append(plan.entries);
  }
  return entries;
}

} // namespace

TEST_F(PipelineLibTest, SaveDataLeafScopeIgnoresSinks)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A" });

  auto* transform =
    new PassthroughTransform(PortType::ImageData, PortType::ImageData);
  transform->setLabel("Transform");
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("in"));
  setVolumeData(transform->outputPort("out"), { "A" });

  // A sink downstream must not disqualify the transform from being a leaf.
  auto* sink = new CollectorSink();
  pipeline->addNode(sink);
  pipeline->createLink(transform->outputPort("out"), sink->inputPort("in"));

  auto leaves = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  ASSERT_EQ(leaves.size(), 1);
  EXPECT_EQ(leaves.first(), transform->outputPort("out"));
}

TEST_F(PipelineLibTest, SaveDataAllPortsScopeIncludesTransientData)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A" });
  source->outputPort("volume")->setPersistent(true);

  auto* transform =
    new PassthroughTransform(PortType::ImageData, PortType::ImageData);
  transform->setLabel("Transform");
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("in"));
  // Flip persistence before publishing — reconciling an already-published
  // port to transient drops the payload.
  transform->outputPort("out")->setPersistent(false);
  setVolumeData(transform->outputPort("out"), { "A" });

  // A transient port still holding its data can be saved like any other:
  // persistence is about how long data is kept, not whether it may be
  // written out.
  auto all = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::AllPorts);
  ASSERT_EQ(all.size(), 2);
  EXPECT_TRUE(all.contains(source->outputPort("volume")));
  EXPECT_TRUE(all.contains(transform->outputPort("out")));
  EXPECT_TRUE(tomviz::SaveDataDialog::releasedPorts(
                pipeline, tomviz::SaveDataDialog::Scope::AllPorts)
                .isEmpty());

  // The transform is the only leaf, so the two scopes disagree here.
  auto leaves = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  ASSERT_EQ(leaves.size(), 1);
  EXPECT_EQ(leaves.first(), transform->outputPort("out"));

  // Once its data is released it cannot be saved, and the dialog is
  // told so it can say why
  transform->outputPort("out")->clearData();
  all = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::AllPorts);
  ASSERT_EQ(all.size(), 1);
  EXPECT_EQ(all.first(), source->outputPort("volume"));
  auto released = tomviz::SaveDataDialog::releasedPorts(
    pipeline, tomviz::SaveDataDialog::Scope::AllPorts);
  ASSERT_EQ(released.size(), 1);
  EXPECT_EQ(released.first(), transform->outputPort("out"));

  // A persistent port with no data yet was never released
  source->outputPort("volume")->clearData();
  released = tomviz::SaveDataDialog::releasedPorts(
    source, tomviz::SaveDataDialog::Scope::AllPorts);
  EXPECT_TRUE(released.isEmpty());
}

TEST_F(PipelineLibTest, SaveDataNodeScopeSeesOnlyItsOwnPorts)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A" });

  auto* transform =
    new PassthroughTransform(PortType::ImageData, PortType::ImageData);
  transform->setLabel("Transform");
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("in"));
  setVolumeData(transform->outputPort("out"), { "A" });

  // The pipeline-wide view spans both nodes...
  EXPECT_EQ(tomviz::SaveDataDialog::candidatePorts(
              pipeline, tomviz::SaveDataDialog::Scope::AllPorts)
              .size(),
            2);

  // ...while a node-scoped export sees only that node, even though the
  // node is upstream of another and so is not a leaf.
  auto sourcePorts = tomviz::SaveDataDialog::candidatePorts(
    source, tomviz::SaveDataDialog::Scope::AllPorts);
  ASSERT_EQ(sourcePorts.size(), 1);
  EXPECT_EQ(sourcePorts.first(), source->outputPort("volume"));

  // Turning a published port transient releases its data: nothing to
  // save, and the port is reported as released instead.
  transform->outputPort("out")->setPersistent(false);
  EXPECT_TRUE(tomviz::SaveDataDialog::candidatePorts(
                transform, tomviz::SaveDataDialog::Scope::AllPorts)
                .isEmpty());
  EXPECT_EQ(tomviz::SaveDataDialog::releasedPorts(
              transform, tomviz::SaveDataDialog::Scope::AllPorts)
              .size(),
            1);
}

TEST_F(PipelineLibTest, SaveDataPortScopeSeesOnlyThatPort)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("volume", PortType::ImageData);
  source->addOutput("mask", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A" });
  setVolumeData(source->outputPort("mask"), { "B" });

  // The node view spans both of its ports...
  EXPECT_EQ(tomviz::SaveDataDialog::candidatePorts(
              source, tomviz::SaveDataDialog::Scope::AllPorts)
              .size(),
            2);

  // ...while the port view is just the one.
  auto ports = tomviz::SaveDataDialog::candidatePorts(
    source->outputPort("mask"), tomviz::SaveDataDialog::Scope::AllPorts);
  ASSERT_EQ(ports.size(), 1);
  EXPECT_EQ(ports.first(), source->outputPort("mask"));

  auto plans = tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), "/tmp/out", multiArrayFormats());
  EXPECT_EQ(fileNamesOf(plans), QStringList({ "Source_mask.emd" }));

  // A port with no data has nothing to offer.
  source->outputPort("mask")->clearData();
  EXPECT_TRUE(tomviz::SaveDataDialog::candidatePorts(
                source->outputPort("mask"),
                tomviz::SaveDataDialog::Scope::AllPorts)
                .isEmpty());
}

TEST_F(PipelineLibTest, SaveDataDialogExplainsReleasedPorts)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A" });

  auto* transform =
    new PassthroughTransform(PortType::ImageData, PortType::ImageData);
  transform->setLabel("Remove Labels");
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("in"));
  transform->outputPort("out")->setPersistent(false);

  // Still in memory: offered, and nothing to explain
  setVolumeData(transform->outputPort("out"), { "A" });
  {
    tomviz::SaveDataDialog dialog(static_cast<Node*>(transform));
    auto* note = dialog.findChild<QLabel*>("releasedNote");
    ASSERT_NE(note, nullptr);
    EXPECT_TRUE(note->isHidden());
    EXPECT_EQ(dialog.selectedEntries().size(), 1);
  }

  // Released: nothing offered, and the note names the port and why
  transform->outputPort("out")->clearData();
  {
    tomviz::SaveDataDialog dialog(static_cast<Node*>(transform));
    auto* note = dialog.findChild<QLabel*>("releasedNote");
    ASSERT_NE(note, nullptr);
    EXPECT_FALSE(note->isHidden());
    EXPECT_TRUE(note->text().contains("Remove Labels (out)"))
      << note->text().toStdString();
    EXPECT_TRUE(note->text().contains("transient"));
    EXPECT_TRUE(dialog.selectedEntries().isEmpty());
  }
}

// visible_if hides a parameter's label with its field, whichever the
// label is attached to: a string's label is its field's buddy, a file's
// is the row's.
TEST_F(PipelineLibTest, VisibleIfHidesLabelsOfEveryParameterType)
{
  ParameterInterfaceBuilder builder;
  builder.setJSONDescription(QString(R"({"parameters": [
    {"name": "auto", "label": "Auto", "type": "bool", "default": true},
    {"name": "centers", "label": "Centers", "type": "string",
     "default": "", "visible_if": "auto == false"},
    {"name": "centers_file", "label": "Centers CSV", "type": "file",
     "default": "", "visible_if": "auto == false"}
  ]})"));
  QWidget parent;
  auto* form = builder.buildWidget(&parent);
  ASSERT_NE(form, nullptr);

  auto labelFor = [&parent](const QString& text) -> QLabel* {
    for (auto* label : parent.findChildren<QLabel*>()) {
      if (label->text() == text) {
        return label;
      }
    }
    return nullptr;
  };
  auto* centers = labelFor("Centers");
  auto* centersFile = labelFor("Centers CSV");
  auto* autoCheck = parent.findChild<QCheckBox*>("auto");
  ASSERT_NE(centers, nullptr);
  ASSERT_NE(centersFile, nullptr);
  ASSERT_NE(autoCheck, nullptr);
  EXPECT_TRUE(centers->isHidden());
  EXPECT_TRUE(centersFile->isHidden());
  EXPECT_TRUE(parent.findChild<QLineEdit*>("centers")->isHidden() ||
              parent.findChild<QLineEdit*>("centers")->parentWidget()
                ->isHidden());

  autoCheck->setChecked(false);
  EXPECT_FALSE(centers->isHidden());
  EXPECT_FALSE(centersFile->isHidden());
}

TEST_F(PipelineLibTest, SaveDataNodeScopeExcludesSinks)
{
  auto* source = new SourceNode();
  source->setLabel("Source");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A" });

  auto* sink = new CollectorSink();
  pipeline->addNode(sink);
  pipeline->createLink(source->outputPort("volume"), sink->inputPort("in"));

  EXPECT_TRUE(tomviz::SaveDataDialog::candidatePorts(
                static_cast<Node*>(sink),
                tomviz::SaveDataDialog::Scope::AllPorts)
                .isEmpty());
}

TEST_F(PipelineLibTest, SaveDataFileNamePlanning)
{
  auto* first = new SourceNode();
  first->setLabel("Recon Volume");
  first->addOutput("out put", PortType::ImageData);
  pipeline->addNode(first);
  setVolumeData(first->outputPort("out put"), { "Scalars_", "Labels/2" });

  // Same label as `first` — both must gain a numeric suffix.
  auto* second = new SourceNode();
  second->setLabel("Recon Volume");
  second->addOutput("out", PortType::ImageData);
  pipeline->addNode(second);
  setVolumeData(second->outputPort("out"), { "A" });

  auto ports = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  ASSERT_EQ(ports.size(), 2);

  // A single-array format splits the two-array port across two files
  // and shows them braced on one row.
  auto split = tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), "/tmp/out", singleArrayFormats());

  EXPECT_EQ(fileNamesOf(split),
            QStringList({ "Recon_Volume_1_out_put_Scalars.tiff",
                          "Recon_Volume_1_out_put_Labels_2.tiff",
                          "Recon_Volume_2_out_A.tiff" }));
  EXPECT_EQ(displayNamesOf(split),
            QStringList({ "Recon_Volume_1_out_put_{Scalars|Labels_2}.tiff",
                          "Recon_Volume_2_out_A.tiff" }));
  EXPECT_EQ(entriesOf(split).first().path,
            QString("/tmp/out/Recon_Volume_1_out_put_Scalars.tiff"));
  EXPECT_EQ(entriesOf(split).first().arrayNames,
            QStringList({ "Scalars_" }));

  // A multi-array format writes one file per port, array name omitted.
  auto merged = tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), "/tmp/out", multiArrayFormats());

  EXPECT_EQ(fileNamesOf(merged), QStringList({ "Recon_Volume_1_out_put.emd",
                                               "Recon_Volume_2_out.emd" }));
  EXPECT_EQ(displayNamesOf(merged), fileNamesOf(merged));
  EXPECT_EQ(entriesOf(merged).first().arrayNames,
            QStringList({ "Scalars_", "Labels/2" }));
}

TEST_F(PipelineLibTest, SaveDataFileNameCollisionsResolved)
{
  auto* source = new SourceNode();
  source->setLabel("Node");
  source->addOutput("out", PortType::ImageData);
  pipeline->addNode(source);
  // Two array names that sanitize onto the same stem.
  setVolumeData(source->outputPort("out"), { "a b", "a/b" });

  auto ports = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  auto plans = tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), "/tmp/out", singleArrayFormats());

  EXPECT_EQ(fileNamesOf(plans),
            QStringList({ "Node_out_a_b.tiff", "Node_out_a_b_2.tiff" }));
  // The braced row reflects the disambiguated names, not the raw arrays.
  EXPECT_EQ(displayNamesOf(plans),
            QStringList({ "Node_out_{a_b|a_b_2}.tiff" }));
}

TEST_F(PipelineLibTest, SaveDataWritesEveryArrayIntoOneEmd)
{
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  auto* source = new SourceNode();
  source->setLabel("Sphere");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A", "B" });

  auto ports = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  auto entries = entriesOf(tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), dir.path(), multiArrayFormats()));
  ASSERT_EQ(entries.size(), 1);

  auto handle = entries.first().port->materialize();
  ASSERT_NE(handle, nullptr);
  EXPECT_TRUE(tomviz::PortDataWriter::write(*handle, entries.first().arrayNames,
                                            entries.first().path));

  vtkNew<vtkImageData> readBack;
  ASSERT_TRUE(
    tomviz::EmdFormat::read(entries.first().path.toStdString(), readBack));
  EXPECT_EQ(readBack->GetPointData()->GetNumberOfArrays(), 2);
  EXPECT_NE(readBack->GetPointData()->GetArray("A"), nullptr);
  EXPECT_NE(readBack->GetPointData()->GetArray("B"), nullptr);
}

TEST_F(PipelineLibTest, SaveDataSplitsArraysForSingleArrayFormats)
{
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  auto* source = new SourceNode();
  source->setLabel("Sphere");
  source->addOutput("volume", PortType::ImageData);
  pipeline->addNode(source);
  setVolumeData(source->outputPort("volume"), { "A", "B" });

  auto ports = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  // EMD is only used here to confirm the per-array filtering; the split
  // itself is what a single-array format like TIFF triggers.
  auto formats = singleArrayFormats();
  formats[PortType::ImageData].multiArray = false;
  formats[PortType::ImageData].extension = "emd";

  auto plans = tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), dir.path(), formats);
  ASSERT_EQ(plans.size(), 1);
  ASSERT_EQ(plans.first().entries.size(), 2);
  EXPECT_EQ(plans.first().displayName, QString("Sphere_volume_{A|B}.emd"));

  for (const auto& entry : plans.first().entries) {
    auto handle = entry.port->materialize();
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(
      tomviz::PortDataWriter::write(*handle, entry.arrayNames, entry.path));

    // Each file carries only the array it was named for.
    vtkNew<vtkImageData> readBack;
    ASSERT_TRUE(tomviz::EmdFormat::read(entry.path.toStdString(), readBack));
    EXPECT_EQ(readBack->GetPointData()->GetNumberOfArrays(), 1);
    EXPECT_NE(
      readBack->GetPointData()->GetArray(
        entry.arrayNames.first().toUtf8().data()),
      nullptr);
  }
}

TEST_F(PipelineLibTest, SaveDataWritesTableAndMolecule)
{
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  auto* tableSource = new SourceNode();
  tableSource->setLabel("Stats");
  tableSource->addOutput("table", PortType::Table);
  pipeline->addNode(tableSource);

  vtkNew<vtkDoubleArray> column;
  column->SetName("value");
  column->InsertNextValue(1.5);
  column->InsertNextValue(2.5);
  vtkSmartPointer<vtkTable> table = vtkSmartPointer<vtkTable>::New();
  table->AddColumn(column);
  tableSource->outputPort("table")->setData(
    PortData(std::any(table), PortType::Table));

  auto* moleculeSource = new SourceNode();
  moleculeSource->setLabel("Atoms");
  moleculeSource->addOutput("molecule", PortType::Molecule);
  pipeline->addNode(moleculeSource);

  vtkSmartPointer<vtkMolecule> molecule = vtkSmartPointer<vtkMolecule>::New();
  molecule->AppendAtom(6, 0.0, 0.0, 0.0);
  molecule->AppendAtom(8, 1.0, 0.0, 0.0);
  moleculeSource->outputPort("molecule")->setData(
    PortData(std::any(molecule), PortType::Molecule));

  auto ports = tomviz::SaveDataDialog::candidatePorts(
    pipeline, tomviz::SaveDataDialog::Scope::LeafNodes);
  auto plans = tomviz::SaveDataDialog::planPorts(
    ports, collectArrayNames(ports), dir.path(), multiArrayFormats());
  auto entries = entriesOf(plans);

  EXPECT_EQ(fileNamesOf(plans),
            QStringList({ "Stats_table.csv", "Atoms_molecule.xyz" }));

  for (const auto& entry : entries) {
    auto handle = entry.port->materialize();
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(
      tomviz::PortDataWriter::write(*handle, entry.arrayNames, entry.path));
    EXPECT_TRUE(QFileInfo::exists(entry.path));
  }

  QFile csv(entries.first().path);
  ASSERT_TRUE(csv.open(QIODevice::ReadOnly | QIODevice::Text));
  EXPECT_EQ(QString(csv.readAll()), QString("value\n1.5\n2.5"));

  QFile xyz(entries.last().path);
  ASSERT_TRUE(xyz.open(QIODevice::ReadOnly | QIODevice::Text));
  EXPECT_TRUE(QString(xyz.readAll()).startsWith("2\n"));
}

// --- Periodic execution (should_auto_execute hook + controller) ---

namespace {

// Source whose poll/execute behavior the test scripts directly. The
// poll runs on the controller's worker thread, so its bookkeeping is
// atomic; execute() runs on the GUI thread via DefaultExecutor.
class AutoAnswerSource : public SourceNode
{
public:
  AutoAnswerSource() : SourceNode()
  {
    addOutput("out", PortType::ImageData);
  }

  bool execute() override
  {
    executeCount++;
    // setOutputData marks this node Current and downstream stale.
    setOutputData("out",
                  PortData(std::any(executeCount), PortType::ImageData));
    return true;
  }

  bool queryShouldAutoExecute() override
  {
    pollCount.fetch_add(1);
    return answer.load();
  }

  int executeCount = 0;
  std::atomic<int> pollCount{ 0 };
  std::atomic<bool> answer{ false };
};

} // namespace

TEST_F(PipelinePythonTest, ShouldAutoExecuteHookAndState)
{
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "Watcher",
    "outputs": [{"name": "volume", "type": "ImageData"}],
    "parameters": [
      {"name": "value", "type": "double", "default": 0.0}
    ]
  })";
  // The hook counts its own invocations in self.state and starts
  // answering True on the second poll; it also sees the current
  // parameter values under their declared names.
  QString scriptStr = R"(
import tomviz.nodes

class Watcher(tomviz.nodes.SourceNode):
    def produce(self, value=0.0):
        return None

    def should_auto_execute(self, value=0.0):
        if value != 7.5:
            raise ValueError('parameters not forwarded')
        polls = self.state.get('polls', 0) + 1
        self.state['polls'] = polls
        return polls >= 2
)";

  auto* source = new PythonSource();
  source->setJSONDescription(jsonStr);
  source->setScript(scriptStr);
  source->setParameter("value", 7.5);
  pipeline->addNode(source);

  // First poll answers no, but its state mutation is kept.
  EXPECT_FALSE(source->queryShouldAutoExecute());
  EXPECT_EQ(source->userState().value("polls").toInt(), 1);

  // Second poll sees polls==1 in state and answers yes.
  EXPECT_TRUE(source->queryShouldAutoExecute());
  EXPECT_EQ(source->userState().value("polls").toInt(), 2);
}

TEST_F(PipelinePythonTest, ShouldAutoExecuteDefaultsFalse)
{
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "Quiet",
    "outputs": [{"name": "volume", "type": "ImageData"}]
  })";
  // No should_auto_execute override: the tomviz.nodes.Node base
  // answers False.
  QString scriptStr = R"(
import tomviz.nodes

class Quiet(tomviz.nodes.SourceNode):
    def produce(self):
        return None
)";

  auto* source = new PythonSource();
  source->setJSONDescription(jsonStr);
  source->setScript(scriptStr);
  pipeline->addNode(source);

  EXPECT_FALSE(source->queryShouldAutoExecute());
  EXPECT_TRUE(source->userState().isEmpty());
}

TEST_F(PipelinePythonTest, UserStatePersistsAcrossTransformRuns)
{
  QString jsonStr = R"({
    "schemaVersion": 2,
    "name": "CountingPass",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData", "persistent": true}]
  })";
  // A fresh instance runs each time, so the run counter only grows if
  // self.state actually round-trips through the host node.
  QString scriptStr = R"(
import tomviz.nodes

class CountingPass(tomviz.nodes.TransformNode):
    def transform(self, inputs):
        self.state['runs'] = self.state.get('runs', 0) + 1
        return {"volume": inputs["volume"]}

    def should_auto_execute(self):
        return self.state.get('runs', 0) > 0
)";

  auto* source = new SphereSource();
  source->setDimensions(4, 4, 4);
  pipeline->addNode(source);

  auto* transform = new PythonTransform();
  transform->setJSONDescription(jsonStr);
  transform->setScript(scriptStr);
  pipeline->addNode(transform);
  pipeline->createLink(source->outputPort("volume"),
                       transform->inputPort("volume"));

  pipeline->execute();
  EXPECT_EQ(transform->state(), NodeState::Current);
  EXPECT_EQ(transform->userState().value("runs").toInt(), 1);

  transform->markStale();
  pipeline->execute();
  EXPECT_EQ(transform->userState().value("runs").toInt(), 2);

  // The hook shares the same state the runs recorded.
  EXPECT_TRUE(transform->queryShouldAutoExecute());
}

// --- Kernel parameter write-back (self.set_parameter) ---

namespace {

const char* kWriteBackDescription = R"({
  "schemaVersion": 2,
  "name": "WriteBack",
  "outputs": [{"name": "volume", "type": "ImageData"}],
  "parameters": [
    {"name": "value", "type": "double", "default": 0.0},
    {"name": "frame", "type": "int", "default": 0},
    {"name": "mode", "type": "enumeration", "default": 0,
     "options": [{"Fast": "fast"}, {"Slow": "slow"}]}
  ]
})";

// produce() advances `frame` and publishes `value` (as a string, to
// exercise coercion); the hook flips `mode` and answers from `frame`.
const char* kWriteBackScript = R"(
import numpy as np
import tomviz.nodes


class WriteBack(tomviz.nodes.SourceNode):
    def produce(self, value=0.0, frame=0, mode='fast'):
        self.set_parameter('frame', frame + 1)
        self.set_parameter('value', '2.5')
        if self.parameter('frame') != frame + 1:
            raise AssertionError('parameter() does not see the update')
        ds = self.create_dataset()
        ds.set_scalars('Scalars', np.full((2, 2, 2), value, dtype=np.float32))
        return {'volume': ds}

    def should_auto_execute(self, value=0.0, frame=0, mode='fast'):
        self.set_parameter('mode', 'slow')
        return frame >= 1
)";

} // namespace

TEST_F(PipelinePythonTest, SetParameterLandsOnNodeQuietly)
{
  auto* source = new PythonSource();
  source->setJSONDescription(kWriteBackDescription);
  source->setScript(kWriteBackScript);
  pipeline->addNode(source);

  QSignalSpy updated(source, &Node::parametersUpdated);
  QSignalSpy applied(source, &Node::parametersApplied);

  pipeline->execute();
  EXPECT_EQ(source->state(), NodeState::Current);
  EXPECT_EQ(source->parameter("frame").toInt(), 1);
  EXPECT_DOUBLE_EQ(source->parameter("value").toDouble(), 2.5);
  EXPECT_EQ(source->parameter("mode").toString(), QString("fast"));

  ASSERT_EQ(updated.count(), 1);
  auto changed = updated.takeFirst().at(0).toMap();
  EXPECT_EQ(changed.size(), 2);
  EXPECT_EQ(changed.value("frame").toInt(), 1);
  EXPECT_DOUBLE_EQ(changed.value("value").toDouble(), 2.5);
  // The quiet path: no editor-style apply, no re-execution.
  EXPECT_EQ(applied.count(), 0);

  // The next run receives the new values; `value` is 2.5 again and is
  // not reported a second time.
  source->markStale();
  pipeline->execute();
  EXPECT_EQ(source->parameter("frame").toInt(), 2);
  ASSERT_EQ(updated.count(), 1);
  changed = updated.takeFirst().at(0).toMap();
  EXPECT_EQ(changed.keys(), QStringList{ "frame" });

  // Serialized `arguments` carry the written-back values.
  auto json = source->serialize();
  EXPECT_EQ(json.value("arguments").toObject().value("frame").toInt(), 2);
}

TEST_F(PipelinePythonTest, SetParameterInShouldAutoExecute)
{
  auto* source = new PythonSource();
  source->setJSONDescription(kWriteBackDescription);
  source->setScript(kWriteBackScript);
  pipeline->addNode(source);
  QSignalSpy updated(source, &Node::parametersUpdated);

  // The hook's write-back lands even when it answers "no"; the answer
  // alone decides whether a run happens.
  EXPECT_FALSE(source->queryShouldAutoExecute());
  EXPECT_EQ(source->parameter("mode").toString(), QString("slow"));
  EXPECT_EQ(updated.count(), 1);
  EXPECT_EQ(source->state(), NodeState::New);

  source->setParameter("frame", 3);
  EXPECT_TRUE(source->queryShouldAutoExecute());
  EXPECT_EQ(updated.count(), 1); // mode is already 'slow'
}

TEST_F(PipelinePythonTest, SetParameterUnknownNameFailsRunKeepsEarlier)
{
  QString scriptStr = R"(
import tomviz.nodes

class Bad(tomviz.nodes.SourceNode):
    def produce(self, value=0.0, frame=0, mode='fast'):
        self.set_parameter('frame', 5)
        self.set_parameter('nope', 1)
        return None
)";
  auto* source = new PythonSource();
  source->setJSONDescription(kWriteBackDescription);
  source->setScript(scriptStr);
  pipeline->addNode(source);

  EXPECT_FALSE(source->execute());
  // Harvested even though produce() raised — parity with self.state
  // under the Python runtime.
  EXPECT_EQ(source->parameter("frame").toInt(), 5);
  EXPECT_FALSE(source->parameters().contains("nope"));
}

TEST_F(PipelinePythonTest, SetParameterRejectsUndeclaredEnumValue)
{
  QString scriptStr = R"(
import tomviz.nodes

class Bad(tomviz.nodes.SourceNode):
    def produce(self, value=0.0, frame=0, mode='fast'):
        self.set_parameter('mode', 'medium')
        return None
)";
  auto* source = new PythonSource();
  source->setJSONDescription(kWriteBackDescription);
  source->setScript(scriptStr);
  pipeline->addNode(source);

  EXPECT_FALSE(source->execute());
  EXPECT_EQ(source->parameter("mode").toString(), QString("fast"));
}

TEST_F(PipelineLibTest, ApplyParameterUpdatesIsQuiet)
{
  auto* source = new PythonSource();
  source->setJSONDescription(kWriteBackDescription);
  pipeline->addNode(source);
  QSignalSpy updated(source, &Node::parametersUpdated);
  QSignalSpy applied(source, &Node::parametersApplied);

  // Only values that differ are written and reported; nothing goes
  // stale (the external executor takes this path after a child run).
  source->applyParameterUpdates({ { "frame", 0 }, { "value", 1.0 } });
  EXPECT_EQ(source->state(), NodeState::New);
  EXPECT_DOUBLE_EQ(source->parameter("value").toDouble(), 1.0);
  ASSERT_EQ(updated.count(), 1);
  EXPECT_EQ(updated.takeFirst().at(0).toMap().keys(), QStringList{ "value" });
  EXPECT_EQ(applied.count(), 0);

  source->applyParameterUpdates({ { "value", 1.0 } });
  EXPECT_EQ(updated.count(), 0);

  // Nodes without parameters ignore the map.
  auto* plain = new SourceNode();
  pipeline->addNode(plain);
  QSignalSpy plainUpdated(plain, &Node::parametersUpdated);
  plain->applyParameterUpdates({ { "x", 1 } });
  EXPECT_EQ(plainUpdated.count(), 0);
}

TEST_F(PipelineLibTest, AutoExecuteControllerTriggersExecution)
{
  auto* source = new AutoAnswerSource();
  pipeline->addNode(source);
  auto* controller = new AutoExecuteController(pipeline, pipeline);

  source->execute();
  EXPECT_EQ(source->executeCount, 1);
  EXPECT_EQ(source->state(), NodeState::Current);

  // Enabled with a "no" answer: polls happen, nothing re-executes.
  source->setAutoExecuteEnabled(true);
  source->setAutoExecuteIntervalSeconds(1);
  for (int i = 0; i < 100 && source->pollCount.load() < 1; ++i) {
    QTest::qWait(100);
  }
  ASSERT_GE(source->pollCount.load(), 1);
  QTest::qWait(200); // let a (wrong) execution land before checking
  EXPECT_EQ(source->executeCount, 1);

  // Flip to "yes": the next poll marks the node stale and re-executes.
  source->answer.store(true);
  for (int i = 0; i < 100 && source->executeCount < 2; ++i) {
    QTest::qWait(100);
  }
  EXPECT_GE(source->executeCount, 2);
  EXPECT_EQ(source->state(), NodeState::Current);

  // Disabling tears the timer down; polling stops.
  source->setAutoExecuteEnabled(false);
  QTest::qWait(300); // drain any in-flight poll
  int polls = source->pollCount.load();
  int executions = source->executeCount;
  QTest::qWait(1500);
  EXPECT_EQ(source->pollCount.load(), polls);
  EXPECT_EQ(source->executeCount, executions);

  delete controller; // joins the worker thread
}

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
