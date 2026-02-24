/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMap>
#include <QStandardItemModel>
#include <QTest>
#include <QVariant>
#include <QWidget>

#include "InterfaceBuilder.h"

using namespace tomviz;

class InterfaceBuilderTest : public QObject
{
  Q_OBJECT

private:
  InterfaceBuilder* builder;
  QWidget* parentWidget;

private slots:
  void init()
  {
    builder = new InterfaceBuilder(this);
    parentWidget = new QWidget();
  }

  void cleanup()
  {
    delete parentWidget;
    parentWidget = nullptr;
  }

  void selectScalarsWidgetCreated()
  {
    QString desc = R"({
      "name": "TestOp",
      "label": "Test Operator",
      "parameters": [
        {"name": "selected_scalars", "type": "select_scalars",
         "label": "Scalars"}
      ]
    })";

    builder->setJSONDescription(desc);
    QLayout* layout = builder->buildInterface();
    QVERIFY(layout != nullptr);

    parentWidget->setLayout(layout);

    auto* widget = parentWidget->findChild<QWidget*>("selected_scalars");
    QVERIFY(widget != nullptr);
    QCOMPARE(widget->property("type").toString(), QString("select_scalars"));
  }

  void selectScalarsParameterValues()
  {
    QString desc = R"({
      "name": "TestOp",
      "label": "Test Operator",
      "parameters": [
        {"name": "selected_scalars", "type": "select_scalars",
         "label": "Scalars"}
      ]
    })";

    builder->setJSONDescription(desc);
    QLayout* layout = builder->buildInterface();
    QVERIFY(layout != nullptr);
    parentWidget->setLayout(layout);

    // The combo box is empty because we have no DataSource.
    // Manually populate the model and check items to test parameterValues().
    auto* container =
      parentWidget->findChild<QWidget*>("selected_scalars");
    QVERIFY(container != nullptr);

    auto* applyAllCB =
      container->findChild<QCheckBox*>("selected_scalars_apply_all");
    QVERIFY(applyAllCB != nullptr);

    auto* combo =
      container->findChild<QComboBox*>("selected_scalars_combo");
    QVERIFY(combo != nullptr);

    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    QVERIFY(model != nullptr);

    // Add items and check them
    auto* itemA = new QStandardItem("scalar_a");
    itemA->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    itemA->setData(Qt::Checked, Qt::CheckStateRole);
    model->appendRow(itemA);

    auto* itemB = new QStandardItem("scalar_b");
    itemB->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    itemB->setData(Qt::Checked, Qt::CheckStateRole);
    model->appendRow(itemB);

    // Uncheck "Apply to all" so individual selections are used
    applyAllCB->setChecked(false);

    auto result = InterfaceBuilder::parameterValues(parentWidget);
    QVERIFY(result.contains("selected_scalars"));

    auto resultScalars = result["selected_scalars"].toList();
    QCOMPARE(resultScalars.size(), 2);
    QCOMPARE(resultScalars[0].toString(), QString("scalar_a"));
    QCOMPARE(resultScalars[1].toString(), QString("scalar_b"));
  }

  void basicParameterTypes()
  {
    QString desc = R"({
      "name": "TestOp",
      "label": "Test Operator",
      "parameters": [
        {"name": "int_param", "type": "int", "label": "Int", "default": 5},
        {"name": "double_param", "type": "double", "label": "Double",
         "default": 1.5},
        {"name": "bool_param", "type": "bool", "label": "Bool", "default": true}
      ]
    })";

    builder->setJSONDescription(desc);
    QLayout* layout = builder->buildInterface();
    QVERIFY(layout != nullptr);

    parentWidget->setLayout(layout);

    auto result = InterfaceBuilder::parameterValues(parentWidget);
    QVERIFY(result.contains("int_param"));
    QVERIFY(result.contains("double_param"));
    QVERIFY(result.contains("bool_param"));

    QCOMPARE(result["int_param"].toInt(), 5);
    QCOMPARE(result["double_param"].toDouble(), 1.5);
    QCOMPARE(result["bool_param"].toBool(), true);
  }

  void enableIfVisibleIf()
  {
    QString desc = R"({
      "name": "TestOp",
      "label": "Test Operator",
      "parameters": [
        {"name": "toggle", "type": "bool", "label": "Toggle", "default": false},
        {"name": "enabled_dep", "type": "int", "label": "Enabled Dep",
         "default": 0, "enable_if": "toggle == true"},
        {"name": "visible_dep", "type": "int", "label": "Visible Dep",
         "default": 0, "visible_if": "toggle == true"}
      ]
    })";

    builder->setJSONDescription(desc);
    QLayout* layout = builder->buildInterface();
    QVERIFY(layout != nullptr);

    parentWidget->setLayout(layout);
    // Must show the parent so isVisible() works on children --
    // Qt's isVisible() requires all ancestors to be visible.
    parentWidget->show();

    auto* toggleCheckBox =
      parentWidget->findChild<QCheckBox*>("toggle");
    QVERIFY(toggleCheckBox != nullptr);

    auto* enabledWidget = parentWidget->findChild<QWidget*>("enabled_dep");
    QVERIFY(enabledWidget != nullptr);

    auto* visibleWidget = parentWidget->findChild<QWidget*>("visible_dep");
    QVERIFY(visibleWidget != nullptr);

    // Toggle is false by default -- enabled_dep should be disabled,
    // visible_dep should be hidden
    QVERIFY(!enabledWidget->isEnabled());
    QVERIFY(!visibleWidget->isVisible());

    // Toggle on -- both should become active
    toggleCheckBox->setChecked(true);
    QVERIFY(enabledWidget->isEnabled());
    QVERIFY(visibleWidget->isVisible());

    // Toggle back off -- both should revert
    toggleCheckBox->setChecked(false);
    QVERIFY(!enabledWidget->isEnabled());
    QVERIFY(!visibleWidget->isVisible());
  }
};

QTEST_MAIN(InterfaceBuilderTest)
#include "InterfaceBuilderTest.moc"
