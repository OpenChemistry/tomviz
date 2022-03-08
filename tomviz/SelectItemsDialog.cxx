/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SelectItemsDialog.h"

#include <QCheckBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

namespace tomviz {

SelectItemsDialog::SelectItemsDialog(QStringList items, QWidget* parent)
  : QDialog(parent), m_items(items)
{
  auto* table = new QTableWidget(this);

  auto setTableCell = [table](QWidget* w, int i, int j) {
    auto* tw = new QWidget(table);
    auto* layout = new QHBoxLayout(tw);
    layout->addWidget(w);
    layout->setContentsMargins(10, 0, 0, 0);
    table->setCellWidget(i, j, tw);
  };

  // Set up the table and add the checkboxes
  table->horizontalHeader()->hide();
  table->horizontalHeader()->setStretchLastSection(true);
  table->setColumnCount(1);
  table->setRowCount(items.size());
  for (auto i = 0; i < items.size(); ++i) {
    auto* cb = new QCheckBox(items[i], table);
    m_checkboxes.append(cb);
    setTableCell(cb, i, 0);
  }

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(table);

  // Setup Ok and Cancel buttons
  auto buttonOptions = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
  auto* buttons = new QDialogButtonBox(buttonOptions, this);
  buttons->setCenterButtons(true);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // No close button in the corner
  setWindowFlags((windowFlags() | Qt::CustomizeWindowHint) &
                 ~Qt::WindowCloseButtonHint);
}

QStringList SelectItemsDialog::selectedItems() const
{
  QStringList selected;
  for (int i = 0; i < m_checkboxes.size(); ++i) {
    if (m_checkboxes[i]->isChecked()) {
      selected.append(m_items[i]);
    }
  }
  return selected;
}

QList<bool> SelectItemsDialog::selections() const
{
  QList<bool> ret;
  for (auto* cb : m_checkboxes) {
    ret.append(cb->isChecked());
  }
  return ret;
}

void SelectItemsDialog::setSelections(QList<bool> selections)
{
  if (selections.size() != m_items.size()) {
    auto msg =
      QString("Size of selections \"%1\" does not match size of items \"%2\"")
        .arg(selections.size())
        .arg(m_items.size());
    qCritical() << msg;
    return;
  }

  for (int i = 0; i < selections.size(); ++i) {
    m_checkboxes[i]->setChecked(selections[i]);
  }
}

} // namespace tomviz
