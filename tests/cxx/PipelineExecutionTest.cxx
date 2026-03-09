/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QFile>
#include <QIODevice>
#include <QSignalSpy>
#include <QTest>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include "DataSource.h"
#include "Pipeline.h"
#include "PipelineProxy.h"
#include "PythonUtilities.h"
#include "TomvizTest.h"
#include "operators/OperatorProxy.h"
#include "operators/OperatorPython.h"

using namespace tomviz;

static QString loadFixture(const QString& name)
{
  QFile file(QString("%1/fixtures/%2").arg(SOURCE_DIR, name));
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  return QString(file.readAll());
}

static vtkSmartPointer<vtkImageData> createImageData(int dim, double fill)
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(dim, dim, dim);
  image->AllocateScalars(VTK_DOUBLE, 1);
  for (int z = 0; z < dim; ++z) {
    for (int y = 0; y < dim; ++y) {
      for (int x = 0; x < dim; ++x) {
        image->SetScalarComponentFromDouble(x, y, z, 0, fill);
      }
    }
  }
  return image;
}

class PipelineExecutionTest : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    OperatorProxyFactory::registerWithFactory();
    PipelineProxyFactory::registerWithFactory();
  }

  void pipelineStopsAtBreakpoint()
  {
    auto image = createImageData(2, 0.0);
    auto* ds = new DataSource(image);

    QString script = loadFixture("increment_scalars.py");
    QVERIFY2(!script.isEmpty(), "Failed to load increment_scalars.py");

    // Create a pipeline (paused so operators aren't auto-executed on add)
    Pipeline pipeline(ds);
    pipeline.pause();

    // Add 3 increment operators
    auto* op0 = new OperatorPython(ds);
    op0->setLabel("increment_0");
    op0->setScript(script);
    ds->addOperator(op0);

    auto* op1 = new OperatorPython(ds);
    op1->setLabel("increment_1");
    op1->setScript(script);
    ds->addOperator(op1);

    auto* op2 = new OperatorPython(ds);
    op2->setLabel("increment_2");
    op2->setScript(script);
    ds->addOperator(op2);

    // Set a breakpoint on the 3rd operator
    op2->setBreakpoint(true);

    // Resume the pipeline and execute -- it should stop before op2
    pipeline.resume();
    QSignalSpy breakpointSpy(&pipeline, &Pipeline::breakpointReached);
    QSignalSpy finishedSpy(&pipeline, &Pipeline::finished);
    auto* future = pipeline.execute(ds, op0);

    // Wait for the pipeline to finish (up to 10 seconds)
    QVERIFY(finishedSpy.wait(10000));

    // breakpointReached should have been emitted with op2
    QCOMPARE(breakpointSpy.count(), 1);
    auto* reachedOp = breakpointSpy.takeFirst().at(0).value<Operator*>();
    QCOMPARE(reachedOp, op2);

    // The first 2 operators should be Complete, the 3rd should be Queued
    QCOMPARE(op0->state(), OperatorState::Complete);
    QCOMPARE(op1->state(), OperatorState::Complete);
    QCOMPARE(op2->state(), OperatorState::Queued);

    // Delete future before pipeline goes out of scope to avoid double-free
    // (PipelineFutureInternal's QScopedPointer vs executor's QObject parent)
    delete future;
  }

  void breakpointSkipsRemainingOps()
  {
    auto image = createImageData(2, 0.0);
    auto* ds = new DataSource(image);

    QString addOneScript = loadFixture("increment_scalars.py");
    QString addTenScript = loadFixture("add_ten.py");
    QVERIFY(!addOneScript.isEmpty());
    QVERIFY(!addTenScript.isEmpty());

    Pipeline pipeline(ds);
    pipeline.pause();

    // Op0: add 1, Op1: add 10, Op2 (breakpoint): would add 1 again
    auto* op0 = new OperatorPython(ds);
    op0->setLabel("add_one");
    op0->setScript(addOneScript);
    ds->addOperator(op0);

    auto* op1 = new OperatorPython(ds);
    op1->setLabel("add_ten");
    op1->setScript(addTenScript);
    ds->addOperator(op1);

    auto* op2 = new OperatorPython(ds);
    op2->setLabel("add_one_again");
    op2->setScript(addOneScript);
    op2->setBreakpoint(true);
    ds->addOperator(op2);

    pipeline.resume();
    QSignalSpy finishedSpy(&pipeline, &Pipeline::finished);
    auto* future = pipeline.execute(ds, op0);

    QVERIFY(finishedSpy.wait(10000));

    // Result should be 11.0 (0 + 1 + 10), not 12.0 (if 3rd ran too)
    auto result = future->result();
    QVERIFY(result != nullptr);
    int dims[3];
    result->GetDimensions(dims);
    for (int z = 0; z < dims[2]; ++z) {
      for (int y = 0; y < dims[1]; ++y) {
        for (int x = 0; x < dims[0]; ++x) {
          QCOMPARE(result->GetScalarComponentAsDouble(x, y, z, 0), 11.0);
        }
      }
    }

    delete future;
  }

  void executionOrderMatters()
  {
    QString addTenScript = loadFixture("add_ten.py");
    QString multiplyTwoScript = loadFixture("multiply_two.py");
    QVERIFY(!addTenScript.isEmpty());
    QVERIFY(!multiplyTwoScript.isEmpty());

    // Order 1: [add_ten, multiply_two] on data starting at 0
    // Expected: (0 + 10) * 2 = 20
    {
      auto image = createImageData(2, 0.0);
      auto* ds = new DataSource(image);
      Pipeline pipeline(ds);
      pipeline.pause();

      auto* opAdd = new OperatorPython(ds);
      opAdd->setLabel("add_ten");
      opAdd->setScript(addTenScript);
      ds->addOperator(opAdd);

      auto* opMul = new OperatorPython(ds);
      opMul->setLabel("multiply_two");
      opMul->setScript(multiplyTwoScript);
      ds->addOperator(opMul);

      pipeline.resume();
      QSignalSpy finishedSpy(&pipeline, &Pipeline::finished);
      auto* future = pipeline.execute(ds, opAdd);

      QVERIFY(finishedSpy.wait(10000));

      auto result = future->result();
      QVERIFY(result != nullptr);
      QCOMPARE(result->GetScalarComponentAsDouble(0, 0, 0, 0), 20.0);

      delete future;
    }

    // Order 2: [multiply_two, add_ten] on data starting at 0
    // Expected: (0 * 2) + 10 = 10
    {
      auto image = createImageData(2, 0.0);
      auto* ds = new DataSource(image);
      Pipeline pipeline(ds);
      pipeline.pause();

      auto* opMul = new OperatorPython(ds);
      opMul->setLabel("multiply_two");
      opMul->setScript(multiplyTwoScript);
      ds->addOperator(opMul);

      auto* opAdd = new OperatorPython(ds);
      opAdd->setLabel("add_ten");
      opAdd->setScript(addTenScript);
      ds->addOperator(opAdd);

      pipeline.resume();
      QSignalSpy finishedSpy(&pipeline, &Pipeline::finished);
      auto* future = pipeline.execute(ds, opMul);

      QVERIFY(finishedSpy.wait(10000));

      auto result = future->result();
      QVERIFY(result != nullptr);
      QCOMPARE(result->GetScalarComponentAsDouble(0, 0, 0, 0), 10.0);

      delete future;
    }
  }
};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);

  // Create a builtin server connection so proxies can be created
  auto* builder = pqApplicationCore::instance()->getObjectBuilder();
  builder->createServer(pqServerResource("builtin:"));

  Python::initialize();

  PipelineExecutionTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "PipelineExecutionTest.moc"
