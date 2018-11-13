/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorDialog.h"

#include "OperatorWidget.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace tomviz {

OperatorDialog::OperatorDialog(QWidget* parentObject) : Superclass(parentObject)
{
  m_ui = new OperatorWidget(this);
  QVBoxLayout* layout = new QVBoxLayout(this);
  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  this->setLayout(layout);
  layout->addWidget(m_ui);
  layout->addWidget(buttons);
}

OperatorDialog::~OperatorDialog() {}

void OperatorDialog::setJSONDescription(const QString& json)
{
  m_ui->setupUI(json);
}

QMap<QString, QVariant> OperatorDialog::values() const
{
  return m_ui->values();
}
} // namespace tomviz
