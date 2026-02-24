/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include <memory>
#include <thread>

#include <vtkDataObject.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QString>

#include "PipelineProxy.h"
#include "TomvizTest.h"
#include "operators/OperatorProxy.h"
#include "operators/OperatorPython.h"

using namespace tomviz;

class OperatorPythonTest : public ::testing::Test
{
public:
  // This should be called once globally - it is not in my testing :-(
  static void SetUpTestSuite()
  {
    // Register our factories for Python wrapping.
    OperatorProxyFactory::registerWithFactory();
    PipelineProxyFactory::registerWithFactory();
  }

protected:
  void SetUp() override
  {
    // Register our factories for Python wrapping. Should be done globally
    // above, but for some reason isn't running for me. Putting it here as a
    // hack - FIXME: remove this when the single invocation works.
    OperatorProxyFactory::registerWithFactory();
    PipelineProxyFactory::registerWithFactory();
    dataObject = vtkDataObject::New();
    pythonOperator = new OperatorPython(nullptr);
  }

  void TearDown() override
  {
    dataObject->Delete();
    pythonOperator->deleteLater();
  }

  vtkDataObject* dataObject;
  OperatorPython* pythonOperator;
};

TEST_F(OperatorPythonTest, transform_function)
{
  pythonOperator->setLabel("transform_function");
  QFile file(QString("%1/fixtures/function.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    pythonOperator->setScript(script);
    pythonOperator->transform(dataObject);
    file.close();
  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, operator_transform)
{
  pythonOperator->setLabel("operator_transform");
  QFile file(QString("%1/fixtures/test_operator.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    pythonOperator->setScript(script);
    ASSERT_EQ(pythonOperator->transform(dataObject), TransformResult::Complete);
    file.close();
  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, cancelable_operator_transform)
{
  pythonOperator->setLabel("cancelable_operator_transform");
  QFile file(QString("%1/fixtures/cancelable.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    file.close();
    pythonOperator->setScript(script);
    ASSERT_EQ(pythonOperator->transform(dataObject), TransformResult::Complete);

    // Mimic user canceling operator
    std::thread canceler([this]() {
      while (!pythonOperator->isCanceled()) {
        // Wait until we are running to cancel
        if (pythonOperator->state() == OperatorState::Running) {
          pythonOperator->cancelTransform();
        }
      }
    });
    TransformResult result = pythonOperator->transform(dataObject);
    canceler.join();
    ASSERT_EQ(result, TransformResult::Canceled);

  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, set_max_progress)
{
  pythonOperator->setLabel("set_max_progress");
  QFile file(QString("%1/fixtures/set_max_progress.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    file.close();
    pythonOperator->setScript(script);

    TransformResult result = pythonOperator->transform(dataObject);
    ASSERT_EQ(result, TransformResult::Complete);
    ASSERT_EQ(pythonOperator->totalProgressSteps(), 10);

  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, update_progress)
{
  pythonOperator->setLabel("update_progress");
  QFile file(QString("%1/fixtures/update_progress.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    file.close();
    pythonOperator->setScript(script);

    QSignalSpy spy(pythonOperator, SIGNAL(progressStepChanged(int)));
    TransformResult result = pythonOperator->transform(dataObject);
    ASSERT_EQ(result, TransformResult::Complete);

    // One from applyTransform() and one from our python code
    ASSERT_EQ(spy.count(), 2);

    // Take the signal emitted from our python code
    QList<QVariant> args = spy.takeAt(1);
    ASSERT_EQ(args.at(0).toInt(), 100);

  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, update_progress_message)
{
  pythonOperator->setLabel("update_progress_message");
  QFile file(QString("%1/fixtures/update_progress_message.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    file.close();
    pythonOperator->setScript(script);

    QSignalSpy spy(pythonOperator,
                   SIGNAL(progressMessageChanged(const QString&)));
    TransformResult result = pythonOperator->transform(dataObject);
    ASSERT_EQ(result, TransformResult::Complete);

    ASSERT_EQ(spy.count(), 1);

    QList<QVariant> args = spy.takeAt(0);
    ASSERT_STREQ(args.at(0).toString().toLatin1().constData(),
                 "Is there anyone out there?");

  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, update_data)
{
  pythonOperator->setLabel("update_data");
  // Disconnect these signals as they will cause us to reach into
  // part of ParaView that aren't available in the test executable.
  // We only need to test that the childDataSourceUpdated signal is
  // fired, not the data source creation.
  QObject::disconnect(pythonOperator,
                      static_cast<void (Operator::*)(DataSource*)>(
                        &OperatorPython::newChildDataSource),
                      nullptr, nullptr);
  QObject::disconnect(pythonOperator, &OperatorPython::childDataSourceUpdated,
                      nullptr, nullptr);
  QFile file(QString("%1/fixtures/update_data.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    file.close();
    pythonOperator->setScript(script);

    QSignalSpy spy(pythonOperator, SIGNAL(childDataSourceUpdated(
                                     vtkSmartPointer<vtkDataObject>)));
    TransformResult result = pythonOperator->transform(dataObject);
    ASSERT_EQ(result, TransformResult::Complete);

    ASSERT_EQ(spy.count(), 1);

    QList<QVariant> args = spy.takeFirst();
    vtkSmartPointer<vtkDataObject> data =
      args.at(0).value<vtkSmartPointer<vtkDataObject>>();
    vtkImageData* imageData = vtkImageData::SafeDownCast(data);
    int dims[3];
    imageData->GetDimensions(dims);
    ASSERT_EQ(dims[0], 3);
    ASSERT_EQ(dims[1], 4);
    ASSERT_EQ(dims[2], 5);

    for (int z = 0; z < dims[2]; z++) {
      for (int y = 0; y < dims[1]; y++) {
        for (int x = 0; x < dims[0]; x++) {
          ASSERT_EQ(imageData->GetScalarComponentAsDouble(x, y, z, 0), 2.0);
        }
      }
    }
  } else {
    FAIL() << "Unable to load script.";
  }
}

// --- Breakpoint API tests ---

TEST_F(OperatorPythonTest, breakpoint_default_false)
{
  ASSERT_FALSE(pythonOperator->hasBreakpoint());
}

TEST_F(OperatorPythonTest, breakpoint_set_emits_signal)
{
  QSignalSpy spy(pythonOperator, SIGNAL(breakpointChanged()));
  pythonOperator->setBreakpoint(true);
  ASSERT_TRUE(pythonOperator->hasBreakpoint());
  ASSERT_EQ(spy.count(), 1);
}

TEST_F(OperatorPythonTest, breakpoint_no_signal_on_same_value)
{
  pythonOperator->setBreakpoint(true);
  QSignalSpy spy(pythonOperator, SIGNAL(breakpointChanged()));
  pythonOperator->setBreakpoint(true);
  ASSERT_EQ(spy.count(), 0);
}

TEST_F(OperatorPythonTest, breakpoint_toggle)
{
  QSignalSpy spy(pythonOperator, SIGNAL(breakpointChanged()));
  pythonOperator->setBreakpoint(true);
  ASSERT_TRUE(pythonOperator->hasBreakpoint());
  pythonOperator->setBreakpoint(false);
  ASSERT_FALSE(pythonOperator->hasBreakpoint());
  ASSERT_EQ(spy.count(), 2);
}

// --- Serialization tests ---

TEST_F(OperatorPythonTest, serialize_breakpoint)
{
  pythonOperator->setLabel("test");
  pythonOperator->setBreakpoint(true);
  auto json = pythonOperator->serialize();
  ASSERT_TRUE(json.contains("breakpoint"));
  ASSERT_TRUE(json["breakpoint"].toBool());
}

TEST_F(OperatorPythonTest, serialize_no_breakpoint_by_default)
{
  pythonOperator->setLabel("test");
  auto json = pythonOperator->serialize();
  ASSERT_FALSE(json.contains("breakpoint"));
}

TEST_F(OperatorPythonTest, deserialize_restores_label_and_script)
{
  QJsonObject json;
  json["label"] = "My Label";
  json["script"] = "def transform(dataset): pass";
  json["description"] = "";
  pythonOperator->deserialize(json);
  ASSERT_STREQ(pythonOperator->label().toLatin1().constData(), "My Label");
  ASSERT_STREQ(pythonOperator->script().toLatin1().constData(),
               "def transform(dataset): pass");
}

// --- JSON description parsing tests ---

TEST_F(OperatorPythonTest, json_description_parameters)
{
  QString desc = R"({
    "name": "TestOp",
    "label": "Test Operator",
    "parameters": [
      {"name": "param1", "type": "int", "default": 0},
      {"name": "param2", "type": "double", "default": 1.0},
      {"name": "param3", "type": "bool", "default": true}
    ]
  })";
  pythonOperator->setJSONDescription(desc);
  ASSERT_EQ(pythonOperator->numberOfParameters(), 3);
  ASSERT_STREQ(pythonOperator->label().toLatin1().constData(),
               "Test Operator");
}

TEST_F(OperatorPythonTest, json_description_no_parameters)
{
  QString desc = R"({
    "name": "SimpleOp",
    "label": "Simple Operator"
  })";
  pythonOperator->setJSONDescription(desc);
  ASSERT_EQ(pythonOperator->numberOfParameters(), 0);
  ASSERT_STREQ(pythonOperator->label().toLatin1().constData(),
               "Simple Operator");
}

TEST_F(OperatorPythonTest, json_description_with_results)
{
  QString desc = R"({
    "name": "ResultOp",
    "label": "Result Operator",
    "parameters": [
      {"name": "threshold", "type": "double", "default": 0.5}
    ],
    "results": [
      {"name": "output_image", "label": "Output Image"}
    ]
  })";
  pythonOperator->setJSONDescription(desc);
  ASSERT_EQ(pythonOperator->numberOfParameters(), 1);
  ASSERT_EQ(pythonOperator->numberOfResults(), 1);
}

// --- Argument serialization round-trip tests ---

TEST_F(OperatorPythonTest, serialize_deserialize_double_argument)
{
  QString desc = R"({
    "name": "TestOp",
    "label": "Test Operator",
    "parameters": [
      {"name": "rotation_center", "type": "double", "default": 0.0}
    ]
  })";
  pythonOperator->setJSONDescription(desc);

  QMap<QString, QVariant> args;
  args["rotation_center"] = 42.5;
  pythonOperator->setArguments(args);
  pythonOperator->setScript("def transform(dataset): pass");

  auto json = pythonOperator->serialize();

  // Deserialize onto a fresh operator
  auto* newOp = new OperatorPython(nullptr);
  ASSERT_TRUE(newOp->deserialize(json));

  auto newArgs = newOp->arguments();
  ASSERT_DOUBLE_EQ(newArgs["rotation_center"].toDouble(), 42.5);

  newOp->deleteLater();
}

TEST_F(OperatorPythonTest, serialize_deserialize_enumeration_argument)
{
  QString desc = R"({
    "name": "TestOp",
    "label": "Test Operator",
    "parameters": [
      {"name": "transform_source", "type": "enumeration", "default": 0,
       "options": [{"Manual": "manual"}, {"Load From File": "from_file"}]}
    ]
  })";
  pythonOperator->setJSONDescription(desc);

  QMap<QString, QVariant> args;
  args["transform_source"] = 1;
  pythonOperator->setArguments(args);
  pythonOperator->setScript("def transform(dataset): pass");

  auto json = pythonOperator->serialize();

  auto* newOp = new OperatorPython(nullptr);
  ASSERT_TRUE(newOp->deserialize(json));

  auto newArgs = newOp->arguments();
  ASSERT_EQ(newArgs["transform_source"].toInt(), 1);

  newOp->deleteLater();
}

TEST_F(OperatorPythonTest, deserialize_select_scalars)
{
  QString desc = R"({
    "name": "TestOp",
    "label": "Test Operator",
    "parameters": [
      {"name": "selected_scalars", "type": "select_scalars"}
    ]
  })";

  QJsonObject json;
  json["description"] = desc;
  json["label"] = "Test Operator";
  json["script"] = "";

  QJsonObject args;
  QJsonArray scalarsArray;
  scalarsArray.append("scalar_a");
  scalarsArray.append("scalar_b");
  args["selected_scalars"] = scalarsArray;
  json["arguments"] = args;

  pythonOperator->deserialize(json);

  auto resultArgs = pythonOperator->arguments();
  auto scalars = resultArgs["selected_scalars"].toList();
  ASSERT_EQ(scalars.size(), 2);
  ASSERT_STREQ(scalars[0].toString().toLatin1().constData(), "scalar_a");
  ASSERT_STREQ(scalars[1].toString().toLatin1().constData(), "scalar_b");
}

TEST_F(OperatorPythonTest, serialize_deserialize_roundtrip)
{
  QString desc = R"({
    "name": "TestOp",
    "label": "Test Operator",
    "parameters": [
      {"name": "value", "type": "double", "default": 0.0}
    ]
  })";
  pythonOperator->setJSONDescription(desc);
  pythonOperator->setScript("def transform(dataset): pass");

  QMap<QString, QVariant> args;
  args["value"] = 3.14;
  pythonOperator->setArguments(args);

  auto json = pythonOperator->serialize();

  // Verify the serialized JSON contains expected fields
  ASSERT_TRUE(json.contains("description"));
  ASSERT_TRUE(json.contains("label"));
  ASSERT_TRUE(json.contains("script"));
  ASSERT_TRUE(json.contains("arguments"));
  ASSERT_STREQ(json["type"].toString().toLatin1().constData(), "Python");

  auto* newOp = new OperatorPython(nullptr);
  ASSERT_TRUE(newOp->deserialize(json));

  ASSERT_DOUBLE_EQ(newOp->arguments()["value"].toDouble(), 3.14);
  ASSERT_STREQ(newOp->label().toLatin1().constData(), "Test Operator");
  ASSERT_STREQ(newOp->script().toLatin1().constData(),
               "def transform(dataset): pass");

  newOp->deleteLater();
}

