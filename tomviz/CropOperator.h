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
  CropOperator(QObject* parent=NULL);
  virtual ~CropOperator();

  virtual QString label() const { return "Crop"; }

  virtual QIcon icon() const;

  virtual bool transform(vtkDataObject* data);

  virtual Operator* clone() const;

  virtual bool serialize(pugi::xml_node& ns) const;
  virtual bool deserialize(const pugi::xml_node& ns);

  void setCropBounds(const int bounds[6]);
  const int* cropBounds() const
    { return this->CropBounds; }

private:
  int CropBounds[6];
  Q_DISABLE_COPY(CropOperator)
};

}

#endif
