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

#include <QLabel>
#include <QVBoxLayout>

namespace tomviz {

OperatorPropertiesPanel::OperatorPropertiesPanel(QWidget* p) : QWidget(p)
{
  // Show active module in the "Module Properties" panel.
  connect(&ActiveObjects::instance(), SIGNAL(operatorChanged(Operator*)),
          SLOT(setOperator(Operator*)));

  // Set up a very simple layout with a description label widget.
  auto layout = new QVBoxLayout;
  m_description = new QLabel("None");
  layout->addWidget(m_description);
  layout->addStretch();
  setLayout(layout);
}

OperatorPropertiesPanel::~OperatorPropertiesPanel() = default;

void OperatorPropertiesPanel::setOperator(Operator* op)
{
  if (op != m_activeOperator) {
    if (m_activeOperator) {
      disconnect(op, SIGNAL(labelModified()), this, SLOT(updatePanel()));
    }
    if (op) {
      connect(op, SIGNAL(labelModified()), SLOT(updatePanel()));
    }
  }

  m_activeOperator = op;
  updatePanel();
}

void OperatorPropertiesPanel::updatePanel()
{
  m_description->setText(m_activeOperator->label());
}
}
