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
#include "MoleculePropertiesPanel.h"

#include "ActiveObjects.h"
#include "MoleculeProperties.h"
#include "MoleculeSource.h"

#include <pqProxyWidget.h>

#include <QDebug>
#include <QLineEdit>
#include <QVBoxLayout>

namespace tomviz {

MoleculePropertiesPanel::MoleculePropertiesPanel(QWidget* parent)
  : QWidget(parent)
{

  m_layout = new QVBoxLayout();
  QWidget* separator = pqProxyWidget::newGroupLabelWidget("Filename", this);
  m_label = new QLineEdit();
  m_label->setAutoFillBackground(true);
  m_label->setFrame(false);
  m_label->setReadOnly(true);
  m_layout->addWidget(separator);
  m_layout->addWidget(m_label);
  m_layout->addStretch();
  this->setLayout(m_layout);

  connect(&ActiveObjects::instance(),
          SIGNAL(moleculeSourceChanged(MoleculeSource*)),
          SLOT(setMoleculeSource(MoleculeSource*)));
  update();
}

MoleculePropertiesPanel::~MoleculePropertiesPanel()
{
}

void MoleculePropertiesPanel::setMoleculeSource(MoleculeSource* source)
{
  m_currentMoleculeSource = source;
  update();
}

void MoleculePropertiesPanel::update()
{
  if (m_moleculeProperties) {
    m_layout->removeWidget(m_moleculeProperties);
    delete m_moleculeProperties;
    m_moleculeProperties = nullptr;
  }

  if (m_currentMoleculeSource) {
    auto fileName = m_currentMoleculeSource->fileName();
    m_label->setText(fileName);
    auto molecule = m_currentMoleculeSource->molecule();
    m_moleculeProperties = new MoleculeProperties(molecule);
    m_layout->insertWidget(m_layout->count() - 1, m_moleculeProperties);

  } else {
    m_label->setText(QString());
  }
}

} // tomviz
