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
#ifndef tomvizModuleThreshold_h
#define tomvizModuleThreshold_h

#include "Module.h"

#include <pqPropertyLinks.h>
#include <vtkWeakPointer.h>

class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz {

class ModuleThreshold : public Module
{
  Q_OBJECT

public:
  ModuleThreshold(QObject* parent = nullptr);
  ~ModuleThreshold() override;

  QString label() const override { return "Threshold"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return true; }

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected:
  void updateColorMap() override;
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void dataUpdated();

  void onScalarArrayChanged();

private:
  Q_DISABLE_COPY(ModuleThreshold)

  pqPropertyLinks m_links;
  vtkWeakPointer<vtkSMSourceProxy> m_thresholdFilter;
  vtkWeakPointer<vtkSMProxy> m_thresholdRepresentation;
};
} // namespace tomviz

#endif
