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
#ifndef tomvizCropOperator_h
#define tomvizCropOperator_h

#include "Operator.h"

namespace tomviz {

class CropOperator : public Operator
{
  Q_OBJECT

public:
  CropOperator(QObject* parent = nullptr);

  QString label() const override { return "Crop"; }

  QIcon icon() const override;

  Operator* clone() const override;

  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;
  bool hasCustomUI() const override { return true; }

  void setCropBounds(const int bounds[6]);
  const int* cropBounds() const { return m_bounds; }

protected:
  bool applyTransform(vtkDataObject* data) override;

private:
  int m_bounds[6];
  Q_DISABLE_COPY(CropOperator)
};
}

#endif
