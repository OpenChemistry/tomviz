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

  auto *view = new QTableView(this);
  m_model = new PresetModel();
  auto *layout = new QVBoxLayout;

  view->setModel(m_model);
  layout->addWidget(view);
  layout->addWidget(m_ui->buttonBox);
  setLayout(layout);

  view->resizeColumnsToContents();
  view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  connect(view, &QTableView::doubleClicked, m_model, &PresetModel::changePreset);
  connect(view, &QTableView::clicked, m_model, &PresetModel::setName);
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &PresetDialog::applyPreset);
  connect(m_model, &PresetModel::applyPreset, this, &PresetDialog::applyPreset);

}
  
QString PresetDialog::getName() {
  return m_model->getName();
}

PresetDialog::~PresetDialog() = default;
} // namespace tomviz
