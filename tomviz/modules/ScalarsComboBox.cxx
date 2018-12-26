#include "ScalarsComboBox.h"

#include "DataSource.h"
#include "Module.h"

namespace tomviz {

ScalarsComboBox::ScalarsComboBox(QWidget* parent) : QComboBox(parent)
{
}

void ScalarsComboBox::setOptions(DataSource* ds, Module* module)
{
  if (module) {
    addItem(module->DEFAULT_SCALARS);
  }

  if (ds) {
    QStringList scalars = ds->listScalars();
    foreach (auto scalar, scalars) {
      addItem(scalar);
    }
  }

  if (module) {
    setCurrentText(module->activeScalars());
  }
}
}
