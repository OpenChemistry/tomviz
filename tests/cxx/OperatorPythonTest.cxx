/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include <thread>

#include <vtkDataObject.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QIODevice>
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
