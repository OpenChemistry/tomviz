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
#ifndef tomvizOperatorResultPropertiesPanel_h
#define tomvizOperatorResultPropertiesPanel_h

#include <QWidget>

#include <QPointer>

class QLabel;
class QTableWidget;
class QVBoxLayout;
class vtkMolecule;

namespace tomviz {
class OperatorResult;

class OperatorResultPropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  OperatorResultPropertiesPanel(QWidget* parent = nullptr);
  virtual ~OperatorResultPropertiesPanel();

private slots:
  void setOperatorResult(OperatorResult*);

private:
  Q_DISABLE_COPY(OperatorResultPropertiesPanel)

  void makeMoleculeProperties(vtkMolecule* molecule);
  QTableWidget* initializeAtomTable();
  void populateAtomTable(QTableWidget* table, vtkMolecule* molecule);
  QPointer<OperatorResult> m_activeOperatorResult = nullptr;
  QVBoxLayout* m_layout = nullptr;
};
} // namespace tomviz

#endif
