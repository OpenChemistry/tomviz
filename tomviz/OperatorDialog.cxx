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

#include "DoubleSpinBox.h"
#include "InterfaceBuilder.h"
#include "SpinBox.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace tomviz {

OperatorDialog::OperatorDialog(QWidget* parentObject)
  : Superclass(parentObject)
{
}

OperatorDialog::~OperatorDialog()
{
}

void OperatorDialog::setJSONDescription(const QString& json)
{
  InterfaceBuilder* ib = new InterfaceBuilder(this);
  ib->setJSONDescription(json);
  QLayout* layout = ib->buildInterface();

  QVBoxLayout* v = new QVBoxLayout();
  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
  v->addLayout(layout);
  v->addWidget(buttons);
  this->setLayout(v);
  this->layout()->setSizeConstraint(QLayout::SetFixedSize);
}

QMap<QString, QVariant> OperatorDialog::values() const
{
  QMap<QString, QVariant> map;

  // Iterate over all children, taking the value of the named widgets
  // and stuffing them into the map.
  QList<QCheckBox*> checkBoxes = this->findChildren<QCheckBox*>();
  for (int i = 0; i < checkBoxes.size(); ++i) {
    map[checkBoxes[i]->objectName()] = (checkBoxes[i]->checkState() == Qt::Checked);
  }

  QList<tomviz::SpinBox*> spinBoxes = this->findChildren<tomviz::SpinBox*>();
  for (int i = 0; i < spinBoxes.size(); ++i) {
    map[spinBoxes[i]->objectName()] = spinBoxes[i]->value();
  }

  QList<tomviz::DoubleSpinBox*> doubleSpinBoxes = this->findChildren<tomviz::DoubleSpinBox*>();
  for (int i = 0; i < doubleSpinBoxes.size(); ++i) {
    map[doubleSpinBoxes[i]->objectName()] = doubleSpinBoxes[i]->value();
  }

  return map;
}

}
