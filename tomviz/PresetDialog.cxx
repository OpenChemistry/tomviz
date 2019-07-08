/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetDialog.h"
#include "PresetModel.h"
#include "ui_PresetDialog.h"

#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QTableView>
#include <QVBoxLayout>

#include <vtkSMProxy.h>

namespace tomviz {

PresetDialog::PresetDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::PresetDialog)
{
  m_ui->setupUi(this);

  m_view = new QTableView(this);
  m_model = new PresetModel();
  auto* layout = new QVBoxLayout;

  m_view->setModel(m_model);
  m_view->horizontalHeader()->hide();
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  layout->addWidget(m_view);
  layout->addWidget(m_ui->buttonBox);
  setLayout(layout);

  m_view->resizeColumnsToContents();
  m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  connect(m_view, &QTableView::doubleClicked, m_model,
          &PresetModel::changePreset);
  connect(m_view, &QTableView::clicked, m_model, &PresetModel::setRow);
  connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this,
          &PresetDialog::applyPreset);
  connect(m_model, &PresetModel::applyPreset, this, &PresetDialog::applyPreset);
  connect(m_view, &QMenu::customContextMenuRequested,
          [&](QPoint pos) { this->customMenuRequested(m_view->indexAt(pos)); });
}

PresetDialog::~PresetDialog() = default;
  
QString PresetDialog::presetName()
{
  return m_model->presetName();
}

void PresetDialog::addNewPreset(const QJsonObject& newPreset)
{
  m_model->addNewPreset(newPreset);
}

QJsonObject PresetDialog::jsonObject()
{
  return m_model->jsonObject();
}

void PresetDialog::customMenuRequested(const QModelIndex& index)
{
  QAction removePreset("Delete Preset", this);
  connect(&removePreset, &QAction::triggered,
          [&]() { m_model->deletePreset(index); });

  QMenu menu(this);
  menu.addAction(&removePreset);
  menu.exec(QCursor::pos());
}
} // namespace tomviz
