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

#ifndef tomvizModuleRuler_h
#define tomvizModuleRuler_h

#include "Module.h"

#include <vtkSmartPointer.h>

class vtkSMSourceProxy;
class vtkSMProxy;

class pqLinePropertyWidget;

namespace tomviz {

class ModuleRuler : public Module
{
  Q_OBJECT

public:
  ModuleRuler(QObject* parent = nullptr);
  virtual ~ModuleRuler();

  QString label() const override { return "Ruler"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  void addToPanel(QWidget*) override;
  void prepareToRemoveFromPanel(QWidget* panel) override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return false; }

  void dataSourceMoved(double, double, double) override {}

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected slots:
  void updateUnits();
  void updateShowLine(bool show);
  void endPointsUpdated();

signals:
  void newEndpointData(double val1, double val2);

protected:
  void updateColorMap() override {}
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

  vtkSmartPointer<vtkSMSourceProxy> m_rulerSource;
  vtkSmartPointer<vtkSMProxy> m_representation;
  QPointer<pqLinePropertyWidget> m_widget;

private:
  Q_DISABLE_COPY(ModuleRuler)

  bool m_showLine = true;
};
}

#endif
