/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleSlice_h
#define tomvizModuleSlice_h

#include "Module.h"

#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <pqPropertyLinks.h>

class QCheckBox;
class QComboBox;
class pqLineEdit;
class vtkSMProxy;
class vtkSMSourceProxy;
class vtkNonOrthoImagePlaneWidget;

namespace tomviz {

class DoubleSliderWidget;
class IntSliderWidget;

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

  QString exportDataTypeString() override { return "Image"; }

  vtkDataObject* dataToExport() override;

  enum Direction
  {
    XY = 0,
    YZ = 1,
    XZ = 2,
    Custom = 3
  };
  Q_ENUM(Direction)

protected:
  void updateColorMap() override;
  void updateSliceWidget();
  static Direction stringToDirection(const QString& name);
  static Direction modeToDirection(int sliceMode);
  vtkImageData* imageData() const;

private slots:
  void onPropertyChanged();
  void onPlaneChanged();

  void dataUpdated();

  void onDirectionChanged(Direction direction);
  void onSliceChanged(int slice);
  void onSliceChanged(double* point);
  int directionAxis(Direction direction);
  void onOpacityChanged(double opacity);

  void onTextureInterpolateChanged(bool flag);

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

  QPointer<QComboBox> m_directionCombo;
  QPointer<IntSliderWidget> m_sliceSlider;
  Direction m_direction = Direction::XY;
  int m_slice = 0;

  QPointer<QCheckBox> m_interpolateCheckBox;
  bool m_interpolate = false;

  QPointer<DoubleSliderWidget> m_opacitySlider;
  double m_opacity = 1;

  QPointer<pqLineEdit> m_pointInputs[3];
  QPointer<pqLineEdit> m_normalInputs[3];
};
} // namespace tomviz

#endif
