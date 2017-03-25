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
#include <vtkWeakPointer.h>

#include <pqPropertyLinks.h>

class vtkSMSourceProxy;
class vtkSMProxy;
class vtkPVRenderView;
class vtkGridAxes3DActor;

namespace tomviz {

/// A simple module to show the outline for any dataset.
class ModuleOutline : public Module
{
  Q_OBJECT

public:
  ModuleOutline(QObject* parent = nullptr);
  ~ModuleOutline() override;

  QString label() const override { return "Outline"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected:
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void dataUpdated();

  void initializeGridAxes(DataSource* dataSource, vtkSMViewProxy* vtkView);
  void updateGridAxesBounds(DataSource* dataSource);
  void updateGridAxesColor(double* color);
  void updateGridAxesUnit(DataSource* dataSource);

private:
  Q_DISABLE_COPY(ModuleOutline)
  vtkWeakPointer<vtkSMSourceProxy> m_outlineFilter;
  vtkWeakPointer<vtkSMProxy> m_outlineRepresentation;
  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkGridAxes3DActor> m_gridAxes;
  pqPropertyLinks m_links;
  bool m_axesVisibility = false;
};
}

#endif
