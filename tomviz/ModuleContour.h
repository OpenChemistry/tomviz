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
#include <vtkWeakPointer.h>

class vtkSMProxy;
class vtkSMSourceProxy;
namespace tomviz {

class ModuleContour : public Module
{
  Q_OBJECT

  typedef Module Superclass;

public:
  ModuleContour(QObject* parent = nullptr);
  virtual ~ModuleContour();

  QString label() const override { return "Contour"; }
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  void addToPanel(QWidget*) override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;
  bool isColorMapNeeded() const override { return true; }

  void dataSourceMoved(double newX, double newY, double newZ) override;

  void setIsoValues(const QList<double>& values);
  void setIsoValue(double value)
  {
    QList<double> values;
    values << value;
    this->setIsoValues(values);
  }

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

protected:
  void updateColorMap() override;
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

  vtkWeakPointer<vtkSMSourceProxy> ContourFilter;
  vtkWeakPointer<vtkSMProxy> ContourRepresentation;
  vtkWeakPointer<vtkSMSourceProxy> ResampleFilter;

  class Private;
  Private* Internals;

  QString Representation;

private slots:
  void dataUpdated();

  // The parameter should really be a bool, but the signal gives an int
  void setUseSolidColor(int useSolidColor);

private:
  Q_DISABLE_COPY(ModuleContour)
};
}

#endif
