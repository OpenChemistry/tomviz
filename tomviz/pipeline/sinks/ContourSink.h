/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineContourSink_h
#define tomvizPipelineContourSink_h

#include "LegacyModuleSink.h"

#include <QColor>
#include <QPointer>
#include <QString>
#include <vtkNew.h>

#include <array>

class vtkActiveScalarsProducer;
class vtkActor;
class vtkColorTransferFunction;
class vtkDataSetMapper;
class vtkFlyingEdges3D;
class vtkProbeFilter;
class vtkProperty;

namespace tomviz {

class ContourSinkWidget;

namespace pipeline {

/// Isosurface/contour visualization sink using vtkFlyingEdges3D.
/// Matches the old ModuleContour pipeline with lighting, representation
/// mode, opacity, and color map support.
class ContourSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  ContourSink(QObject* parent = nullptr);
  ~ContourSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;
  bool isColorMapNeeded() const override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  double isoValue() const;
  void setIsoValue(double value);

  double opacity() const;
  void setOpacity(double value);

  /// Phong lighting parameters.
  double ambient() const;
  void setAmbient(double value);
  double diffuse() const;
  void setDiffuse(double value);
  double specular() const;
  void setSpecular(double value);
  double specularPower() const;
  void setSpecularPower(double value);

  /// Representation: VTK_SURFACE (2), VTK_WIREFRAME (1), VTK_POINTS (0).
  int representation() const;
  void setRepresentation(int rep);

  /// Solid color (used when mapScalars is false).
  void color(double rgb[3]) const;
  void setColor(double r, double g, double b);

  /// QColor convenience accessors.
  QColor qcolor() const;
  void setQColor(const QColor& c);

  /// String-based representation: "Surface", "Wireframe", "Points".
  QString representationString() const;
  void setRepresentationString(const QString& rep);

  /// Solid color toggle (separate from mapScalars).
  bool useSolidColor() const;
  void setUseSolidColor(bool state);

  /// Color by a different array than the contour array.
  bool colorByArray() const;
  void setColorByArray(bool state);
  QString colorByArrayName() const;
  void setColorByArrayName(const QString& name);

  /// Active scalar index for contour-by-array (-1 = default).
  int activeScalars() const;
  void setActiveScalars(int idx);

  /// Cached scalar range from the last consume().
  void scalarRange(double range[2]) const;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  void addClippingPlane(vtkPlane* plane) override;
  void removeClippingPlane(vtkPlane* plane) override;

  /// Toggle scalar color mapping on the surface.
  bool mapScalars() const;
  void setMapScalars(bool map);

  void onMetadataChanged() override;

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;
  void updateColorMap() override;
  void updatePanel();
  bool applyActiveScalars();
  void updateColorArray();
  void updateColorArrayProducer();
  void updateMapperInput();
  void updateScalarArrayOptions();

private:
  vtkNew<vtkActiveScalarsProducer> m_contourArrayProducer;
  vtkNew<vtkActiveScalarsProducer> m_colorArrayProducer;
  vtkNew<vtkFlyingEdges3D> m_flyingEdges;
  vtkNew<vtkProbeFilter> m_probeFilter;
  vtkNew<vtkDataSetMapper> m_mapper;
  vtkNew<vtkActor> m_actor;
  vtkNew<vtkProperty> m_property;
  double m_isoValue = 0.0;
  bool m_isoValueSet = false;
  bool m_mapScalars = true;
  bool m_useSolidColor = false;
  bool m_colorByArray = false;
  QString m_colorByArrayName;
  int m_activeScalars = -1;
  QString m_lastContourArrayName;
  double m_scalarRange[2] = { 0.0, 1.0 };
  std::array<double, 3> m_baseSpacing = { 0.0, 0.0, 0.0 };
  std::array<double, 3> m_baseOrigin = { 0.0, 0.0, 0.0 };
  QStringList m_scalarArrayNames;
  QPointer<ContourSinkWidget> m_controllers;
};

} // namespace pipeline
} // namespace tomviz

#endif
