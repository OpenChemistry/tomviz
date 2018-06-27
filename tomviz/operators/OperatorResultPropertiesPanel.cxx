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
#include "vtkPeriodicTable.h"

#include <QAbstractButton>
#include <QDebug>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
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
      m_layout->addWidget(new QLabel(result->label()));

      if (vtkMolecule::SafeDownCast(result->dataObject())) {
        makeMoleculeProperties(vtkMolecule::SafeDownCast(result->dataObject()));
      }
    }
  }

  m_activeOperatorResult = result;
}

void OperatorResultPropertiesPanel::makeMoleculeProperties(
  vtkMolecule* molecule)
{
  auto table = new QTableWidget();
  table->setColumnCount(4);
  QStringList labels;
  labels << "Symbol"
         << "X"
         << "Y"
         << "Z";
  table->setHorizontalHeaderLabels(labels);
  table->setRowCount(molecule->GetNumberOfAtoms());
  vtkNew<vtkPeriodicTable> periodicTable;
  for (int i = 0; i < molecule->GetNumberOfAtoms(); ++i) {
    vtkAtom atom = molecule->GetAtom(i);
    auto symbolItem = new QTableWidgetItem(
      QString(periodicTable->GetSymbol(atom.GetAtomicNumber())));
    symbolItem->setFlags(Qt::ItemIsEnabled);

    auto position = atom.GetPosition();
    auto xItem = new QTableWidgetItem(QString::number(position[0]));
    xItem->setFlags(Qt::ItemIsEnabled);
    auto yItem = new QTableWidgetItem(QString::number(position[1]));
    yItem->setFlags(Qt::ItemIsEnabled);
    auto zItem = new QTableWidgetItem(QString::number(position[2]));
    zItem->setFlags(Qt::ItemIsEnabled);

    table->setItem(i, 0, symbolItem);
    table->setItem(i, 1, xItem);
    table->setItem(i, 2, yItem);
    table->setItem(i, 3, zItem);
  }

  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

  // Button to save molecule to file
  auto saveButton = new QPushButton("Export to file");
  connect(saveButton, &QPushButton::clicked, this,
          [molecule]() { moleculeToFile(molecule); });

  m_layout->addWidget(saveButton);
  m_layout->addWidget(table);
}

} // namespace tomviz
