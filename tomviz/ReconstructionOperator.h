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

namespace tomviz
{
class DataSource;

class ReconstructionOperator : public Operator
{
  Q_OBJECT
  typedef Operator Superclass;

public:
  ReconstructionOperator(DataSource *source, QObject* parent=nullptr);
  virtual ~ReconstructionOperator();

  QString label() const override { return "Reconstruction"; }

  QIcon icon() const override;

  Operator* clone() const override;

  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  bool hasCustomUI() const override { return false; }

  QWidget *getCustomProgressWidget(QWidget*) const override;
  int totalProgressSteps() const override;

  void cancelTransform() override;
protected:
  bool applyTransform(vtkDataObject* data) override;

signals:
  // emitted after each slice is reconstructed, use to display intermediate results
  // the first vector contains the sinogram reconstructed, the second contains the
  // slice of the resulting image.
  void intermediateResults(std::vector<float> resultSlice);

private:
  DataSource *dataSource;
  int extent[6];
  bool canceled;
  Q_DISABLE_COPY(ReconstructionOperator)
};

}

#endif

