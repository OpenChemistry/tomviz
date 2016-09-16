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
#ifndef tomvizModuleSlice_h
#define tomvizModuleSlice_h

#include "Module.h"
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <pqPropertyLinks.h>

class vtkSMProxy;
class vtkSMSourceProxy;
class vtkNonOrthoImagePlaneWidget;

namespace tomviz {
class ModuleSlice : public Module
{
  Q_OBJECT
  typedef Module Superclass;

public:
  ModuleSlice(QObject* parent = nullptr);
  virtual ~ModuleSlice();

  QString label() const override { return "Slice"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;
  bool isColorMapNeeded() const override { return true; }
  void addToPanel(QWidget* panel) override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected:
  void updateColorMap() override;
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void onPropertyChanged();
  void onPlaneChanged();

  void dataUpdated();

private:
  // Should only be called from initialize after the PassThrough has been setup.
  bool setupWidget(vtkSMViewProxy* view, vtkSMSourceProxy* producer);

  Q_DISABLE_COPY(ModuleSlice)

  vtkWeakPointer<vtkSMSourceProxy> PassThrough;
  vtkSmartPointer<vtkSMProxy> PropsPanelProxy;
  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> Widget;
  bool IgnoreSignals;

  pqPropertyLinks m_Links;
};
}

#endif
