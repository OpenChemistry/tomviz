/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CloneDataReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "Utilities.h"

#include <QInputDialog>

namespace tomviz {

CloneDataReaction::CloneDataReaction(QAction* parentObject)
  : Reaction(parentObject)
{
}

DataSource* CloneDataReaction::clone(DataSource* toClone)
{
  toClone = toClone ? toClone : ActiveObjects::instance().activeDataSource();
  if (!toClone) {
    return nullptr;
  }

  QStringList items;
  items << "Data only"
        << "Data with transformations";

  bool userOkayed;
  QString selection =
    QInputDialog::getItem(tomviz::mainWidget(), "Clone Data Options",
                          "Select what should be cloned", items,
                          /*current=*/0,
                          /*editable=*/false,
                          /*ok*/ &userOkayed);

  if (userOkayed) {
    DataSource* newClone = toClone->clone();
    LoadDataReaction::dataSourceAdded(newClone);
    auto cloneOperators = selection == items[1];
    // We clone the operators after adding the data source as at this point the
    // appropriate signals are connected on the data source.
    if (cloneOperators) {
      // now, clone the operators.
      foreach (Operator* op, toClone->operators()) {
        Operator* opClone(op->clone());
        newClone->addOperator(opClone);
      }
    }
    return newClone;
  }
  return nullptr;
}
} // namespace tomviz
