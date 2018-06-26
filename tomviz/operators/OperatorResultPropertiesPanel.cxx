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
#include "OperatorResultPropertiesPanel.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "OperatorResult.h"
#include "Pipeline.h"
#include "Utilities.h"

#include "vtkMolecule.h"

#include <QAbstractButton>
#include <QDebug>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace tomviz {

OperatorResultPropertiesPanel::OperatorResultPropertiesPanel(QWidget* p)
  : QWidget(p)
{
  // Show active module in the "OperatorResult Properties" panel.
  connect(&ActiveObjects::instance(), SIGNAL(resultChanged(OperatorResult*)),
          SLOT(setOperatorResult(OperatorResult*)));

  // Set up a very simple layout with a description label widget.
  m_layout = new QVBoxLayout;
  setLayout(m_layout);
}

OperatorResultPropertiesPanel::~OperatorResultPropertiesPanel() = default;

void OperatorResultPropertiesPanel::setOperatorResult(OperatorResult* result)
{
  qDebug() << "OperatorResultPropertiesPanel::setResult";
  if (result != m_activeOperatorResult) {
    deleteLayoutContents(m_layout);
    if (result) {
      if (vtkMolecule::SafeDownCast(result->dataObject())) {
        makeMoleculeProperties(vtkMolecule::SafeDownCast(result->dataObject()));
      }
      m_layout->addStretch();
    }
  }

  m_activeOperatorResult = result;
}

void OperatorResultPropertiesPanel::makeMoleculeProperties(
  vtkMolecule* molecule)
{
  m_layout->addWidget(new QLabel("Atomic Positions"));
  table = new QTableWidget();
}

} // namespace tomviz
