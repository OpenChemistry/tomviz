#include "ScalarsComboBox.h"

#include "DataSource.h"
#include "Module.h"

namespace tomviz {

ScalarsComboBox::ScalarsComboBox(QWidget* parent) : QComboBox(parent)
{
}

void ScalarsComboBox::setOptions(DataSource* ds, Module* module)
{
  const QSignalBlocker blocker(this);

  clear();

  if (!ds || !module) {
    return;
  }

  addItem("Default", Module::defaultScalarsIdx());

  QStringList scalars = ds->listScalars();
  for (int i = 0; i < scalars.length(); ++i) {
    addItem(scalars[i], i);
  }

  int currentIndex;
  if (module->activeScalars() == Module::defaultScalarsIdx()) {
    currentIndex = 0;
  } else {
    currentIndex = module->activeScalars() + 1;
  }

  setCurrentIndex(currentIndex);
}
}
