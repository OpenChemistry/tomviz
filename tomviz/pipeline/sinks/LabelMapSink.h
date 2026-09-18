/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLabelMapSink_h
#define tomvizPipelineLabelMapSink_h

#include "VolumeSink.h"

#include <QJsonObject>
#include <QVector>

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkType.h>

#include <array>
#include <memory>

class vtkActor;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkProperty;

namespace tomviz {
namespace pipeline {

class LabelMapData;
using LabelMapDataPtr = std::shared_ptr<LabelMapData>;

/// Label map visualization sink.
///
/// Draws the labels either as surfaces or as a volume. Surfaces are the
/// default: gradient-shaded volume rendering has nothing to shade inside
/// a piecewise-constant label map, so away from the one-voxel boundary
/// shell every sample comes out unlit, while a Surface Nets mesh has a
/// real normal everywhere. The volume representation stays available for
/// translucent overlays and for label maps too large to mesh; it renders
/// through the same VTK pipeline as VolumeSink, so mappers, bricking,
/// clipping and lighting carry over unchanged.
///
/// Either way the panel hides the continuous-scalar settings and lists
/// the labels present in the data, each with a checkbox that shows or
/// hides it and a color the user can pick.
///
/// Those choices live in the payload's LabelTable, which this sink
/// projects onto the color and scalar-opacity transfer functions. It
/// never edits those functions as state of their own: they are rebuilt
/// from the table on every edit and on every execution.
class LabelMapSink : public VolumeSink
{
  Q_OBJECT

public:
  LabelMapSink(QObject* parent = nullptr);
  ~LabelMapSink() override;

  QIcon icon() const override;

  void setVisibility(bool visible) override;
  bool initialize(vtkSMViewProxy* view) override;
  bool finalize() override;
  void clearVisualization() override;
  void addClippingPlane(vtkPlane* plane) override;
  void removeClippingPlane(vtkPlane* plane) override;
  void onMetadataChanged() override;

  QWidget* createSinkPropertiesWidget(QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  enum class Representation
  {
    Surface = 0,
    Volume
  };
  Representation representation() const;
  void setRepresentation(Representation representation);

  /// Surface Nets smoothing iterations for the surface representation;
  /// 0 keeps the raw voxel faces.
  int surfaceSmoothing() const;
  void setSurfaceSmoothing(int iterations);

  /// Opacity of the surface representation, 0 to 1.
  double surfaceOpacity() const;
  void setSurfaceOpacity(double opacity);

  /// The mesh currently drawn by the surface representation, empty
  /// when there is none. Exposed for tests.
  vtkPolyData* surface() const;

  /// The label map payload this sink last consumed. When the port
  /// carries a plain volume whose values read as labels, this is a view
  /// this sink built over the same voxels instead. Null when there is no
  /// data yet, or the data cannot be read as labels at all.
  LabelMapDataPtr labelMap() const;

  /// Project the label table onto whichever color map this sink renders
  /// through - the payload's shared one, or its own detached one - and
  /// request a redraw.
  void applyLabels();

  /// The label table was edited through this sink's panel: re-project
  /// it and tell anything bound to the labels (a Remove Labels editor).
  void labelTableEdited();

  /// The labels the user has hidden, ascending; the background is never
  /// among them.
  QVector<double> hiddenLabels() const;
  /// Hide exactly @a labels and show every other label. The background
  /// keeps whatever visibility it has. Re-projects and emits
  /// labelVisibilityChanged() only if something changed.
  void setHiddenLabels(const QVector<double>& labels);

signals:
  /// Emitted when the visibility of one or more labels changed, whether
  /// through this sink's panel or setHiddenLabels().
  void labelVisibilityChanged();

  /// Emitted when the set of labels may have changed: after each
  /// execution, and after the color map this sink uses is swapped.
  void labelsChanged();

  /// Emitted when the representation or one of its settings changes.
  void representationChanged();

protected:
  bool consume(const QMap<QString, PortData>& inputs) override;
  bool volumeRenderingEnabled() const override;

private:
  /// Rebuild the surface if the data, the labels drawn or the smoothing
  /// changed since the last extraction, then recolor it and show or hide
  /// the actor as the representation and visibility require.
  void updateSurface();
  /// Move the surface actor with the data's display position,
  /// orientation, origin and spacing, without re-extracting.
  void applySurfaceTransform();
  void showSurfaceActor();

  Representation m_representation = Representation::Surface;
  int m_surfaceSmoothing = 16;
  /// The ambient floor for the volume representation has been applied.
  bool m_volumeLookApplied = false;

  /// Every face of every region, from extractLabelMesh: the expensive
  /// part, kept across visibility changes.
  vtkSmartPointer<vtkPolyData> m_mesh;
  /// The visible faces of m_mesh, what the mapper draws.
  vtkSmartPointer<vtkPolyData> m_surface;
  vtkNew<vtkPolyDataMapper> m_surfaceMapper;
  vtkNew<vtkActor> m_surfaceActor;
  vtkNew<vtkProperty> m_surfaceProperty;
  /// What m_mesh was extracted from, to skip a redundant extraction.
  struct MeshKey
  {
    vtkImageData* image = nullptr;
    vtkMTimeType imageTime = 0;
    QVector<double> regions;
    int smoothing = -1;
    bool operator==(const MeshKey& other) const
    {
      return image == other.image && imageTime == other.imageTime &&
             regions == other.regions && smoothing == other.smoothing;
    }
  };
  MeshKey m_meshKey;
  /// The labels m_surface was selected for.
  QVector<double> m_surfaceVisible;
  /// Origin and spacing baked into m_surface's geometry.
  std::array<double, 3> m_surfaceOrigin = { 0.0, 0.0, 0.0 };
  std::array<double, 3> m_surfaceSpacing = { 0.0, 0.0, 0.0 };

  /// A label view over a port that is not typed as a label map. Held
  /// here rather than published, because the port's payload is shared
  /// with every other sink reading it and they are entitled to go on
  /// seeing a plain volume.
  LabelMapDataPtr m_adopted;

  /// An adopted table read from a state file, held until there is data
  /// to attach it to. Deserialize runs before the sink has consumed
  /// anything, so there is no view to put it in yet.
  QJsonObject m_restoredAdopted;
};

} // namespace pipeline
} // namespace tomviz

#endif
