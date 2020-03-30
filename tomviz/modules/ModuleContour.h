/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleContour_h
#define tomvizModuleContour_h

#include "Module.h"

#include <vtkNew.h>

class vtkActor;
class vtkDataSetMapper;
class vtkFlyingEdges3D;
class vtkProbeFilter;
class vtkProperty;
class vtkPVRenderView;

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
  using Module::initialize;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  void addToPanel(QWidget*) override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return true; }

  void dataSourceMoved(double newX, double newY, double newZ) override;

  void setIsoValue(double value);
  void resetIsoValue();

  QString exportDataTypeString() override { return "Mesh"; }

  vtkDataObject* dataToExport() override;

  bool colorMapData() const;
  double ambient() const;
  double diffuse() const;
  double specular() const;
  double specularPower() const;
  double iso() const;
  QString representation() const;
  double opacity() const;
  QColor color() const;
  bool useSolidColor() const;
  bool colorByArray() const;
  QString colorByArrayName() const;
  bool updateClippingPlane(vtkPlane* plane, bool newFilter) override;

protected:
  void updatePanel();
  void updateColorMap() override;
  void updateColorArray();
  void updateColorArrayDataSet();
  void clearColorArrayDataSet();
  void updateIsoRange();
  void updateColorByArrayOptions();

  vtkNew<vtkActor> m_actor;
  vtkNew<vtkDataSetMapper> m_mapper;
  vtkNew<vtkProperty> m_property;
  vtkNew<vtkFlyingEdges3D> m_flyingEdges;
  vtkNew<vtkProbeFilter> m_probeFilter;
  vtkWeakPointer<vtkPVRenderView> m_view;

  class Private;
  Private* d;

  QPointer<ModuleContourWidget> m_controllers;

  QString m_representation;

private slots:
  void onActiveScalarsChanged();
  void onDataPropertiesChanged();
  void onColorMapDataToggled(const bool state);
  void onAmbientChanged(const double value);
  void onDiffuseChanged(const double value);
  void onSpecularChanged(const double value);
  void onSpecularPowerChanged(const double value);
  void onIsoChanged(const double value);
  void onRepresentationChanged(const QString& representation);
  void onOpacityChanged(const double value);
  void onColorChanged(const QColor& color);
  void onUseSolidColorToggled(const bool state);
  void onColorByArrayToggled(const bool state);
  void onColorByArrayNameChanged(const QString& name);

private:
  Q_DISABLE_COPY(ModuleContour)
};
} // namespace tomviz

#endif
