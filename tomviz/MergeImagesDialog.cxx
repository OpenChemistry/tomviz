/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MergeImagesDialog.h"
#include "ui_MergeImagesDialog.h"

namespace tomviz {

MergeImagesDialog::MergeImagesDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::MergeImagesDialog)
{
  m_ui->setupUi(this);
  m_ui->MergeImageArraysRadioButton->setChecked(true);
  m_ui->MergeImageArraysRadioButton->setChecked(false);
  m_ui->MergeArrayComponentsWidget->hide();
}

MergeImagesDialog::~MergeImagesDialog() {}

MergeImagesDialog::MergeMode MergeImagesDialog::getMode()
{
  return (m_ui->MergeImageArraysRadioButton->isChecked() ? Arrays : Components);
}

} // namespace tomviz
