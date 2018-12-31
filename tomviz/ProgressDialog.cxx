/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ProgressDialog.h"

#include "ui_ProgressDialog.h"

namespace tomviz {

ProgressDialog::ProgressDialog(const QString& title, const QString& msg,
                               QWidget* parent)
  : QDialog(parent), m_ui(new Ui::ProgressDialog)
{
  m_ui->setupUi(this);
  setWindowTitle(title);
  m_ui->label->setText(msg);
}

ProgressDialog::~ProgressDialog() = default;

} // namespace tomviz
