/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PresetDialog.h"
#include "PresetModel.h"
#include "ui_PresetDialog.h"

#include <QColorDialog>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QTableView>
#include <QVBoxLayout>

namespace tomviz {

PresetDialog::PresetDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::PresetDialog)
{
  m_ui->setupUi(this);

  m_view = m_ui->tableView;
  m_model = new PresetModel();

  m_view->setModel(m_model);

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
  connect(m_ui->resetToDefaultsButton, &QPushButton::clicked, this,
          &PresetDialog::warning);
  connect(m_ui->createSolidColormap, &QPushButton::clicked, this,
          &PresetDialog::createSolidColormap);
  connect(this, &PresetDialog::resetToDefaults, m_model,
	        &PresetModel::resetToDefaults);
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

void PresetDialog::customMenuRequested(const QModelIndex& index)
{
  QAction editPreset("Edit Preset Name", this);
  QAction removePreset("Delete Preset", this);

  connect(&editPreset, &QAction::triggered,
          [&]() { m_view->edit(index); });
  connect(&removePreset, &QAction::triggered,
          [&]() { m_model->deletePreset(index); });

  QMenu menu(this);
  menu.addAction(&editPreset);
  menu.addAction(&removePreset);
  menu.exec(QCursor::pos());
}

void PresetDialog::warning()
{
  QMessageBox warning(this);
  warning.setText("Are you sure you want to reset? This will erase any custom "
                  "presets and restore default names.");
  warning.setStandardButtons(QMessageBox::Yes);
  warning.addButton(QMessageBox::Cancel);
  warning.setDefaultButton(QMessageBox::Cancel);
  warning.setWindowTitle("Restore Defaults");
  warning.setIcon(QMessageBox::Warning);

  if (warning.exec() == QMessageBox::Yes) {
    emit resetToDefaults();
  };
}

void PresetDialog::createSolidColormap()
{
  auto color = QColorDialog::getColor(Qt::white, this);
  if (!color.isValid()) {
    // User canceled...
    return;
  }

  QJsonArray array = { 0, color.redF(), color.greenF(), color.blueF(),
                       1, color.redF(), color.greenF(), color.blueF() };
  QJsonObject newColor{ { "name", color.name() },
                        { "colorSpace", "RGB" },
                        { "colors", array } };
  m_model->addNewPreset(newColor);
  emit applyPreset();
}

} // namespace tomviz
