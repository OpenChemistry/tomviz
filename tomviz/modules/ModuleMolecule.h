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
#ifndef tomvizModuleMolecule_h
#define tomvizModuleMolecule_h

#include "Module.h"
#include "vtkActor.h"
#include "vtkMoleculeMapper.h"
#include "vtkPVRenderView.h"
#include <pqPropertyLinks.h>
#include <vtkWeakPointer.h>

class QCheckBox;
class vtkMolecule;
class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz {

class OperatorResult;

class ModuleMolecule : public Module
{
  Q_OBJECT

public:
  ModuleMolecule(QObject* parent = nullptr);
  ~ModuleMolecule() override;

  QString label() const override { return "Molecule"; }
  QIcon icon() const override;
  bool initializeWithResult(DataSource* dataSource, vtkSMViewProxy* view,
                            OperatorResult* result) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QString exportDataTypeString() override { return "Molecule"; }
  vtkSmartPointer<vtkDataObject> getDataToExport() override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;
  void dataSourceMoved(double newX, double newY, double newZ) override;

protected:
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void ballRadiusChanged(double val);
  void bondRadiusChanged(double val);

private:
  Q_DISABLE_COPY(ModuleMolecule)
  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkMolecule* m_molecule;
  vtkNew<vtkMoleculeMapper> m_moleculeMapper;
  vtkNew<vtkActor> m_moleculeActor;

  pqPropertyLinks m_links;
};
} // namespace tomviz
#endif
