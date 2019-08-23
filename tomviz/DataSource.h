/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataSource_h
#define tomvizDataSource_h

#include <QObject>

#include <QJsonObject>
#include <QScopedPointer>
#include <QVariantMap>
#include <QVector>

#include <vtkRect.h>

class vtkSMProxy;
class vtkSMSourceProxy;
class vtkImageData;
class vtkDataArray;
class vtkDataObject;
class vtkPiecewiseFunction;
class vtkAlgorithm;
class vtkTrivialProducer;

namespace tomviz {
class Operator;
class Pipeline;

/// Encapsulation for a DataSource. This class manages a data source, including
/// the provenance for any operations performed on the data source.
class DataSource : public QObject
{
  Q_OBJECT

public:
  /// The type of data in the data source.  The data types currently supported
  /// are volumetric data and image stacks representing tilt series.
  enum DataSourceType
  {
    Volume,
    TiltSeries,
    FIB
  };

  enum class PersistenceState
  {
    Transient, // Doesn't need to be written to disk
    Saved,     // Written to disk
    Modified   // Needs to be written to disk
  };

  /// Deprecated constructor, prefer directly setting data.
  DataSource(vtkSMSourceProxy* dataSource, DataSourceType dataType = Volume);

  /// \c dataSource is the original reader that reads the data into the
  /// application.
  DataSource(vtkImageData* dataSource, DataSourceType dataType = Volume,
             QObject* parent = nullptr,
             PersistenceState persistState = PersistenceState::Saved);

  /// Create a new dataSource not associated with a source proxy
  DataSource(const QString& label = QString(), DataSourceType dataType = Volume,
             QObject* parent = nullptr,
             PersistenceState persistState = PersistenceState::Saved,
             const QJsonObject& sourceInfo = QJsonObject());

  ~DataSource() override;

  /// Append a slice to the data source, this must be of the same x and y
  /// dimension as the existing slices in order to be appended.
  bool appendSlice(vtkImageData* slice);

  /// Returns the proxy that can be inserted in ParaView pipelines.
  /// This proxy instance doesn't change over the lifetime of a DataSource even
  /// if new DataOperators are added to the source.
  vtkSMSourceProxy* proxy() const;

  //// Returns the trivial producer to insert in VTK pipelines.
  vtkTrivialProducer* producer() const;

  /// Returns the output data object associated with the proxy.
  vtkDataObject* dataObject() const;

  /// Returns a list of operators added to the DataSource.
  const QList<Operator*>& operators() const;

  /// Add/remove operators.
  int addOperator(Operator* op);
  bool removeOperator(Operator* op);
  bool removeAllOperators();

  /// Creates a new clone from this DataSource.
  DataSource* clone() const;

  /// Save the state out.
  QJsonObject serialize() const;
  bool deserialize(const QJsonObject& state);

  /// Set the file name.
  void setFileName(const QString& fileName);

  /// Returns the name of the file used to load the data source.
  QString fileName() const;

  /// Set the ordered list of file names if loading from a stack of images.
  void setFileNames(const QStringList fileNames);

  /// Returns the list of files used to load the volume (if a stack was used).
  QStringList fileNames() const;

  /// Return true is data source is an image stack, false otherwise.
  bool isImageStack() const;

  /// Set the PV reader properties.
  void setReaderProperties(const QVariantMap& properties);

  /// Get the PV reader properties.
  QVariantMap readerProperties() const;

  /// Check to see if the data was subsampled while reading
  bool wasSubsampled() const;

  /// Set whether the data was subsampled while reading
  void setWasSubsampled(bool b);

  /// Get the stride used to generate the subsample
  int subsampleStride() const;

  /// Set the stride used to generate the subsample
  void setSubsampleStride(int i);

  /// Get the volume bounds used to generate the subsample
  void subsampleVolumeBounds(int bs[6]) const;

  /// Set the volume bounds used to generate the subsample
  void setSubsampleVolumeBounds(int bs[6]);

  /// Can we reload and resample the original dataset?
  bool canReloadAndResample() const;

  // Reload and resample the original dataset
  bool reloadAndResample();

  /// Set the label for the data source.
  void setLabel(const QString& label);

  /// Returns the name of the filename used from the originalDataSource.
  QString label() const;

  /// Returns the type of data in this DataSource
  DataSourceType type() const;
  /// Sets the type of data in the DataSource
  void setType(DataSourceType t);

  /// Returns the color map for the DataSource.
  vtkSMProxy* colorMap() const;
  vtkSMProxy* opacityMap() const;
  vtkPiecewiseFunction* gradientOpacityMap() const;
  vtkImageData* transferFunction2D() const;
  vtkRectd* transferFunction2DBox() const;

  /// Indicates whether the DataSource has a label map of the voxels.
  bool hasLabelMap();

  /// Crop the data to the given volume
  void crop(int bounds[6]);

  /// Returns true if the dataset already has a tilt angles array
  /// --- this CAN return true even if the dataset is currently a
  ///     volume.  This is to tell if switching the dataset to a tilt
  ///     series also needs to set the tilt angles.
  bool hasTiltAngles();

  /// Get a copy of the current tilt angles
  QVector<double> getTiltAngles() const;

