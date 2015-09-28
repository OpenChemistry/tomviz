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

#include "SelectVolumeWidget.h"
#include "EditOperatorWidget.h"
#include "vtkDataObject.h"
#include "vtkExtractVOI.h"
#include "vtkImageData.h"
#include "vtkNew.h"

#include <QPointer>
#include <QHBoxLayout>

namespace
{
class CropWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT
  typedef tomviz::EditOperatorWidget Superclass;
public:
  CropWidget(tomviz::CropOperator *source, QWidget* p)
    : Superclass(p), Op(source)
  {
    this->Widget = new tomviz::SelectVolumeWidget(
                         source->inputDataOrigin(),
                         source->inputDataSpacing(),
                         source->inputDataExtent(),
                         source->cropBounds(),
                         this);
    QHBoxLayout *hboxlayout = new QHBoxLayout;
    hboxlayout->addWidget(this->Widget);
    this->setLayout(hboxlayout);
  }
  ~CropWidget() {}

  virtual void applyChangesToOperator()
  {
    int bounds[6];
    this->Widget->getExtentOfSelection(bounds);
    if (this->Op)
    {
      this->Op->setCropBounds(bounds);
    }
  }
private:
  QPointer<tomviz::CropOperator> Op;
  tomviz::SelectVolumeWidget* Widget;
};
}

#include "CropOperator.moc"

namespace tomviz
{

CropOperator::CropOperator(const int *dataExtent, const double *dataOrigin,
                           const double *dataSpacing, QObject* p)
  : Superclass(p)
{
  // By default include the entire volume
  for (int i = 0; i < 6; ++i)
  {
    this->CropBounds[i] = dataExtent[i];
  }
  std::copy(dataExtent, dataExtent + 6, this->InputDataExtent);
  std::copy(dataOrigin, dataOrigin + 3, this->InputDataOrigin);
  std::copy(dataSpacing, dataSpacing + 3, this->InputDataSpacing);
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
  vtkImageData *imageData = vtkImageData::SafeDownCast(data);
  imageData->GetExtent(this->InputDataExtent);
  imageData->GetOrigin(this->InputDataOrigin);
  imageData->GetSpacing(this->InputDataSpacing);
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
  CropOperator *other = new CropOperator(this->InputDataExtent,
                                         this->InputDataOrigin,
                                         this->InputDataSpacing);
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
  return new CropWidget(this, p);
}

void CropOperator::inputDataExtent(int *extent)
{
  std::copy(this->InputDataExtent, this->InputDataExtent + 6, extent);
}

void CropOperator::inputDataOrigin(double *origin)
{
  std::copy(this->InputDataOrigin, this->InputDataOrigin + 3, origin);
}

void CropOperator::inputDataSpacing(double *spacing)
{
  std::copy(this->InputDataSpacing, this->InputDataSpacing + 3, spacing);
}

}
