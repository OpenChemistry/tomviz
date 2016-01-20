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
#include <QSharedPointer>
#include <QVector>
#include <vtk_pugixml.h>

class vtkSMProxy;
class vtkSMSourceProxy;


namespace tomviz
{
class Operator;

/// Encapsulation for a DataSource. This class manages a data source, including
/// the provenance for any operations performed on the data source.
class DataSource : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  /// The type of data in the data source.  The data types currently supported
  /// are volumetric data and image stacks representing tilt series.
  enum DataSourceType { Volume, TiltSeries };
  /// \c dataSource is the original reader that reads the data into the
  /// application.
  DataSource(vtkSMSourceProxy* dataSource, DataSourceType dataType = Volume,
             QObject* parent=nullptr);
  virtual ~DataSource();

  /// Returns the data producer proxy to insert in ParaView pipelines.
  /// This proxy instance doesn't change over the lifetime of a DataSource even
  /// if new DataOperators are added to the source.
  vtkSMSourceProxy* producer() const;

  /// Returns a list of operators added to the DataSource.
  const QList<QSharedPointer<Operator> >& operators() const;

  /// Add/remove operators.
  int addOperator(QSharedPointer<Operator>& op);
  bool removeOperator(QSharedPointer<Operator>& op);

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

  /// Returns the name of the filename used from the originalDataSource.
  QString filename() const;

  /// Returns the type of data in this DataSource
  DataSourceType type() const;
  /// Sets the type of data in the DataSource
  void setType(DataSourceType t);

  /// Returns the color map for the DataSource.
  vtkSMProxy* colorMap() const;
  vtkSMProxy* opacityMap() const;

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
  void setTiltAngles(const QVector<double> &angles);

  /// Moves the displayPosition of the DataSource by detlaPosition
  void translate(const double deltaPosition[3]);

  /// Gets the display position of the data source
  const double *displayPosition();

  /// Sets the display position of the data source
  void setDisplayPosition(const double newPosition[3]);

signals:
  /// This signal is fired to notify the world that the DataSource may have
  /// new/updated data.
  void dataChanged();

  /// This signal is fired every time a new operator is added to this
  /// DataSource.
  void operatorAdded(Operator*);
  void operatorAdded(QSharedPointer<Operator>&);

  /// This signal is fired every time the display position is changed
  /// Any actors based on this DataSource's data should update the position
  /// on their actors to match this so the effect of setting the position is
  /// to translate the dataset.
  void displayPositionChanged(double newX, double newY, double newZ);

public slots:
  void dataModified();

protected:
  void operate(Operator* op);
  void resetData();

protected slots:
  void operatorTransformModified();

  /// update the color map range.
  void updateColorMap();

private:
  Q_DISABLE_COPY(DataSource)

  class DSInternals;
  const QScopedPointer<DSInternals> Internals;
};

}

#endif
