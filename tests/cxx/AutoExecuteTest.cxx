/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "AutoExecuteController.h"
#include "EditNodeWidget.h"
#include "Node.h"
#include "Pipeline.h"
#include "transforms/LegacyPythonTransform.h"
#include "transforms/PythonTransform.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSpinBox>
#include <QThread>

#include <atomic>

#include "TomvizTest.h"

using tomviz::pipeline::AutoExecuteController;
using tomviz::pipeline::EditNodeWidget;
using tomviz::pipeline::LegacyPythonTransform;
using tomviz::pipeline::Node;
using tomviz::pipeline::Pipeline;
using tomviz::pipeline::PythonTransform;

namespace {

const char* kV2Description = R"({
  "schemaVersion": 2,
  "name": "MultiplyBy",
  "label": "Multiply By",
  "inputs":  [{"name": "volume", "type": "ImageData"}],
  "outputs": [{"name": "volume", "type": "ImageData"}],
  "parameters": [
    {"name": "factor", "type": "double", "default": 1.0}
  ]
})";

// No "schemaVersion" → v1 (legacy operator API, no
// should_auto_execute hook).
const char* kV1Description = R"({
  "name": "LegacyOp",
  "label": "Legacy Op",
  "parameters": []
})";

// A slow should_auto_execute() query, which must not outlive its node
class SlowQueryNode : public Node
{
public:
  std::atomic<bool> querying{ false };

  bool queryShouldAutoExecute() override
  {
    querying = true;
    QThread::msleep(300);
    querying = false;
    return false;
  }

  ~SlowQueryNode() override { EXPECT_FALSE(querying); }
};

} // namespace

TEST(AutoExecuteTest, RemovingANodeWaitsForItsQuery)
{
  tomviz_test::ensureQApp();

  Pipeline pipeline;
  AutoExecuteController controller(&pipeline);
  auto* node = new SlowQueryNode;
  node->setAutoExecuteIntervalSeconds(1);
  node->setAutoExecuteEnabled(true);
  pipeline.addNode(node);

  for (int i = 0; i < 1000 && !node->querying; ++i) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
  ASSERT_TRUE(node->querying);
  pipeline.removeNode(node);
}

TEST(AutoExecuteTest, EditorShowsControlsForV2AndAppliesToNode)
{
  tomviz_test::ensureQApp();

  Pipeline pipeline;
  auto* transform = new PythonTransform();
  transform->setJSONDescription(kV2Description);
  transform->setScript("import tomviz.nodes\n"
                       "class MultiplyBy(tomviz.nodes.TransformNode):\n"
                       "    def transform(self, inputs, factor=1.0):\n"
                       "        return None\n");
  pipeline.addNode(transform);

  EXPECT_FALSE(transform->autoExecuteEnabled());
  EXPECT_EQ(transform->autoExecuteIntervalSeconds(),
            Node::kDefaultAutoExecuteIntervalSeconds);

  auto* editor = transform->createPropertiesWidget(&pipeline, nullptr);
  auto* check = editor->findChild<QCheckBox*>("autoExecuteCheck");
  auto* spin = editor->findChild<QSpinBox*>("autoExecuteIntervalSpin");
  ASSERT_NE(check, nullptr);
  ASSERT_NE(spin, nullptr);

  // Off by default, with the interval control gated on the checkbox.
  EXPECT_FALSE(check->isChecked());
  EXPECT_FALSE(spin->isEnabled());
  EXPECT_EQ(spin->value(), Node::kDefaultAutoExecuteIntervalSeconds);

  QSignalSpy spy(transform, &Node::autoExecuteChanged);
  check->setChecked(true);
  EXPECT_TRUE(spin->isEnabled());
  spin->setValue(60);
  editor->applyChangesToOperator();

  EXPECT_TRUE(transform->autoExecuteEnabled());
  EXPECT_EQ(transform->autoExecuteIntervalSeconds(), 60);
  EXPECT_GE(spy.count(), 1);
  delete editor;

  // Reopening seeds the controls from the node.
  auto* editor2 = transform->createPropertiesWidget(&pipeline, nullptr);
  auto* check2 = editor2->findChild<QCheckBox*>("autoExecuteCheck");
  auto* spin2 = editor2->findChild<QSpinBox*>("autoExecuteIntervalSpin");
  ASSERT_NE(check2, nullptr);
  ASSERT_NE(spin2, nullptr);
  EXPECT_TRUE(check2->isChecked());
  EXPECT_TRUE(spin2->isEnabled());
  EXPECT_EQ(spin2->value(), 60);

  // Applying with the box unticked turns the feature back off.
  check2->setChecked(false);
  editor2->applyChangesToOperator();
  EXPECT_FALSE(transform->autoExecuteEnabled());
  EXPECT_EQ(transform->autoExecuteIntervalSeconds(), 60);
  delete editor2;
}

