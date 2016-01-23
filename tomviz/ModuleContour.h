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
#ifndef tomvizModuleContour_h
#define tomvizModuleContour_h

#include "Module.h"
#include "vtkWeakPointer.h"

class vtkSMProxy;
class vtkSMSourceProxy;
namespace tomviz
{

class ModuleContour : public Module
{
  Q_OBJECT

  typedef Module Superclass;

public:
  ModuleContour(QObject* parent=nullptr);
  virtual ~ModuleContour();

  QString label() const override { return  "Contour"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  void addToPanel(pqProxiesWidget*) override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;
  bool isColorMapNeeded() const override { return true; }

  void setIsoValues(const QList<double>& values);
  void setIsoValue(double value)
  {
    QList<double> values;
    values << value;
    this->setIsoValues(values);
  }

protected:
  void updateColorMap() override;

  vtkWeakPointer<vtkSMSourceProxy> ContourFilter;
  vtkWeakPointer<vtkSMProxy> ContourRepresentation;

private:
  Q_DISABLE_COPY(ModuleContour)
};

}

#endif
