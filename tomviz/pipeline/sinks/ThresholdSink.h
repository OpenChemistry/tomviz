/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineThresholdSink_h
#define tomvizPipelineThresholdSink_h

#include "LegacyModuleSink.h"

#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <vtkSmartPointer.h>

#include <array>

class vtkImageData;
class vtkPlane;
class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz {

class ThresholdSinkWidget;

namespace pipeline {

/// Threshold visualization sink. Shows the region of a volume within
/// a specified scalar range, with configurable appearance properties
/// matching the old ModuleThreshold.
class ThresholdSink : public LegacyModuleSink
{
  Q_OBJECT

public:
  ThresholdSink(QObject* parent = nullptr);
  ~ThresholdSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;
  bool isColorMapNeeded() const override;

  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;

  void clearVisualization() override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  double lowerThreshold() const;
  double upperThreshold() const;
  void setThresholdRange(double lower, double upper);

signals:
  /// Emitted whenever the threshold range changes, from the panel, a
  /// state file, or a bound operator parameter.
  void thresholdRangeChanged(double lower, double upper);

public:

  double opacity() const;
  void setOpacity(double value);

  double specular() const;
  void setSpecular(double value);

  /// Representation: VTK_SURFACE (2), VTK_WIREFRAME (1), VTK_POINTS (0).
  int representation() const;
  void setRepresentation(int rep);

  /// String-based representation: "Surface", "Wireframe", "Points".
  QString representationString() const;
  void setRepresentationString(const QString& rep);

  void addClippingPlane(vtkPlane* plane) override;
  void removeClippingPlane(vtkPlane* plane) override;

  /// Toggle scalar color mapping.
  bool mapScalars() const;
  void setMapScalars(bool map);

  /// Active scalar array index for thresholding (-1 = default).
  int activeScalars() const;
  void setActiveScalars(int idx);

  /// Color by a different array than the threshold array.
  bool colorByArray() const;
  void setColorByArray(bool state);
  QString colorByArrayName() const;
  void setColorByArrayName(const QString& name);

  /// Cached scalar range from the last consume().
  void scalarRange(double range[2]) const;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  void onMetadataChanged() override;

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;
  void updateColorMap() override;
  void updatePanel();

private:
  /// Build or update the SM proxy pipeline on the main thread.
  void setupOrUpdatePipeline();
  void applyActiveScalars();
  void updateColorArray();
  void updateScalarArrayOptions();
  void applyClippingPlanes();

  vtkSmartPointer<vtkSMSourceProxy> m_producer;
  vtkSmartPointer<vtkSMSourceProxy> m_thresholdFilter;
  vtkSmartPointer<vtkSMProxy> m_thresholdRepresentation;
  vtkSmartPointer<vtkImageData> m_pendingImage;
  double m_lower = 0.0;
  double m_upper = 1.0;
  bool m_rangeSet = false;
  bool m_mapScalars = true;
  int m_activeScalars = -1;
  bool m_colorByArray = false;
  QString m_colorByArrayName;
  double m_scalarRange[2] = { 0.0, 1.0 };
  QStringList m_scalarArrayNames;
  QPointer<ThresholdSinkWidget> m_controllers;
  QSet<vtkPlane*> m_clippingPlanes;
  std::array<double, 3> m_lastSpacing = { 0.0, 0.0, 0.0 };
  std::array<double, 3> m_lastOrigin = { 0.0, 0.0, 0.0 };
};

} // namespace pipeline
} // namespace tomviz

#endif
