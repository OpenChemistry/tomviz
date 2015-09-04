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

#include "CropOperator.h"

#include "vtkDataObject.h"
#include "vtkExtractVOI.h"
#include "vtkNew.h"

namespace tomviz
{

CropOperator::CropOperator(QObject* p)
  : Superclass(p)
{
  for (int i = 0; i < 6; ++i)
  {
    this->CropBounds[i] = 0;
  }
}

CropOperator::~CropOperator()
{
}

QIcon CropOperator::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqExtractGrid24.png");
}

bool CropOperator::transform(vtkDataObject* data)
{
  vtkNew<vtkExtractVOI> extractor;
  extractor->SetVOI(this->CropBounds);
  extractor->SetInputDataObject(data);
  extractor->Update();
  extractor->UpdateWholeExtent();
  data->ShallowCopy(extractor->GetOutputDataObject(0));
  return true;
}

Operator* CropOperator::clone() const
{
  CropOperator *other = new CropOperator();
  other->setCropBounds(this->CropBounds);
  return other;
}

bool CropOperator::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("boundsXmin").set_value(this->CropBounds[0]);
  ns.append_attribute("boundsXmax").set_value(this->CropBounds[1]);
  ns.append_attribute("boundsYmin").set_value(this->CropBounds[2]);
  ns.append_attribute("boundsYmax").set_value(this->CropBounds[3]);
  ns.append_attribute("boundsZmin").set_value(this->CropBounds[4]);
  ns.append_attribute("boundsZmax").set_value(this->CropBounds[5]);
  return true;
}

bool CropOperator::deserialize(const pugi::xml_node& ns)
{
  int bounds[6];
  bounds[0] = ns.attribute("boundsXmin").as_int();
  bounds[1] = ns.attribute("boundsXmax").as_int();
  bounds[2] = ns.attribute("boundsYmin").as_int();
  bounds[3] = ns.attribute("boundsYmax").as_int();
  bounds[4] = ns.attribute("boundsZmin").as_int();
  bounds[5] = ns.attribute("boundsZmax").as_int();
  this->setCropBounds(bounds);
  return true;
}

void CropOperator::setCropBounds(const int bounds[6])
{
  for (int i = 0; i < 6; ++i)
  {
    this->CropBounds[i] = bounds[i];
  }
  emit this->transformModified();
}

EditOperatorWidget *CropOperator::getEditorContents(QWidget *p)
{
  return NULL; // TODO - fixme
}

}
