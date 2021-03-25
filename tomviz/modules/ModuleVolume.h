/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleVolume_h
#define tomvizModuleVolume_h

#include "Module.h"

#include <vtkNew.h>
#include <vtkWeakPointer.h>

#include <QMap>
#include <QPointer>

#include <array>

class vtkPVRenderView;

class vtkImageClip;
class vtkImageData;
class vtkPiecewiseFunction;
class vtkPlane;
class vtkVolumeProperty;
class vtkVolume;

namespace tomviz {

class ModuleVolumeWidget;
class ScalarsComboBox;
class SmartVolumeMapper;

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

  bool rgbaMappingAllowed();
  bool useRgbaMapping();
  void updateMapperInput(DataSource* data = nullptr);
  void updateRgbaMappingDataObject();
  void resetRgbaMappingRanges();
  void updateVectorMode();

  void dataSourceMoved(double newX, double newY, double newZ) override;

  bool supportsGradientOpacity() override { return true; }

  QString exportDataTypeString() override { return "Volume"; }

  vtkDataObject* dataToExport() override;

  bool updateClippingPlane(vtkPlane* plane, bool newFilter) override;

  double solidity() const;

  vtkVolume* getVolume();

protected:
  void updateColorMap() override;

private:
  Q_DISABLE_COPY(ModuleVolume)

  QString rgbaMappingComponent();
  bool rgbaMappingCombineComponents() const
  {
    return m_rgbaMappingCombineComponents;
  }
  std::array<double, 2> computeRange(const QString& component) const;
  static void computeRange(vtkDataArray* array, double range[2]);
  std::array<double, 2>& rangeForComponent(const QString& component);
  std::vector<std::array<double, 2>> activeRgbaRanges();

  vtkWeakPointer<vtkPVRenderView> m_view;
  vtkNew<vtkVolume> m_volume;
  vtkNew<SmartVolumeMapper> m_volumeMapper;
  vtkNew<vtkVolumeProperty> m_volumeProperty;
  vtkNew<vtkPiecewiseFunction> m_gradientOpacity;
  QPointer<ModuleVolumeWidget> m_controllers;
  QPointer<ScalarsComboBox> m_scalarsCombo;

  // Data object used for mapping 3-components to Rgba
  vtkNew<vtkImageData> m_rgbaDataObject;

  bool m_useRgbaMapping = false;
  bool m_rgbaMappingCombineComponents = true;
  QString m_rgbaMappingComponent;

  std::array<double, 2> m_rgbaMappingRangeAll;
  // Ranges used for Rgba data object. QString is the component name.
  QMap<QString, std::array<double, 2>> m_rgbaMappingRanges;

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
  void onRgbaMappingToggled(const bool b);
  void onRgbaMappingCombineComponentsToggled(const bool b);
  void onRgbaMappingComponentChanged(const QString& component);
  void onRgbaMappingMinChanged(const double value);
  void onRgbaMappingMaxChanged(const double value);
  void onScalarArrayChanged();
  void setSolidity(const double value);
  int scalarsIndex();
  void onAllowMultiVolumeToggled(const bool value);

  void onDataChanged();
};
} // namespace tomviz

#endif
