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
#ifndef tomvizModuleVolume_h
#define tomvizModuleVolume_h

#include "Module.h"

#include <vtkNew.h>
#include <vtkWeakPointer.h>

class vtkSMProxy;
class vtkSMSourceProxy;

class vtkPVRenderView;

class vtkSmartVolumeMapper;
class vtkVolumeProperty;
class vtkVolume;

namespace tomviz {

class ModuleVolume : public Module
{
  Q_OBJECT

  typedef Module Superclass;

public:
  ModuleVolume(QObject* parent = nullptr);
  virtual ~ModuleVolume();

  QString label() const override { return "Volume"; }
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

private:
  Q_DISABLE_COPY(ModuleVolume)

  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkVolume> m_volume;
  vtkNew<vtkSmartVolumeMapper> m_volumeMapper;
  vtkNew<vtkVolumeProperty> m_volumeProperty;

private slots:
  void setLighting(bool val);
  void setMaximumIntensity(bool val);
};
}

#endif
