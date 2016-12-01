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
#include "OperatorPropertiesPanel.h"

#include "ActiveObjects.h"
#include "Operator.h"
#include "OperatorWidget.h"
#include "Utilities.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace tomviz {

OperatorPropertiesPanel::OperatorPropertiesPanel(QWidget* p) : QWidget(p)
{
  // Show active module in the "Operator Properties" panel.
  connect(&ActiveObjects::instance(), SIGNAL(operatorChanged(Operator*)),
          SLOT(setOperator(Operator*)));

  // Set up a very simple layout with a description label widget.
  m_layout = new QVBoxLayout;
  setLayout(m_layout);
}

OperatorPropertiesPanel::~OperatorPropertiesPanel() = default;

void OperatorPropertiesPanel::setOperator(Operator* op)
{
  if (op != m_activeOperator) {
    if (m_activeOperator) {
      disconnect(op, SIGNAL(labelModified()));
    }
    deleteLayoutContents(m_layout);
    m_operatorWidget = nullptr;
    if (op) {
      // See if we are dealing with a Python operator
      OperatorPython* pythonOperator = qobject_cast<OperatorPython*>(op);
      if (pythonOperator) {
        setOperator(pythonOperator);
      } else {
        auto description = new QLabel(op->label());
        layout()->addWidget(description);
        connect(op, &Operator::labelModified, [this, description]() {
          description->setText(this->m_activeOperator->label());
        });
      }

      m_layout->addStretch();
    }
  }

  m_activeOperator = op;
}

void OperatorPropertiesPanel::setOperator(OperatorPython* op)
{
  m_operatorWidget = new OperatorWidget(this);
  m_operatorWidget->setupUI(op);

  // Check if we have any UI for this operator, there is probably a nicer
  // way todo this.
  if (m_operatorWidget->layout()->count() == 0) {
    m_operatorWidget->deleteLater();
    m_operatorWidget = nullptr;
    return;
  }

  // For now add to scroll box, out operator widget tend to be a little
  // wide!
  auto scroll = new QScrollArea(this);
  scroll->setWidget(m_operatorWidget);

  m_layout->addWidget(scroll);
  auto apply =
    new QDialogButtonBox(QDialogButtonBox::Apply, Qt::Horizontal, this);
  connect(apply, &QDialogButtonBox::clicked, this,
          &OperatorPropertiesPanel::apply);

  m_layout->addWidget(apply);
}

void OperatorPropertiesPanel::apply()
{
  cout << "here" << endl;
  if (m_operatorWidget) {
    auto values = m_operatorWidget->values();
    OperatorPython* pythonOperator =
      qobject_cast<OperatorPython*>(m_activeOperator);
    if (pythonOperator) {
      pythonOperator->setArguments(values);
      emit pythonOperator->transformModified();
    }
  }
}
}