  /// Set the tilt angles to the values in the given QVector
  void setTiltAngles(const QVector<double>& angles);

  /// Moves the displayPosition of the DataSource by deltaPosition
  void translate(const double deltaPosition[3]);

  /// Gets the display position of the data source
  const double* displayPosition();

  /// Sets the display position of the data source
  void setDisplayPosition(const double newPosition[3]);

  /// Returns the extent of the transformed dataset
  void getExtent(int extent[6]);
  /// Returns the physical extent (bounds) of the transformed dataset
  void getBounds(double bounds[6]);
  /// Returns the range of the transformed dataset
  void getRange(double range[2]);
  /// Returns the spacing of the transformed dataset
  void getSpacing(double spacing[3]) const;
  /// Sets the scale factor (ratio between units and spacing)
  /// one component per axis
  void setSpacing(const double scaleFactor[3], bool markModified = true);

  /// Set the active scalars by array name.
  void setActiveScalars(const QString& arrayName);
  QString activeScalars() const;
  /// Set the active scalars by component index.
  void setActiveScalars(int arrayIdx);
  int activeScalarsIdx() const;
  /// Get the scalars name for a given index.
  QString scalarsName(int arrayIdx) const;

  /// Get the scalars list
  QStringList listScalars() const;

  // Get pointer to scalar array
  vtkDataArray* getScalarsArray(const QString& arrayName);

  /// Returns the number of components in the dataset.
  unsigned int getNumberOfComponents();

  /// Returns a string describing the units for the data
  QString getUnits();
  /// Set the string describing the units
  void setUnits(const QString& units, bool markModified = true);

  /// Set the persistence state
  void setPersistenceState(PersistenceState state);

  /// Returns the persistence state
  PersistenceState persistenceState() const;

  Pipeline* pipeline() const;

  /// Create copy of current data object, caller is responsible for ownership
  vtkDataObject* copyData();

  /// Set data output of trivial producer to new data object, the trivial
  /// producer takes over ownership of the data object.
  void setData(vtkDataObject* newData);

  /// Copy data from a data object to the existing data.
  void copyData(vtkDataObject* newData);

  bool unitsModified();

  // Returns true if the data source is not associated with a file, false
  // otherwise.
  bool isTransient() const;

  /// Returns true if child operators can be added to this data source, false
  // otherwise.
  bool forkable();
  void setForkable(bool forkable);

  static bool hasTiltAngles(vtkDataObject* image);
  static QVector<double> getTiltAngles(vtkDataObject* image);
  static void setTiltAngles(vtkDataObject* image,
                            const QVector<double>& angles);

  /// Check to see if the data was subsampled while reading
  static bool wasSubsampled(vtkDataObject* image);

  /// Set whether the data was subsampled while reading
  static void setWasSubsampled(vtkDataObject* image, bool b);

  /// Get the stride used to generate the subsample
  static int subsampleStride(vtkDataObject* image);

  /// Set the stride used to generate the subsample
  static void setSubsampleStride(vtkDataObject* image, int i);

  /// Get the volume bounds used to generate the subsample
  static void subsampleVolumeBounds(vtkDataObject* image, int bs[6]);

  /// Set the volume bounds used to generate the subsample
  static void setSubsampleVolumeBounds(vtkDataObject* image, int bs[6]);

signals:
  /// This signal is fired to notify the world that the DataSource may have
  /// new/updated data.
  void dataChanged();

  /// This signal is fired to notify the world that the data's properties may
  /// have changed.
  void dataPropertiesChanged();

  /// Fired when active scalars change
  void activeScalarsChanged();

  /// This signal is fired every time a new operator is added to this
  /// DataSource.
  void operatorAdded(Operator*);

  void operatorRemoved(Operator*);

  /// This signal is fired every time the display position is changed
  /// Any actors based on this DataSource's data should update the position
  /// on their actors to match this so the effect of setting the position is
  /// to translate the dataset.
  void displayPositionChanged(double newX, double newY, double newZ);

public slots:
  void dataModified();
  void renameScalarsArray(const QString& oldName, const QString& newName);

protected slots:
  /// update the color map range.
  void updateColorMap();

private:
  /// Private method to initialize the data source.
  void init(vtkImageData* dataSource, DataSourceType dataType,
            PersistenceState persistState);

  vtkAlgorithm* algorithm() const;

  Q_DISABLE_COPY(DataSource)

  class DSInternals;
  const QScopedPointer<DSInternals> Internals;

  QJsonObject m_json;
};

inline bool DataSource::wasSubsampled() const
{
  return wasSubsampled(dataObject());
}

inline void DataSource::setWasSubsampled(bool b)
{
  setWasSubsampled(dataObject(), b);
}

inline int DataSource::subsampleStride() const
{
  return subsampleStride(dataObject());
}

inline void DataSource::setSubsampleStride(int i)
{
  setSubsampleStride(dataObject(), i);
}

inline void DataSource::subsampleVolumeBounds(int bs[6]) const
{
  subsampleVolumeBounds(dataObject(), bs);
}

inline void DataSource::setSubsampleVolumeBounds(int bs[6])
{
  setSubsampleVolumeBounds(dataObject(), bs);
}

} // namespace tomviz

#endif
