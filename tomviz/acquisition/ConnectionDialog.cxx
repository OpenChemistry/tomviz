/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "ConnectionDialog.h"
#include "ui_ConnectionDialog.h"

#include <QIntValidator>

namespace tomviz {

ConnectionDialog::ConnectionDialog(const QString name, const QString hostName,
                                   int port, QWidget* parent)
  : QDialog(parent), m_ui(new Ui::ConnectionDialog)
{
  m_ui->setupUi(this);
  m_ui->portLineEdit->setValidator(new QIntValidator(1024, 65535, this));

  m_ui->nameLineEdit->setText(name);
  m_ui->hostNameLineEdit->setText(hostName);
  m_ui->portLineEdit->setText(QString::number(port));
}

ConnectionDialog::~ConnectionDialog() = default;

QString ConnectionDialog::name()
{
  return m_ui->nameLineEdit->text();
}

QString ConnectionDialog::hostName()
{
  return m_ui->hostNameLineEdit->text();
}

int ConnectionDialog::port()
{
  return m_ui->portLineEdit->text().toInt();
}
}
