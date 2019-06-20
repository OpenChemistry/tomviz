/* This source file is part of the Tomviz project, https://tomviz.org/. 
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetDialog.h"
#include "PresetModel.h"
#include "ui_PresetDialog.h"

#include <QTableView>
#include <QVBoxLayout>

namespace tomviz {

PresetDialog::PresetDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::PresetDialog)
{
  m_ui->setupUi(this);

  /*
    Create instance of PresetModel and pass a pointer to it to the view.
    The view will then invoke the methods of the model pointer to 
    determine the number of rows and columns that should be displayed.
  */
  auto *view = new QTableView(this);
  auto *model = new PresetModel();
  view->setModel(model);
  // make columns fit what is in them
  view->resizeColumnsToContents();
  // make columns stetch to fit table size
  view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  // create a grid to center the column
  auto *layout = new QVBoxLayout;
  layout->addWidget(view);
  layout->addWidget(m_ui->buttonBox);
  setLayout(layout);
}

PresetDialog::~PresetDialog() = default;
} // namespace tomviz
