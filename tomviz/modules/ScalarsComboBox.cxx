#include "ScalarsComboBox.h"

#include "DataSource.h"
#include "Module.h"

namespace tomviz {

ScalarsComboBox::ScalarsComboBox(QWidget* parent) : QComboBox(parent)
{
}

void ScalarsComboBox::setOptions(DataSource* ds, Module* module)
{
  this->blockSignals(true);

  clear();

  if (!ds || !module) {
    this->blockSignals(false);
    return;
  }

  addItem("Default", Module::DEFAULT_SCALARS_IDX);

  QStringList scalars = ds->listScalars();
  for (int i = 0; i < scalars.length(); ++i) {
    addItem(scalars[i], i);
  }

  int currentIndex;
  if (module->activeScalars() == Module::DEFAULT_SCALARS_IDX) {
    currentIndex = 0;
  } else {
    currentIndex = module->activeScalars() + 1;
  }

  setCurrentIndex(currentIndex);

  this->blockSignals(false);
}
}
