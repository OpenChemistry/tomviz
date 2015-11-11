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
#include <math.h>
#include "TomographyReconstruction.h"
#include "vtkImageData.h"
#include "vtkFieldData.h"
#include "vtkDataArray.h"
#define PI 3.14159265359
#include "vtkPointData.h"
#include "vtkFloatArray.h"
#include "vtkSmartPointer.h"

#include <QDebug>

namespace tomviz
{

// conversion code
template<typename T>
vtkSmartPointer<vtkFloatArray> convertToFloatT(T *data, int len)
{
  vtkSmartPointer<vtkFloatArray> array = vtkSmartPointer<vtkFloatArray>::New();
  array->SetNumberOfTuples(len);
  float *f = static_cast<float*>(array->GetVoidPointer(0));
  for (int i = 0; i < len; ++i)
  {
    f[i] = (float) data[i];
  }
  return array;
}
  
vtkSmartPointer<vtkFloatArray> convertToFloat(vtkImageData* image)
{
  vtkDataArray *scalars = image->GetPointData()->GetScalars();
  int len = scalars->GetNumberOfTuples();
  vtkSmartPointer<vtkFloatArray> array;
  switch(scalars->GetDataType())
  {
    vtkTemplateMacro(array = convertToFloatT(static_cast<VTK_TT*>(scalars->GetVoidPointer(0)), len););
  }
  return array;
}

namespace tomviz
{
namespace TomographyTiltSeries
{

//Extract sinograms from tilt series
void tiltSeriesToSinogram(vtkImageData *tiltSeries, int sliceNumber,float* sinogram)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays
  int zDim = extents[5] - extents[4] + 1; //number of tilts
  
  //Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(0)); //Get pointer to tilt series (of type float)
  
  //Extract sinograms from tilt series. Make a deep copy
  for (int t = 0; t < zDim; ++t) //loop through tilts (z-direction)
    for (int r = 0; r < yDim; ++r) //loop through rays (y-direction)
        {
          sinogram[t * yDim + r] = dataPtr[t * xDim * yDim + r * xDim + sliceNumber];
        }
}


//Extract sinograms from tilt series
void getSinogram(vtkImageData *tiltSeries, int sliceNumber,float* sinogram, double axisPosition)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays
  int zDim = extents[5] - extents[4] + 1; //number of tilts
    
  //Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(0)); //Get pointer to tilt series (of type float)
    
  //Extract sinograms from tilt series. Make a deep copy
  for (int t = 0; t < zDim; ++t) //loop through tilts (z-direction)
    for (int r = 0; r < yDim; ++r) //loop through rays (y-direction)
    {
      sinogram[t * yDim + r] = dataPtr[t * xDim * yDim + r * xDim + sliceNumber];
    }
  }
  

  
}
