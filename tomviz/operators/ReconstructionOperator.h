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
#ifndef tomvizReconstructionOperator_h
#define tomvizReconstructionOperator_h

#include "Operator.h"

namespace tomviz {
class DataSource;

class ReconstructionOperator : public Operator
{
  Q_OBJECT

public:
  ReconstructionOperator(DataSource* source, QObject* parent = nullptr);

  QString label() const override { return "Reconstruction"; }

  QIcon icon() const override;

  Operator* clone() const override;

  QWidget* getCustomProgressWidget(QWidget*) const override;

protected:
  bool applyTransform(vtkDataObject* data) override;

signals:
  /// Emitted after each slice is reconstructed, use to display intermediate
  /// results the first vector contains the sinogram reconstructed, the second
  /// contains the slice of the resulting image.
  void intermediateResults(std::vector<float> resultSlice);

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
  Q_DISABLE_COPY(ReconstructionOperator)
};
}

#endif
