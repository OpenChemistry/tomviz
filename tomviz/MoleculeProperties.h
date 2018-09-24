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

#ifndef tomvizMoleculeProperties_h
#define tomvizMoleculeProperties_h

#include <QWidget>

class QTableWidget;
class vtkMolecule;

namespace tomviz {
class MoleculeProperties : public QWidget
{
  Q_OBJECT

public:
  MoleculeProperties(vtkMolecule* molecule, QWidget* parent = nullptr);
  // virtual ~MoleculeProperties();
private:
  QTableWidget* initializeAtomTable();
  void populateAtomTable(QTableWidget* table, vtkMolecule* molecule);
  QMap<QString, int> moleculeSpeciesCount(vtkMolecule* molecule);
};
}

#endif
