/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConformVolumeDialog.h"
#include "ui_ConformVolumeDialog.h"

#include "DataSource.h"

namespace tomviz {

ConformVolumeDialog::ConformVolumeDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::ConformVolumeDialog)
{
  m_ui->setupUi(this);

  setupConnections();
}

ConformVolumeDialog::~ConformVolumeDialog() = default;

void ConformVolumeDialog::setupConnections()
{
  connect(m_ui->conformingVolume,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ConformVolumeDialog::updateConformToLabel);
}

void ConformVolumeDialog::updateConformToLabel()
{
  auto current = m_ui->conformingVolume->currentText();
  for (const auto* volume : m_volumes) {
    if (current != volume->label()) {
      m_ui->conformToVolumeLabel->setText(volume->label());
      break;
    }
  }
}

void ConformVolumeDialog::setVolumes(QList<DataSource*> volumes)
{
  m_volumes = volumes;

  // Set up the combo box options
  m_ui->conformingVolume->clear();
  for (const auto* volume : volumes) {
    m_ui->conformingVolume->addItem(volume->label());
  }
}

DataSource* ConformVolumeDialog::selectedVolume()
{
  auto selectedIndex = m_ui->conformingVolume->currentIndex();
  return m_volumes[selectedIndex];
}

} // namespace tomviz
