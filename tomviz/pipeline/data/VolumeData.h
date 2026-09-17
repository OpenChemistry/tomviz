/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineVolumeData_h
#define tomvizPipelineVolumeData_h

#include <QJsonObject>
#include <QString>

#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <QList>
#include <QMap>
#include <QStringList>
#include <QVector>

#include <array>
#include <memory>

class vtkColorTransferFunction;
class vtkImageData;
class vtkDataArray;
class vtkPiecewiseFunction;
class vtkSMProxy;

namespace tomviz {
namespace pipeline {

/// Lightweight wrapper around vtkImageData providing essential metadata
/// accessors for pipeline port data. This is a value-semantic type
/// (shared via shared_ptr) with no QObject overhead.
class VolumeData
{
public:
  VolumeData();
  explicit VolumeData(vtkSmartPointer<vtkImageData> imageData);
  /// Virtual so LabelMapData can extend the payload; port data is always
  /// shared as VolumeDataPtr, so subclasses are destroyed through this.
  virtual ~VolumeData();

  // Non-copyable: the volume data can be large and the vtkNew member is not
  // copyable anyway. Use the explicit copyColorMapFrom() helper or share via
  // VolumeDataPtr instead of making accidental value copies. Movable only.
  VolumeData(const VolumeData&) = delete;
  VolumeData& operator=(const VolumeData&) = delete;
  VolumeData(VolumeData&&) = default;
  VolumeData& operator=(VolumeData&&) = default;

  /// Access the underlying vtkImageData
  vtkImageData* imageData() const;
  void setImageData(vtkSmartPointer<vtkImageData> data);

  /// Returns true if the underlying image data is non-null
  bool isValid() const;

  // -- Geometry accessors --

  /// Get the dimensions (number of voxels along each axis)
  std::array<int, 3> dimensions() const;

  /// Get the spacing (physical size of each voxel)
  std::array<double, 3> spacing() const;
  void setSpacing(double x, double y, double z);

  /// Get/set the origin
  std::array<double, 3> origin() const;
  void setOrigin(double x, double y, double z);

  /// Get the extent [xmin, xmax, ymin, ymax, zmin, zmax]
  std::array<int, 6> extent() const;

  /// Get the physical bounds [xmin, xmax, ymin, ymax, zmin, zmax]
  std::array<double, 6> bounds() const;

  /// Display transform: additional visual translation applied to actors
  /// (separate from vtkImageData origin).
  std::array<double, 3> displayPosition() const;
  void setDisplayPosition(double x, double y, double z);

  /// Display transform: Euler angles applied to actors
  /// (vtkImageData has no rotation support).
  std::array<double, 3> displayOrientation() const;
  void setDisplayOrientation(double x, double y, double z);

  // -- Scalar data accessors --

  /// Get the active scalars array
  vtkDataArray* scalars() const;

  /// List the names of all point-data scalar arrays in the underlying
  /// vtkImageData, in their current order. Returns an empty list if the
  /// image data is null.
  QStringList scalarNames() const;

  /// Name of the currently active scalars array, or an empty string if
  /// there is no active array (or it has no name).
  QString activeScalarName() const;

  /// Rename a scalar array. No-op if @a oldName doesn't exist, @a newName
  /// is already taken, or the two names are equal. Preserves rename
  /// history so state files can round-trip user-applied renames.
  /// Keeps the array as the active scalars if it was active before.
  void renameScalarArray(const QString& oldName, const QString& newName);

  /// Return the name a scalar array had at load time. For arrays that
  /// have never been renamed this equals @a currentName. Returns the
  /// empty string if no array with @a currentName exists in our map.
  QString originalScalarName(const QString& currentName) const;

  /// Get the number of scalar components
  int numberOfComponents() const;

  /// Get the scalar range [min, max] of the active scalars
  std::array<double, 2> scalarRange() const;

  /// Value below which @a fraction (0 to 1) of the active scalars lie,
  /// estimated from a histogram; the magnitude for multi-component data.
  double scalarPercentile(double fraction) const;

  /// A starting lower threshold for showing this data: the 95th
  /// percentile of the voxels above the minimum value, so the padding
  /// at the minimum (usually zero) does not pull the estimate into the
  /// background noise, capped so that no more than about 250,000 voxels
  /// pass on a large volume. Either way the first render of a Threshold
  /// visualization stays quick.
  double thresholdSeed() const;

  /// The range the color map should span: scalarRange(), or the union of
  /// every step's range when this is a time series.
  ///
  /// Playback swaps in a different vtkImageData every frame. Scaling the
  /// color map to the current step's own range would renormalize each
  /// frame, so a feature that genuinely brightens over time would look
  /// constant and the steps could not be compared. The series range is
  /// what actually bounds the data, and it does not move with the step.
  std::array<double, 2> colorMapRange() const;

  // -- Color/Opacity map --

  /// Returns true if the color map has been initialized.
  bool hasColorMap() const;

  /// Create the SM proxy and cache client-side VTK objects. Must be called
  /// on the main thread. Safe to call multiple times (no-op if already done).
  void initColorMap();

  /// Get the color transfer function proxy. Calls initColorMap() if needed.
  vtkSMProxy* colorMap();

  /// Get the scalar opacity function sub-proxy from the color map.
  vtkSMProxy* opacityMap();