TEST(AutoExecuteTest, EditorOmitsControlsForLegacyV1)
{
  tomviz_test::ensureQApp();

  Pipeline pipeline;
  auto* transform = new LegacyPythonTransform();
  transform->setJSONDescription(kV1Description);
  transform->setScript("def transform(dataset):\n    pass\n");
  pipeline.addNode(transform);

  auto* editor = transform->createPropertiesWidget(&pipeline, nullptr);
  EXPECT_EQ(editor->findChild<QCheckBox*>("autoExecuteCheck"), nullptr);
  EXPECT_EQ(editor->findChild<QSpinBox*>("autoExecuteIntervalSpin"),
            nullptr);

  // Applying must leave the node's (default) setting untouched:
  // autoExecuteEdited stays false without the controls.
  editor->applyChangesToOperator();
  EXPECT_FALSE(transform->autoExecuteEnabled());
  delete editor;
}

TEST(AutoExecuteTest, NodeSerializeRoundTrip)
{
  tomviz_test::ensureQApp();

  // Defaults leave no footprint in the serialized form.
  Node defaultNode;
  EXPECT_FALSE(defaultNode.serialize().contains("autoExecute"));

  Node node;
  node.setAutoExecuteEnabled(true);
  node.setAutoExecuteIntervalSeconds(45);
  QJsonObject json = node.serialize();
  ASSERT_TRUE(json.contains("autoExecute"));
  EXPECT_TRUE(json["autoExecute"].toObject()["enabled"].toBool());
  EXPECT_EQ(json["autoExecute"].toObject()["intervalSeconds"].toInt(), 45);

  Node restored;
  QSignalSpy spy(&restored, &Node::autoExecuteChanged);
  ASSERT_TRUE(restored.deserialize(json));
  EXPECT_TRUE(restored.autoExecuteEnabled());
  EXPECT_EQ(restored.autoExecuteIntervalSeconds(), 45);
  // Deserialize routes through the setters so the controller (which
  // may have seen the node before its state was applied) re-syncs.
  EXPECT_GE(spy.count(), 1);

  // A remembered interval round-trips even while disabled.
  Node disabledNode;
  disabledNode.setAutoExecuteIntervalSeconds(120);
  Node restored2;
  ASSERT_TRUE(restored2.deserialize(disabledNode.serialize()));
  EXPECT_FALSE(restored2.autoExecuteEnabled());
  EXPECT_EQ(restored2.autoExecuteIntervalSeconds(), 120);
}

TEST(AutoExecuteTest, UserStateIsRuntimeOnly)
{
  tomviz_test::ensureQApp();

  Node node;
  QVariantMap state;
  state["counter"] = 3;
  state["label"] = QStringLiteral("last-run");
  node.setUserState(state);
  EXPECT_EQ(node.userState().value("counter").toInt(), 3);

  // Not serialized — a save/load round trip starts with an empty bag.
  QJsonObject json = node.serialize();
  EXPECT_FALSE(json.contains("userState"));
  Node restored;
  ASSERT_TRUE(restored.deserialize(json));
  EXPECT_TRUE(restored.userState().isEmpty());
}
