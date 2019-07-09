/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleVolume_h
#define tomvizModuleVolume_h

#include "Module.h"

#include <vtkNew.h>
#include <vtkWeakPointer.h>
#include <vtkSmartPointer.h>

#include <QPointer>

class vtkPVRenderView;

class vtkGPUVolumeRayCastMapper;
class vtkVolumeProperty;
class vtkVolume;

namespace tomviz {

class ModuleVolumeWidget;
class ScalarsComboBox;

class ModuleVolume : public Module
{
  Q_OBJECT

public:
  ModuleVolume(QObject* parent = nullptr);
  virtual ~ModuleVolume();

  QString label() const override { return "Volume"; }
  QIcon icon() const override;
  using Module::initialize;
  void initializeMapper(DataSource *data=nullptr);
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  bool isColorMapNeeded() const override { return true; }
  void addToPanel(QWidget* panel) override;
  void updatePanel();

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool supportsGradientOpacity() override { return true; }

  QString exportDataTypeString() override { return "Volume"; }

  vtkDataObject* dataToExport() override;

protected:
  void updateColorMap() override;

private:
  Q_DISABLE_COPY(ModuleVolume)

  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkVolume> m_volume;
  vtkSmartPointer<vtkGPUVolumeRayCastMapper> m_volumeMapper;
  vtkNew<vtkVolumeProperty> m_volumeProperty;
  QPointer<ModuleVolumeWidget> m_controllers;
  QPointer<ScalarsComboBox> m_scalarsCombo;

private slots:
  /**
   * Actuator methods for m_volumeMapper.  These slots should be connected to
   * the appropriate UI signals.
   */
  void setLighting(const bool val);
  void setBlendingMode(const int mode);
  void onInterpolationChanged(const int type);
  void setJittering(const bool val);
  void onAmbientChanged(const double value);
  void onDiffuseChanged(const double value);
  void onSpecularChanged(const double value);
  void onSpecularPowerChanged(const double value);
  void onTransferModeChanged(const int mode);
  void onScalarArrayChanged();
  int scalarsIndex();
};
} // namespace tomviz

#endif
