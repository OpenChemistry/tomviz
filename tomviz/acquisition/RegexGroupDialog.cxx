/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RegexGroupDialog.h"

#include "ui_RegexGroupDialog.h"

namespace tomviz {

RegexGroupDialog::RegexGroupDialog(const QString name, QWidget* parent)
  : QDialog(parent), m_ui(new Ui::RegexGroupDialog)
{
  m_ui->setupUi(this);
  m_ui->nameLineEdit->setText(name);
}

RegexGroupDialog::~RegexGroupDialog() = default;

QString RegexGroupDialog::name()
{
  return m_ui->nameLineEdit->text();
}
} // namespace tomviz
