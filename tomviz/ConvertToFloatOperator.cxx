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

#include "ConvertToFloatOperator.h"

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

namespace {

template <typename T>
void convertToFloat(vtkFloatArray* fArray, int nComps, int nTuples, void* data)
{
  T* d = static_cast<T*>(data);
  float* a = static_cast<float*>(fArray->GetVoidPointer(0));
  for (int i = 0; i < nComps * nTuples; ++i) {
    a[i] = (float)d[i];
  }
}
}

namespace tomviz {

ConvertToFloatOperator::ConvertToFloatOperator(QObject* p) : Operator(p)
{
}

QIcon ConvertToFloatOperator::icon() const
{
  return QIcon();
}

bool ConvertToFloatOperator::applyTransform(vtkDataObject* data)
{
  vtkImageData* imageData = vtkImageData::SafeDownCast(data);
  // sanity check
  if (!imageData) {
    return false;
  }
  vtkDataArray* scalars = imageData->GetPointData()->GetScalars();
  vtkNew<vtkFloatArray> floatArray;
  floatArray->SetNumberOfComponents(scalars->GetNumberOfComponents());
  floatArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
  floatArray->SetName(scalars->GetName());
  switch (scalars->GetDataType()) {
    vtkTemplateMacro(convertToFloat<VTK_TT>(
      floatArray.Get(), scalars->GetNumberOfComponents(),
      scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0)));
  }
  imageData->GetPointData()->RemoveArray(scalars->GetName());
  imageData->GetPointData()->SetScalars(floatArray.Get());
  return true;
}

Operator* ConvertToFloatOperator::clone() const
{
  return new ConvertToFloatOperator();
}

}
