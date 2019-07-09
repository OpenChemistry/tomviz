/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleThreshold_h
#define tomvizModuleThreshold_h

#include "Module.h"

#include <pqPropertyLinks.h>
#include <vtkWeakPointer.h>

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
  using Module::initialize;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return true; }

  void dataSourceMoved(double newX, double newY, double newZ) override;

protected:
  void updateColorMap() override;

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
