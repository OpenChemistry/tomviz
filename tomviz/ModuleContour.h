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

#include <QPointer>

class vtkSMProxy;
class vtkSMSourceProxy;
namespace tomviz {

class ModuleContourWidget;

class ModuleContour : public Module
{
  Q_OBJECT

public:
  ModuleContour(QObject* parent = nullptr);
  ~ModuleContour() override;

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
    setIsoValues(values);
  }

  DataSource* colorMapDataSource() const override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

  QString exportDataTypeString() override { return "Mesh"; }

  vtkSmartPointer<vtkDataObject> getDataToExport() override;

protected:
  void updateColorMap() override;
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;
  QList<DataSource*> getChildDataSources();
  void updateScalarColoring();
  void createCategoricalColoringPipeline();

  vtkWeakPointer<vtkSMSourceProxy> m_contourFilter;
  vtkWeakPointer<vtkSMSourceProxy> m_resampleFilter;
  vtkWeakPointer<vtkSMProxy> m_resampleRepresentation;
  vtkWeakPointer<vtkSMSourceProxy> m_pointDataToCellDataFilter;
  vtkWeakPointer<vtkSMProxy> m_pointDataToCellDataRepresentation;
  vtkWeakPointer<vtkSMProxy> m_activeRepresentation;

  class Private;
  Private* d;

  QPointer<ModuleContourWidget> m_controllers;

  QString m_representation;

private slots:
  /// invoked whenever a property widget changes
  void onPropertyChanged();

  void onScalarArrayChanged();

  void setUseSolidColor(const bool useSolidColor);

  void updateRangeSliders();

  /// Reset the UI for widgets not connected to a proxy property
  void updateGUI();

private:
  Q_DISABLE_COPY(ModuleContour)
};
}

#endif
