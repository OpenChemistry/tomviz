/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleSlice_h
#define tomvizModuleSlice_h

#include "Module.h"
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <pqPropertyLinks.h>

class QCheckBox;
class vtkSMProxy;
class vtkSMSourceProxy;
class vtkNonOrthoImagePlaneWidget;

namespace tomviz {

class ScalarsComboBox;

class ModuleSlice : public Module
{
  Q_OBJECT

public:
  ModuleSlice(QObject* parent = nullptr);
  virtual ~ModuleSlice();

  QString label() const override { return "Slice"; }
  QIcon icon() const override;
  using Module::initialize;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return true; }
  bool isOpacityMapped() const override { return m_mapOpacity; }
  bool areScalarsMapped() const override;
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

  void onScalarArrayChanged();

private:
  // Should only be called from initialize after the PassThrough has been setup.
  bool setupWidget(vtkSMViewProxy* view);

  Q_DISABLE_COPY(ModuleSlice)

  vtkWeakPointer<vtkSMSourceProxy> m_passThrough;
  vtkSmartPointer<vtkSMProxy> m_propsPanelProxy;
  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> m_widget;
  bool m_ignoreSignals = false;

  pqPropertyLinks m_Links;

  QPointer<QCheckBox> m_opacityCheckBox;
  bool m_mapOpacity = false;

  vtkNew<vtkImageData> m_imageData;
  QPointer<ScalarsComboBox> m_scalarsCombo;
};
} // namespace tomviz

#endif
