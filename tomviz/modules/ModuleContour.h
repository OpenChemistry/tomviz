/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleContour_h
#define tomvizModuleContour_h

#include "Module.h"

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
  double getIsoValue() const;

  DataSource* colorMapDataSource() const override;

  QString exportDataTypeString() override { return "Mesh"; }

  vtkDataObject* dataToExport() override;

protected:
  void updateColorMap() override;
  QList<DataSource*> getChildDataSources();
  void updateScalarColoring();

  vtkWeakPointer<vtkSMSourceProxy> m_contourFilter;
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

private:
  Q_DISABLE_COPY(ModuleContour)
};
} // namespace tomviz

#endif
