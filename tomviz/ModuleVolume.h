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

class vtkGPUVolumeRayCastMapper;
class vtkVolumeProperty;
class vtkVolume;

namespace tomviz {

class ModuleVolumeWidget;

class ModuleVolume : public Module
{
  Q_OBJECT

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

  bool supportsGradientOpacity() override { return true; }

  QString exportDataTypeString() override { return "Volume"; }

  vtkSmartPointer<vtkDataObject> getDataToExport() override;

protected:
  void updateColorMap() override;
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private:
  Q_DISABLE_COPY(ModuleVolume)

  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkVolume> m_volume;
  vtkNew<vtkGPUVolumeRayCastMapper> m_volumeMapper;
  vtkNew<vtkVolumeProperty> m_volumeProperty;
  ModuleVolumeWidget* m_controllers = nullptr;

private slots:
  /**
   * Actuator methods for m_volumeMapper.  These slots should be connected to
   * the appropriate UI signals.
   */
  void setLighting(const bool val);
  void setBlendingMode(const int mode);
  void onInterpolationChanged(const int type);
  void setJittering(const bool val);
  void onAmbientChanged(const double value);
  void onDiffuseChanged(const double value);
  void onSpecularChanged(const double value);
  void onSpecularPowerChanged(const double value);
  void onTransferModeChanged(const int mode);
};
}

#endif
