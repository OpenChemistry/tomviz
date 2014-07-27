/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __DataSource_h
#define __DataSource_h

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <vtk_pugixml.h>

class vtkSMProxy;
class vtkSMSourceProxy;

namespace TEM
{
class Operator;

/// Encapsulation for a DataSource. This class manages a data source, including
/// the provenance for any operations performed on the data source.
class DataSource : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  /// \c dataSource is the original reader that reads the data into the
  /// application.
  DataSource(vtkSMSourceProxy* dataSource, QObject* parent=NULL);
  virtual ~DataSource();

  /// Returns the data producer proxy to insert in ParaView pipelines.
  /// This proxy instance doesn't change over the lifetime of a DataSource even
  /// if new DataOperators are added to the source.
  vtkSMSourceProxy* producer() const;

  /// Returns a list of operators added to the DataSource.
  const QList<QSharedPointer<Operator> >& operators() const;

  /// Add/remove operators.
  int addOperator(const QSharedPointer<Operator>& op);

  /// Creates a new clone from this DataSource.
  DataSource* clone(bool clone_operators) const;

  /// Save the state out.
  bool serialize(pugi::xml_node& in) const;
  bool deserialize(const pugi::xml_node& ns);

  /// Returns the original data source. This is not meant to be used to connect
  /// visualization pipelines on directly. Use producer() instead.
  vtkSMSourceProxy* originalDataSource() const;

  /// Returns the name of the filename used from the originalDataSource.
  QString filename() const;

  /// Returns the color map for the DataSource.
  vtkSMProxy* colorMap();

signals:
  /// This signal is fired to notify the world that the DataSource may have
  /// new/updated data.
  void dataChanged();

  /// This signal is fired every time a new operator is added to this
  /// DataSource.
  void operatorAdded(Operator*);

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
