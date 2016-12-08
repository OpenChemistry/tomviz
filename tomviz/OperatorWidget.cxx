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
#include "OperatorWidget.h"

#include "DoubleSpinBox.h"
#include "InterfaceBuilder.h"
#include "SpinBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QMap>
#include <QSet>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>

namespace tomviz {

OperatorWidget::OperatorWidget(QWidget* parentObject) : Superclass(parentObject)
{
}

OperatorWidget::~OperatorWidget()
{
}

void OperatorWidget::setupUI(OperatorPython* op)
{
  QString json = op->JSONDescription();
  if (!json.isNull()) {
    InterfaceBuilder* ib = new InterfaceBuilder(this);
    ib->setJSONDescription(json);
    ib->setParameterValues(op->arguments());
    buildInterface(ib);
  }
}

void OperatorWidget::setupUI(const QString& json)
{
  InterfaceBuilder* ib = new InterfaceBuilder(this);
  ib->setJSONDescription(json);
  buildInterface(ib);
}

void OperatorWidget::buildInterface(InterfaceBuilder* builder)
{
  QLayout* layout = builder->buildInterface();
  this->setLayout(layout);
}

QMap<QString, QVariant> OperatorWidget::values() const
{
  QMap<QString, QVariant> map;

  // Iterate over all children, taking the value of the named widgets
  // and stuffing them into the map.
  QList<QCheckBox*> checkBoxes = this->findChildren<QCheckBox*>();
  for (int i = 0; i < checkBoxes.size(); ++i) {
    map[checkBoxes[i]->objectName()] =
      (checkBoxes[i]->checkState() == Qt::Checked);
  }

  QList<tomviz::SpinBox*> spinBoxes = this->findChildren<tomviz::SpinBox*>();
  for (int i = 0; i < spinBoxes.size(); ++i) {
    map[spinBoxes[i]->objectName()] = spinBoxes[i]->value();
  }

  QList<tomviz::DoubleSpinBox*> doubleSpinBoxes =
    this->findChildren<tomviz::DoubleSpinBox*>();
  for (int i = 0; i < doubleSpinBoxes.size(); ++i) {
    map[doubleSpinBoxes[i]->objectName()] = doubleSpinBoxes[i]->value();
  }

  QList<QComboBox*> comboBoxes = this->findChildren<QComboBox*>();
  for (int i = 0; i < comboBoxes.size(); ++i) {
    int currentIndex = comboBoxes[i]->currentIndex();
    map[comboBoxes[i]->objectName()] = comboBoxes[i]->itemData(currentIndex);
  }

  // Assemble multi-component properties into single properties in the map.
  QMap<QString, QVariant>::iterator iter = map.begin();
  while (iter != map.end()) {
    QString name = iter.key();
    QVariant value = iter.value();
    int poundIndex = name.indexOf(tr("#"));
    if (poundIndex >= 0) {
      QString indexString = name.mid(poundIndex + 1);

      // Keep the part of the name to the left of the '#'
      name = name.left(poundIndex);

      QList<QVariant> valueList;
      QMap<QString, QVariant>::iterator findIter = map.find(name);
      if (findIter != map.end()) {
        valueList = map[name].toList();
      }

      // The QMap keeps entries sorted by lexicographic order, so we
      // can just append to the list and the elements will be inserted
      // in the correct order.
      valueList.append(value);
      map[name] = valueList;

      // Delete the individual component map entry. Doing so increments the
      // iterator.
      iter = map.erase(iter);
    } else {
      // Single-element parameter, nothing to do
      ++iter;
    }
  }

  return map;
}
}
