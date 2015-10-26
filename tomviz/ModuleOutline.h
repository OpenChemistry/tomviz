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
#ifndef tomvizModuleOutline_h
#define tomvizModuleOutline_h

#include "Module.h"
#include "vtkWeakPointer.h"

class vtkSMSourceProxy;
class vtkSMProxy;

namespace tomviz
{

/// A simple module to show the outline for any dataset.
class ModuleOutline : public Module
{
  Q_OBJECT

  typedef Module Superclass;

public:
  ModuleOutline(QObject* parent=nullptr);
  virtual ~ModuleOutline();

  virtual QString label() const override { return  "Outline"; }
  virtual QIcon icon() const override;
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  virtual bool finalize() override;
  virtual bool setVisibility(bool val) override;
  virtual bool visibility() const override;
  virtual void addToPanel(pqProxiesWidget*) override;
  virtual bool serialize(pugi::xml_node& ns) const override;
  virtual bool deserialize(const pugi::xml_node& ns) override;

private:
  Q_DISABLE_COPY(ModuleOutline)
  vtkWeakPointer<vtkSMSourceProxy> OutlineFilter;
  vtkWeakPointer<vtkSMProxy> OutlineRepresentation;
};

}

#endif
