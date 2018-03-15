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

}
