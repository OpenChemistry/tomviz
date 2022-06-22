/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataBrokerSaveDialog.h"
#include "DataBroker.h"
#include "Utilities.h"

#include "ui_DataBrokerSaveDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QTreeWidget>

#include <pqSettings.h>

namespace tomviz {

DataBrokerSaveDialog::DataBrokerSaveDialog(DataBroker* dataBroker,
                                           QWidget* parent)
  : QDialog(parent), m_ui(new Ui::DataBrokerSaveDialog),
    m_dataBroker(dataBroker)
{
  m_ui->setupUi(this);

  setEnabledOkButton(false);

  connect(m_ui->nameLineEdit, &QLineEdit::textChanged, this,
          [this](const QString& name) {
            if (name.size() > 0) {
              m_name = name;
              setEnabledOkButton(true);
            }
          });
}

DataBrokerSaveDialog::~DataBrokerSaveDialog() = default;

void DataBrokerSaveDialog::setEnabledOkButton(bool enabled)
{
  auto okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);
  okButton->setEnabled(enabled);
}

QString DataBrokerSaveDialog::name()
{
  return m_name;
}

} // namespace tomviz
