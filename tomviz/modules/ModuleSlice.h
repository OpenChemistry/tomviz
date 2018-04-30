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

public:
  ModuleSlice(QObject* parent = nullptr);
  virtual ~ModuleSlice();

  QString label() const override { return "Slice"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return true; }
  void addToPanel(QWidget* panel) override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

  QString exportDataTypeString() override { return "Image"; }

  vtkSmartPointer<vtkDataObject> getDataToExport() override;

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

  vtkWeakPointer<vtkSMSourceProxy> m_passThrough;
  vtkSmartPointer<vtkSMProxy> m_propsPanelProxy;
  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> m_widget;
  bool m_ignoreSignals = false;

  pqPropertyLinks m_Links;
};
} // namespace tomviz

#endif
