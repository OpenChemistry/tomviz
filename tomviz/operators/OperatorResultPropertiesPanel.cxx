/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorResultPropertiesPanel.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "MoleculeProperties.h"
#include "OperatorResult.h"
#include "Pipeline.h"
#include "Utilities.h"

#include "vtkMolecule.h"

#include <QLabel>
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
  if (result != m_activeOperatorResult) {
    deleteLayoutContents(m_layout);
    if (result) {
      m_layout->addWidget(new QLabel(result->label()));

      if (vtkMolecule::SafeDownCast(result->dataObject())) {
        auto molecule = vtkMolecule::SafeDownCast(result->dataObject());
        m_layout->addWidget(new MoleculeProperties(molecule));
      }
    }
    m_layout->addStretch();
  }

  m_activeOperatorResult = result;
}

} // namespace tomviz
