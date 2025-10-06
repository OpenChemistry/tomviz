/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleSlice_h
#define tomvizModuleSlice_h

#include "Module.h"

#include <vtkNew.h>
#include <vtkSmartPointer.h>

class QCheckBox;
class QComboBox;
class QSpinBox;
class pqLineEdit;
class vtkActiveScalarsProducer;
class vtkNonOrthoImagePlaneWidget;

namespace tomviz {

class ScalarsComboBox;
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

  void planeBounds(double b[6]);

  void dataSourceMoved(double newX, double newY, double newZ) override;
  void dataSourceRotated(double newX, double newY, double newZ) override;

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

  enum Mode
  {
    Min = 0,
    Max = 1,
    Mean = 2,
    Sum = 3
  };
  Q_ENUM(Mode)

  bool showArrow() const;

  bool updateClippingPlane(vtkPlane* plane, bool newFilter) override;

  bool isOrtho() const;
  int slice() const { return m_slice; }
  int maxSlice() const;

  void onDirectionChanged(Direction direction);
  void onSliceChanged(int slice);
  void setShowArrow(bool b);

signals:

  void sliceChanged(int slice);

  void directionChanged(Direction direction);

protected:
  void updateColorMap() override;
  void updateSliceWidget();
  void updateInteractionState();
  static Direction stringToDirection(const QString& name);
  static Direction modeToDirection(int sliceMode);
  vtkImageData* imageData() const;

private slots:
  void onPlaneChanged();

  void dataChanged();

  void dataUpdated();

  void dataPropertiesChanged();

  void onScalarArrayChanged();

  void setMapScalars(bool b);

  void updatePointOnPlane();
  void updatePlaneNormal();

  void onSliceChanged(double* point);
  void onThicknessChanged(int value);
  void onThickSliceModeChanged(int index);
  int directionAxis(Direction direction) const;
  void onOpacityChanged(double opacity);

  void onTextureInterpolateChanged(bool flag);

  void setNormalToView();

private:
  bool setupWidget(vtkSMViewProxy* view);

  Q_DISABLE_COPY(ModuleSlice)

  vtkSmartPointer<vtkNonOrthoImagePlaneWidget> m_widget;
  bool m_ignoreSignals = false;

  QPointer<QCheckBox> m_opacityCheckBox;
  bool m_mapOpacity = false;

  QPointer<QCheckBox> m_mapScalarsCheckBox;
  QPointer<QComboBox> m_directionCombo;
  QPointer<QComboBox> m_sliceCombo;
  QPointer<IntSliderWidget> m_sliceSlider;
  QPointer<QSpinBox> m_thicknessSpin;
  QPointer<ScalarsComboBox> m_scalarsCombo;
  Direction m_direction = Direction::XY;
  int m_slice = 0;
  int m_sliceThickness = 1;
  Mode m_thickSliceMode = Mode::Mean;

  QPointer<QCheckBox> m_interpolateCheckBox;
  bool m_interpolate = false;

  QPointer<QCheckBox> m_showArrowCheckBox;

  QPointer<DoubleSliderWidget> m_opacitySlider;
  double m_opacity = 1;

  QPointer<pqLineEdit> m_pointInputs[3];
  QPointer<pqLineEdit> m_normalInputs[3];

  vtkNew<vtkActiveScalarsProducer> m_producer;
};
} // namespace tomviz

#endif
