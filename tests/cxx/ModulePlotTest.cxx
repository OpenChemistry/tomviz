/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QCheckBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QTest>
#include <QWidget>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>
#include <pqView.h>

#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkTable.h>

#include "modules/ModulePlot.h"
#include "operators/OperatorResult.h"

using namespace tomviz;

class ModulePlotTest : public QObject
{
  Q_OBJECT

private:
  ModulePlot* modulePlot;

  // Create a simple vtkTable with x and y columns
  vtkNew<vtkTable> createTestTable()
  {
    vtkNew<vtkTable> table;

    vtkNew<vtkDoubleArray> xCol;
    xCol->SetName("x");
    xCol->SetNumberOfTuples(5);
    for (int i = 0; i < 5; ++i) {
      xCol->SetValue(i, static_cast<double>(i));
    }

    vtkNew<vtkDoubleArray> yCol;
    yCol->SetName("y");
    yCol->SetNumberOfTuples(5);
    for (int i = 0; i < 5; ++i) {
      yCol->SetValue(i, static_cast<double>(i * i));
    }

    table->AddColumn(xCol);
    table->AddColumn(yCol);
    return table;
  }

private slots:
  void init()
  {
    modulePlot = new ModulePlot(this);
  }

  void cleanup()
  {
    delete modulePlot;
    modulePlot = nullptr;
  }

  void label()
  {
    QCOMPARE(modulePlot->label(), QString("Plot"));
  }

  void visibilityDefault()
  {
    QVERIFY(modulePlot->visibility());
  }

  void setVisibilityToggle()
  {
    QVERIFY(modulePlot->setVisibility(false));
    QVERIFY(!modulePlot->visibility());

    QVERIFY(modulePlot->setVisibility(true));
    QVERIFY(modulePlot->visibility());
  }

  void exportDataTypeString()
  {
    QCOMPARE(modulePlot->exportDataTypeString(), QString(""));
  }

  void dataToExportReturnsNull()
  {
    QVERIFY(modulePlot->dataToExport() == nullptr);
  }

  void initializeDataSourceReturnsFalse()
  {
    // Plot only works with OperatorResult, not DataSource
    QVERIFY(!modulePlot->initialize(static_cast<DataSource*>(nullptr),
                                    nullptr));
  }

  void finalize()
  {
    QVERIFY(modulePlot->finalize());
  }

  void finalizeTwice()
  {
    QVERIFY(modulePlot->finalize());
    QVERIFY(modulePlot->finalize());
  }

  void deserializeVisibility()
  {
    QJsonObject props;
    props["visibility"] = false;
    QJsonObject json;
    json["properties"] = props;

    QVERIFY(modulePlot->deserialize(json));
    QVERIFY(!modulePlot->visibility());
  }

  void deserializeRestoresVisibilityTrue()
  {
    modulePlot->setVisibility(false);
    QVERIFY(!modulePlot->visibility());

    QJsonObject props;
    props["visibility"] = true;
    QJsonObject json;
    json["properties"] = props;

    QVERIFY(modulePlot->deserialize(json));
    QVERIFY(modulePlot->visibility());
  }

  void dataSourceMovedDoesNotCrash()
  {
    modulePlot->dataSourceMoved(1.0, 2.0, 3.0);
  }

  void dataSourceRotatedDoesNotCrash()
  {
    modulePlot->dataSourceRotated(45.0, 90.0, 0.0);
  }

  void initializeWithOperatorResult()
  {
    auto* objBuilder = pqApplicationCore::instance()->getObjectBuilder();
    auto* server = pqApplicationCore::instance()->getActiveServer();
    QVERIFY(server != nullptr);

    auto* pqView = objBuilder->createView("XYChartView", server);
    QVERIFY(pqView != nullptr);
    auto* viewProxy = pqView->getViewProxy();
    QVERIFY(viewProxy != nullptr);

    auto table = createTestTable();
    auto* result = new OperatorResult(this);
    result->setName("test_result");
    result->setDataObject(table);

    QVERIFY(modulePlot->initialize(result, viewProxy));
    QVERIFY(modulePlot->visibility());

    QVERIFY(modulePlot->finalize());
    objBuilder->destroy(pqView);
  }

  void addToPanel()
  {
    auto* objBuilder = pqApplicationCore::instance()->getObjectBuilder();
    auto* server = pqApplicationCore::instance()->getActiveServer();
    auto* pqView = objBuilder->createView("XYChartView", server);
    auto* viewProxy = pqView->getViewProxy();

    auto table = createTestTable();
    auto* result = new OperatorResult(this);
    result->setName("test_result");
    result->setDataObject(table);

    QVERIFY(modulePlot->initialize(result, viewProxy));

    // addToPanel should create the label/log-scale controls
    QWidget panel;
    modulePlot->addToPanel(&panel);

    auto* layout = qobject_cast<QFormLayout*>(panel.layout());
    QVERIFY(layout != nullptr);

    auto* xLogCheckBox = panel.findChild<QCheckBox*>();
    QVERIFY(xLogCheckBox != nullptr);

    auto* xLabelEdit = panel.findChild<QLineEdit*>();
    QVERIFY(xLabelEdit != nullptr);

    QVERIFY(modulePlot->finalize());
    objBuilder->destroy(pqView);
  }

  void serializeAfterInit()
  {
    auto* objBuilder = pqApplicationCore::instance()->getObjectBuilder();
    auto* server = pqApplicationCore::instance()->getActiveServer();
    auto* pqView = objBuilder->createView("XYChartView", server);
    auto* viewProxy = pqView->getViewProxy();

    auto table = createTestTable();
    auto* result = new OperatorResult(this);
    result->setName("test_result");
    result->setDataObject(table);

    QVERIFY(modulePlot->initialize(result, viewProxy));

    auto json = modulePlot->serialize();

    // Should contain properties with visibility
    QVERIFY(json.contains("properties"));
    auto props = json["properties"].toObject();
    QCOMPARE(props["visibility"].toBool(), true);

    // Should contain the operator result name
    QCOMPARE(json["operatorResultName"].toString(), QString("test_result"));

    QVERIFY(modulePlot->finalize());
    objBuilder->destroy(pqView);
  }

  void visibilityToggleWithChart()
  {
    auto* objBuilder = pqApplicationCore::instance()->getObjectBuilder();
    auto* server = pqApplicationCore::instance()->getActiveServer();
    auto* pqView = objBuilder->createView("XYChartView", server);
    auto* viewProxy = pqView->getViewProxy();

    auto table = createTestTable();
    auto* result = new OperatorResult(this);
    result->setName("test_result");
    result->setDataObject(table);

    QVERIFY(modulePlot->initialize(result, viewProxy));

    // Toggle visibility off and on with an actual chart backing it
    QVERIFY(modulePlot->setVisibility(false));
    QVERIFY(!modulePlot->visibility());

    QVERIFY(modulePlot->setVisibility(true));
    QVERIFY(modulePlot->visibility());

    QVERIFY(modulePlot->finalize());
    objBuilder->destroy(pqView);
  }
};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);

  // Create a builtin server connection so views and proxies can be created
  auto* builder = pqApplicationCore::instance()->getObjectBuilder();
  builder->createServer(pqServerResource("builtin:"));

  ModulePlotTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "ModulePlotTest.moc"
