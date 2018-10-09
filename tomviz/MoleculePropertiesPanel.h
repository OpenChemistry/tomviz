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
#ifndef tomvizMoleculePropertiesPanel_h
#define tomvizMoleculePropertiesPanel_h

#include <QWidget>

#include <QPointer>

class QVBoxLayout;
class QLineEdit;

namespace tomviz {

class MoleculeSource;
class MoleculeProperties;

class MoleculePropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  explicit MoleculePropertiesPanel(QWidget* parent = nullptr);
  ~MoleculePropertiesPanel() override;

private slots:
  void setMoleculeSource(MoleculeSource*);

private:
  Q_DISABLE_COPY(MoleculePropertiesPanel)

  void update();

  QPointer<MoleculeSource> m_currentMoleculeSource;
  QVBoxLayout* m_layout;
  QLineEdit* m_label;
  MoleculeProperties* m_moleculeProperties = nullptr;
};
}

#endif
