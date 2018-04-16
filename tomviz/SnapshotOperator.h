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
#ifndef tomvizSnapshotOperator_h
#define tomvizSnapshotOperator_h

#include "Operator.h"

namespace tomviz {
class DataSource;

class SnapshotOperator : public Operator
{
  Q_OBJECT

public:
  SnapshotOperator(DataSource* source, QObject* parent = nullptr);

  QString label() const override { return "Snapshot"; }

  QIcon icon() const override;

  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QWidget* getCustomProgressWidget(QWidget*) const override;

protected:
  bool applyTransform(vtkDataObject* data) override;

signals:
  // Signal used to request the creation of a new data source. Needed to
  // ensure the initialization of the new DataSource is performed on UI thread
  void newChildDataSource(const QString&, vtkSmartPointer<vtkDataObject>);

private slots:
  // Create a new child datasource and set it on this operator
  void createNewChildDataSource(const QString& label,
                                vtkSmartPointer<vtkDataObject>);

private:
  DataSource* m_dataSource;
  bool m_updateCache = true; // Update the first time, then freeze.
  Q_DISABLE_COPY(SnapshotOperator)
};
}

#endif