  /// Direct access to the cached client-side VTK objects.
  /// These are populated by initColorMap() and can be used for thread-safe
  /// copy/rescale operations without going through SM proxies.
  vtkColorTransferFunction* colorTransferFunction() const;
  vtkPiecewiseFunction* scalarOpacity() const;

  /// Get the gradient opacity function.
  vtkPiecewiseFunction* gradientOpacity() const;

  /// Rescale the color and opacity maps to the current colorMapRange().
  void rescaleColorMap();

  /// Copy color/opacity map control points from another VolumeData as-is
  /// (preserving the source's X positions).
  /// Both this and source must have initColorMap() called first.
  void copyColorMapFrom(const VolumeData& source);

  /// Push the client-side VTK color/opacity state into the SM proxy
  /// properties so that proxy-level operations see the current values.
  void syncColorMapToProxy();

  /// Convenience: copyColorMapFrom() + rescaleColorMap().
  void copyAndRescaleColorMapFrom(const VolumeData& source);

  // -- Metadata --

  /// A user-visible label
  QString label() const;
  void setLabel(const QString& label);

  /// Units string (e.g. "nm", "angstrom")
  QString units() const;
  void setUnits(const QString& units);

  // -- Tilt angle accessors --

  bool hasTiltAngles() const;
  QVector<double> tiltAngles() const;
  void setTiltAngles(const QVector<double>& angles);

  /// Static utility: check if a vtkImageData has tilt angles stored
  /// in its field data (array named "tilt_angles").
  static bool hasTiltAngles(vtkImageData* image);
  static QVector<double> getTiltAngles(vtkImageData* image);
  static void setTiltAngles(vtkImageData* image, const QVector<double>& angles);

  // -- Scan ID accessors --

  bool hasScanIds() const;
  QVector<int> scanIds() const;
  void setScanIds(const QVector<int>& ids);

  static bool hasScanIds(vtkImageData* image);
  static QVector<int> getScanIds(vtkImageData* image);
  static void setScanIds(vtkImageData* image, const QVector<int>& ids);
  static void clearScanIds(vtkImageData* image);

  // -- Subsample metadata (stored in field data) --

  static bool wasSubsampled(vtkImageData* image);
  static void setWasSubsampled(vtkImageData* image, bool b);
  static void subsampleStrides(vtkImageData* image, int strides[3]);
  static void setSubsampleStrides(vtkImageData* image, int strides[3]);
  static void subsampleVolumeBounds(vtkImageData* image, int bounds[6]);
  static void setSubsampleVolumeBounds(vtkImageData* image, int bounds[6]);

  // -- Data type metadata (persisted in EMD field data) --

  /// On-disk tomviz data-source type, stored as an int8 in the image's
  /// field data ("tomviz_data_source_type"). Distinct from the runtime
  /// PortType; this is the value EMD files carry for compatibility.
  enum class DataType : int
  {
    Volume = 0,
    TiltSeries = 1,
    Fib = 2
  };

  static void setType(vtkImageData* image, DataType type);

  // -- Time series support --

  struct TimeStep {
    QString label;
    vtkSmartPointer<vtkImageData> image;
    double time = 0.0;
  };

  void setTimeSteps(const QList<TimeStep>& steps);
  QList<TimeStep> timeSteps() const;
  bool hasTimeSteps() const;
  int currentTimeStepIndex() const;
  void switchTimeStep(int index);

  // -- Serialization --

  /// Emit a JSON object capturing the state a state file needs to
  /// round-trip this VolumeData: label, units, vtkImageData geometry
  /// (spacing, origin), display transform (position, orientation),
  /// color map, and gradient opacity. Does NOT serialize the underlying
  /// voxel data — that lives in the file format (EMD / HDF5 group).
  /// Virtual so subclasses can add their own state (LabelMapData adds
  /// the label table); OutputPort routes through the base pointer.
  virtual QJsonObject serialize() const;
  virtual bool deserialize(const QJsonObject& json);

private:
  vtkSmartPointer<vtkImageData> m_imageData;
  vtkSmartPointer<vtkSMProxy> m_colorMap;
  vtkNew<vtkPiecewiseFunction> m_gradientOpacity;
  // Cached client-side VTK objects from the SM proxy
  vtkColorTransferFunction* m_ctf = nullptr;
  vtkPiecewiseFunction* m_opacity = nullptr;
  QString m_label;
  QString m_units;
  std::array<double, 3> m_displayPosition = { 0.0, 0.0, 0.0 };
  std::array<double, 3> m_displayOrientation = { 0.0, 0.0, 0.0 };
  QList<TimeStep> m_timeSteps;
  int m_currentTimeStep = 0;
  /// Union of every time step's scalar range, recomputed by setTimeSteps.
  /// Only meaningful while m_timeSteps is non-empty.
  std::array<double, 2> m_timeSeriesRange = { 0.0, 0.0 };
  /// Maps the current display name of each scalar array to the name it
  /// had when the array was first added (usually the on-disk name from
  /// the file or the name assigned by a Python operator). renameScalarArray
  /// keeps this in sync; serialize emits it as {originalName: currentName}.
  QMap<QString, QString> m_currentToOriginal;
};

/// Convenience type for sharing VolumeData through ports via std::any
using VolumeDataPtr = std::shared_ptr<VolumeData>;

} // namespace pipeline
} // namespace tomviz

#endif
