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
#ifndef tomvizCacheOperator_h
#define tomvizCacheOperator_h

#include "Operator.h"

namespace tomviz {
class DataSource;

class CacheOperator : public Operator
{
  Q_OBJECT

public:
  CacheOperator(DataSource* source, QObject* parent = nullptr);

  QString label() const override { return "Cache"; }

  QIcon icon() const override;

  Operator* clone() const override;

  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  bool hasCustomUI() const override { return false; }

  QWidget* getCustomProgressWidget(QWidget*) const override;

protected:
  bool applyTransform(vtkDataObject* data) override;

signals:
  // Signal used to request the creation of a new data source. Needed to
  // ensure the initialization of the new DataSource is performed on UI thread
  void newChildDataSource(const QString&, vtkSmartPointer<vtkDataObject>);

  void newOperatorResult(vtkSmartPointer<vtkDataObject>);

private slots:
  // Create a new child datasource and set it on this operator
  void createNewChildDataSource(const QString& label,
                                vtkSmartPointer<vtkDataObject>);

  void setOperatorResult(vtkSmartPointer<vtkDataObject> result);

private:
  DataSource* m_dataSource;
  int m_extent[6];
  bool m_updateCache = true; // Update the first time, then freeze.
  Q_DISABLE_COPY(CacheOperator)
};
}

#endif
