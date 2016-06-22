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
#include "OperatorFactory.h"

#include "ConvertToFloatOperator.h"
#include "CropOperator.h"
#include "DataSource.h"
#include "OperatorPython.h"
#include "ReconstructionOperator.h"
#include "TranslateAlignOperator.h"

#include "vtkImageData.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

namespace tomviz
{

OperatorFactory::OperatorFactory()
{
}

OperatorFactory::~OperatorFactory()
{
}

QList<QString> OperatorFactory::operatorTypes()
{
  QList<QString> reply;
  reply << "Python" << "ConvertToFloat" << "Crop" << "CxxReconstruction" << "TranslateAlign";
  qSort(reply);
  return reply;
}

Operator* OperatorFactory::createOperator(const QString &type, DataSource *ds)
{
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    ds->producer()->GetClientSideObject());
  vtkImageData *image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  Operator* op = nullptr;
  if (type == "Python")
  {
    op = new OperatorPython();
  }
  else if (type == "ConvertToFloat")
  {
    op = new ConvertToFloatOperator();
  }
  else if (type == "Crop")
  {
    op = new CropOperator();
  }
  else if (type == "CxxReconstruction")
  {
    op = new ReconstructionOperator(ds);
  }
  else if (type == "TranslateAlign")
  {
    op = new TranslateAlignOperator(ds);
  }
  return op;
}

const char* OperatorFactory::operatorType(Operator* op)
{
  if (qobject_cast<OperatorPython*>(op))
  {
    return "Python";
  }
  if (qobject_cast<ConvertToFloatOperator*>(op))
  {
    return "ConvertToFloat";
  }
  if (qobject_cast<CropOperator*>(op))
  {
    return "Crop";
  }
  if (qobject_cast<ReconstructionOperator*>(op))
  {
    return "CxxReconstruction";
  }
  if (qobject_cast<TranslateAlignOperator*>(op))
  {
    return "TranslateAlign";
  }
  return nullptr;
}

}
