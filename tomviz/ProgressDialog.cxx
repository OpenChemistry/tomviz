/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ProgressDialog.h"

#include "ui_ProgressDialog.h"

#include <QKeyEvent>

namespace tomviz {

ProgressDialog::ProgressDialog(const QString& title, const QString& msg,
                               QWidget* parent)
  : QDialog(parent), m_ui(new Ui::ProgressDialog)
{
  m_ui->setupUi(this);

  // Override the pqOutputWidget message pattern that was just set.
  qSetMessagePattern("[%{type}] %{message}");

  // Force full messages to be shown
  m_ui->outputWidget->showFullMessages(true);
  setWindowTitle(title);
  m_ui->label->setText(msg);

  // Hide the output widget by default
  m_ui->outputWidget->hide();

  // No close button in the corner
  setWindowFlags((windowFlags() | Qt::CustomizeWindowHint) &
                 ~Qt::WindowCloseButtonHint);
}

ProgressDialog::~ProgressDialog() = default;

void ProgressDialog::setText(QString text)
{
  m_ui->label->setText(text);
}

void ProgressDialog::showOutputWidget(bool b)
{
  m_ui->outputWidget->setVisible(b);
}

void ProgressDialog::clearOutputWidget()
{
  m_ui->outputWidget->clear();
}

void ProgressDialog::keyPressEvent(QKeyEvent* e)
{
  // Do not let the user close the dialog by pressing escape
  if (e->key() == Qt::Key_Escape) {
    return;
  }

  QDialog::keyPressEvent(e);
}

} // namespace tomviz
