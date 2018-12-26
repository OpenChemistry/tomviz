#include "ScalarsComboBox.h"

#include "DataSource.h"
#include "Module.h"

namespace tomviz {

ScalarsComboBox::ScalarsComboBox(QWidget* parent) : QComboBox(parent)
{
}

void ScalarsComboBox::setOptions(DataSource* ds, Module* module)
{
  if (!ds || !module) {
    return;
  }

  addItem("Default", module->DEFAULT_SCALARS);

  QStringList scalars = ds->listScalars();
  for (int i = 0; i < scalars.length(); ++i) {
    addItem(scalars[i], i);
  }

  setCurrentIndex(module->activeScalars() + 1);
}
}
