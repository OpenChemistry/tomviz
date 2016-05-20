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

namespace tomviz
{

class CropOperator : public Operator
{
  Q_OBJECT
  typedef Operator Superclass;

public:
  CropOperator(const int *dataExtent, const double *dataOrigin,
               const double *dataSpacing, QObject* parent=nullptr);
  virtual ~CropOperator();

  QString label() const override { return "Crop"; }

  QIcon icon() const override;

  Operator* clone() const override;

  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  EditOperatorWidget *getEditorContents(QWidget* parent) override;
  bool hasCustomUI() const override { return true; }

  void setCropBounds(const int bounds[6]);
  const int* cropBounds() const
    { return this->CropBounds; }

  // Used for the editor dialog
  void inputDataExtent(int *extent);
  const int *inputDataExtent() const { return this->InputDataExtent; }
  void inputDataOrigin(double *origin);
  const double *inputDataOrigin() const { return this->InputDataOrigin; }
  void inputDataSpacing(double *spacing);
  const double *inputDataSpacing() const { return this->InputDataSpacing; }

protected:
  bool applyTransform(vtkDataObject* data) override;

private:
  int CropBounds[6];
  int InputDataExtent[6];
  double InputDataOrigin[3];
  double InputDataSpacing[3];
  Q_DISABLE_COPY(CropOperator)
};

}

#endif
