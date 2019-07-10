/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleOutline_h
#define tomvizModuleOutline_h

#include "Module.h"
#include <vtkWeakPointer.h>

#include <pqPropertyLinks.h>

class vtkSMSourceProxy;
class vtkSMProxy;
class vtkPVRenderView;
class vtkGridAxes3DActor;

namespace tomviz {

/// A simple module to show the outline for any dataset.
class ModuleOutline : public Module
{
  Q_OBJECT

public:
  ModuleOutline(QObject* parent = nullptr);
  ~ModuleOutline() override;

  QString label() const override { return "Outline"; }
  QIcon icon() const override;
  using Module::initialize;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

private slots:
  void dataUpdated();

  void initializeGridAxes(DataSource* dataSource, vtkSMViewProxy* vtkView);
  void updateGridAxesBounds(DataSource* dataSource);
  void updateGridAxesColor(double* color);
  void updateGridAxesUnit(DataSource* dataSource);

private:
  Q_DISABLE_COPY(ModuleOutline)
  vtkWeakPointer<vtkSMSourceProxy> m_outlineFilter;
  vtkWeakPointer<vtkSMProxy> m_outlineRepresentation;
  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkGridAxes3DActor> m_gridAxes;
  pqPropertyLinks m_links;
  bool m_axesVisibility = false;
};
} // namespace tomviz

#endif
