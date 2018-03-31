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
#ifndef tomvizModuleScaleCube_h
#define tomvizModuleScaleCube_h

#include "Module.h"

#include <vtkNew.h>
#include <vtkWeakPointer.h>

class vtkSMProxy;
class vtkSMSourceProxy;

class vtkPVRenderView;

class vtkHandleWidget;
class vtkMeasurementCubeHandleRepresentation3D;

namespace tomviz {

class ModuleScaleCubeWidget;

class ModuleScaleCube : public Module
{
  Q_OBJECT

public:
  ModuleScaleCube(QObject* parent = nullptr);
  ~ModuleScaleCube() override;

  QString label() const override { return "Scale Cube"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool visibility() const override;
  bool setVisibility(bool choice) override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;
  void addToPanel(QWidget* panel) override;

  void dataSourceMoved(double, double, double) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected slots:
  void dataPropertiesChanged();

protected:
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private:
  Q_DISABLE_COPY(ModuleScaleCube)

  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkHandleWidget> m_handleWidget;
  vtkNew<vtkMeasurementCubeHandleRepresentation3D> m_cubeRep;
  ModuleScaleCubeWidget* m_controllers = nullptr;
  unsigned long m_observedPositionId;
  unsigned long m_observedSideLengthId;
  bool m_annotationVisibility = true;
  double m_offset[3];

signals:
  /**
   * relaying changes from m_cubeRep.
   */
  void onPositionChanged();
  void onPositionChanged(const double, const double, const double);

  void onSideLengthChanged();
  void onSideLengthChanged(const double);

  /**
   * Relaying changes from the data
   */
  void onLengthUnitChanged(const QString);
  void onPositionUnitChanged(const QString);

private slots:
  /**
   * Actuator methods for m_cubeRep.  These slots should be connected to the
   * appropriate UI signals.
   */
  void setAdaptiveScaling(const bool);
  void setSideLength(const double);
  void setAnnotation(const bool);

  void setLengthUnit();
  void setPositionUnit();
  void onBoxColorChanged(const QColor& color);

  void updateOffset(double, double, double);
};
}

#endif
