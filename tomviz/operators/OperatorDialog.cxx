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
