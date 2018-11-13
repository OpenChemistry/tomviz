/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleOrthogonalSlice_h
#define tomvizModuleOrthogonalSlice_h

#include "Module.h"
#include <pqPropertyLinks.h>
#include <vtkWeakPointer.h>

class QCheckBox;
class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz {

class ModuleOrthogonalSlice : public Module
{
  Q_OBJECT

public:
  ModuleOrthogonalSlice(QObject* parent = nullptr);
  ~ModuleOrthogonalSlice() override;

  QString label() const override { return "Orthogonal Slice"; }
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
  bool isOpacityMapped() const override { return m_mapOpacity; }
  bool areScalarsMapped() const override;

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool isProxyPartOfModule(vtkSMProxy* proxy) override;

  QString exportDataTypeString() override { return "Image"; }

  vtkSmartPointer<vtkDataObject> getDataToExport() override;

protected:
  void updateColorMap() override;
  std::string getStringForProxy(vtkSMProxy* proxy) override;
  vtkSMProxy* getProxyForString(const std::string& str) override;

private slots:
  void dataUpdated();

  void onScalarArrayChanged();

private:
  Q_DISABLE_COPY(ModuleOrthogonalSlice)
  vtkWeakPointer<vtkSMSourceProxy> m_passThrough;
  vtkWeakPointer<vtkSMProxy> m_representation;

  pqPropertyLinks m_links;

  QPointer<QCheckBox> m_opacityCheckBox;
  bool m_mapOpacity = false;
};
} // namespace tomviz
#endif
