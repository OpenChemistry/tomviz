/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "DoubleSpinBox.h"
#include "InterfaceBuilder.h"
#include "SpinBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>

namespace tomviz {

OperatorWidget::OperatorWidget(QWidget* parentObject) : Superclass(parentObject)
{}

OperatorWidget::~OperatorWidget() {}

void OperatorWidget::setupUI(OperatorPython* op)
{
  this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
  QString json = op->JSONDescription();
  if (!json.isNull()) {
    DataSource* dataSource = nullptr;
    if (op->hasChildDataSource())
      dataSource = op->childDataSource();
    else
      dataSource = qobject_cast<DataSource*>(op->parent());

    if (!dataSource)
      dataSource = ActiveObjects::instance().activeDataSource();

    InterfaceBuilder* ib = new InterfaceBuilder(this, dataSource);
    ib->setJSONDescription(json);
    ib->setParameterValues(op->arguments());
    buildInterface(ib);
  }
}

void OperatorWidget::setupUI(const QString& json)
{
  InterfaceBuilder* ib =
    new InterfaceBuilder(this, ActiveObjects::instance().activeDataSource());
  ib->setJSONDescription(json);
  buildInterface(ib);
}

void OperatorWidget::buildInterface(InterfaceBuilder* builder)
{
  QLayout* layout = builder->buildInterface();
  this->setLayout(layout);
}

QMap<QString, QVariant> OperatorWidget::values() const
{
  return InterfaceBuilder::parameterValues(this);
}
} // namespace tomviz
