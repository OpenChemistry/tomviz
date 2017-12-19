/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizDataSource_h
#define tomvizDataSource_h

#include <QObject>

#include <QScopedPointer>
#include <QVector>

#include <vtkSmartPointer.h>

#include <vtk_pugixml.h>

#include "PipelineWorker.h"

#include <functional>

class vtkSMProxy;
class vtkSMSourceProxy;
class vtkImageData;
class vtkDataObject;
class vtkPiecewiseFunction;
class vtkTransferFunction2DItem;

namespace tomviz {
class Operator;

/// Encapsulation for a DataSource. This class manages a data source, including
/// the provenance for any operations performed on the data source.
class DataSource : public QObject
{
  Q_OBJECT

public:
  class ImageFuture;

  /// The type of data in the data source.  The data types currently supported
  /// are volumetric data and image stacks representing tilt series.
  enum DataSourceType
  {
    Volume,
    TiltSeries
  };

  enum class PersistenceState
  {
    Transient, // Doesn't need to written to disk
    Saved,     // Written to disk
    Modified   // Needs to be written to disk
  };

  /// \c dataSource is the original reader that reads the data into the
  /// application.
  DataSource(vtkSMSourceProxy* dataSource, DataSourceType dataType = Volume,
             QObject* parent = nullptr,
             PersistenceState persistState = PersistenceState::Saved);
  ~DataSource() override;

  /// Returns the data producer proxy to insert in ParaView pipelines.
  /// This proxy instance doesn't change over the lifetime of a DataSource even
  /// if new DataOperators are added to the source.
  vtkSMSourceProxy* producer() const;

  /// Returns a list of operators added to the DataSource.
  const QList<Operator*>& operators() const;

  /// Add/remove operators.
  int addOperator(Operator* op);
  bool removeOperator(Operator* op);
  bool removeAllOperators();

  /// Creates a new clone from this DataSource. If cloneOperators then clone
  /// the operators too, if cloneTransformedOnly clone the transformed data.
  DataSource* clone(bool cloneOperators,
                    bool cloneTransformedOnly = false) const;

  /// Save the state out.
  bool serialize(pugi::xml_node& in) const;
  bool deserialize(const pugi::xml_node& ns);

  /// Returns the original data source. This is not meant to be used to connect
  /// visualization pipelines on directly. Use producer() instead.
  vtkSMSourceProxy* originalDataSource() const;

  /// Override the filename.
  void setFilename(const QString& filename);

  /// Returns the name of the filename used from the originalDataSource.
  QString filename() const;

  /// Returns the type of data in this DataSource
  DataSourceType type() const;

  /// Returns the color map for the DataSource.
  vtkSMProxy* colorMap() const;
  vtkSMProxy* opacityMap() const;
  vtkPiecewiseFunction* gradientOpacityMap() const;
  QVector<vtkSmartPointer<vtkTransferFunction2DItem>>& transferFunction2D()
    const;
  vtkImageData* transferFunction2DImage() const;

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
  QVector<double> getTiltAngles(bool useOriginalDataTiltAngles = false) const;

  /// Set the tilt angles to the values in the given QVector
  void setTiltAngles(const QVector<double>& angles);

  /// Moves the displayPosition of the DataSource by detlaPosition
  void translate(const double deltaPosition[3]);

  /// Gets the display position of the data source
  const double* displayPosition();

  /// Sets the display position of the data source
  void setDisplayPosition(const double newPosition[3]);

  ImageFuture* getCopyOfImagePriorTo(Operator* op);

  /// Returns the extent of the transformed dataset
  void getExtent(int extent[6]);
  /// Returns the physical extent (bounds) of the transformed dataset
  void getBounds(double bounds[6]);
  /// Returns the spacing of the transformed dataset
  void getSpacing(double spacing[3]) const;
  /// Sets the scale factor (ratio between units and spacing)
  /// one component per axis
  void setSpacing(const double scaleFactor[3]);

  /// Returns the number of components in the dataset.
  unsigned int getNumberOfComponents();

  /// Returns a string describing the units for the given axis of the data
  QString getUnits(int axis);
  /// Set the string describing the units
  void setUnits(const QString& units);

  /// Execute the operator pipeline associate with the datasource
  void executeOperators();

  /// Return true is datasource is an image stack, false otherwise
  bool isImageStack();

  /// Return true if an operator is running in this DataSource's worker
  bool isRunningAnOperator();

  // Pause the automatic exection of the operator pipeline
  void pausePipeline();

  // Resume the automatic execution of the operator pipeline, will execution the
  // existing pipeline. If execute is true the entire pipeline will be executed.
  void resumePipeline(bool execute = true);

  // Cancel execution of the operator pipeline. canceled is a optional callback
  // that will be called when the pipeline has been successfully canceled.
  void cancelPipeline(std::function<void()> canceled = nullptr);

  /// Set the persistence state
  void setPersistenceState(PersistenceState state);

  /// Returns the persistence state
  PersistenceState persistenceState() const;

signals:
  /// This signal is fired to notify the world that the DataSource may have
  /// new/updated data.
  void dataChanged();

  /// This signal is fired to notify the world that the data's properties may
  /// have changed.
  void dataPropertiesChanged();

  /// This signal is fired every time a new operator is added to this
  /// DataSource.
  void operatorAdded(Operator*);

  /// This signal is fired every time the display position is changed
  /// Any actors based on this DataSource's data should update the position
  /// on their actors to match this so the effect of setting the position is
  /// to translate the dataset.
  void displayPositionChanged(double newX, double newY, double newZ);

  /// This signal is fired when the return value from isRunningAnOperator
  /// becomes true
  void operatorStarted();
  /// This signal is fired when the return value from isRunningAnOperator
  /// becomes false
  void allOperatorsFinished();

public slots:
  void dataModified();

protected:
  void operate(Operator* op);

  /// Reset the data output of the trivial producer to original data object.
  void resetData();

  /// Set data output of trivial producer to new data object, the trivial
  /// producer takes over ownership of the data object.
  void setData(vtkDataObject* newData);

  /// Create copy of current data object, caller is responsible for ownership
  vtkDataObject* copyData();

  /// Create copy of original data object, caller is responsible for ownership
  vtkDataObject* copyOriginalData();

  /// Sets the type of data in the DataSource
  void setType(DataSourceType t);

protected slots:
  void operatorTransformModified();

  /// update the color map range.
  void updateColorMap();

  /// The pipeline worker is finished
  void pipelineFinished(bool result);

  /// The pipeline worker is has been canceled
  void pipelineCanceled();
  void updateCache();

private:
  Q_DISABLE_COPY(DataSource)

  class DSInternals;
  const QScopedPointer<DSInternals> Internals;
};

/// Return from getCopyOfImagePriorTo for caller to track async operation.
class DataSource::ImageFuture : public QObject
{
  Q_OBJECT

public:
  friend class DataSource;

  vtkSmartPointer<vtkImageData> result() { return m_imageData; }
  Operator* op() { return m_operator; }

signals:
  void finished(bool result);
  void canceled();

private:
  ImageFuture(Operator* op, vtkSmartPointer<vtkImageData> m_imageData,
              PipelineWorker::Future* future = nullptr,
              QObject* parent = nullptr);
  ~ImageFuture() override;
  Operator* m_operator;
  vtkSmartPointer<vtkImageData> m_imageData;
  PipelineWorker::Future* m_future;
};
}

#endif
