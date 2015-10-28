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
TomographyReconstruction::TomographyReconstruction()
{

}

TomographyReconstruction::~TomographyReconstruction()
{

}

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
  
void TomographyReconstruction::reconWBP(vtkImageData *tiltSeries,vtkImageData *recon)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1;
  int yDim = extents[3] - extents[2] + 1;
  int zDim = extents[5] - extents[4] + 1;
  //Convert input data type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *data = static_cast<float*>(dataAsFloats->GetVoidPointer(0));
  
  //Get tilt angles
  vtkDataArray *tiltAnglesArray = tiltSeries->GetFieldData()->GetArray("tilt_angles");
  int numOfTilts = tiltAnglesArray->GetNumberOfTuples();
  double *tiltAngles = static_cast<double*>(tiltAnglesArray->GetVoidPointer(0));
  
  // Creating the output volume and getting a pointer to it
  int outputSize[3] = {xDim, yDim, yDim};
  recon->SetExtent(0, outputSize[0] - 1, 0, outputSize[1] - 1, 0, outputSize[2] - 1);
  recon->AllocateScalars(VTK_FLOAT, 1); // 1 is for one component (i.e. not vector)
  float *reconPtr = static_cast<float*>(recon->GetScalarPointer());
  
  
  //Reconstruction
  for (int x = 0; x < xDim; ++x) //loop through slices (x-direction)
  {
    //2D Back Projection
    for (int ang = 0; ang < numOfTilts; ++ang) //loop through tilt angles
    {
      double angle = tiltAngles[ang]*PI/180;
      for (int iy = 0; iy < outputSize[1]; ++iy) //loop through all pixels in reconstructed image (y-z plane)
      {
        for (int iz = 0; iz < outputSize[2]; ++iz)
        {
          //Calcualte y,z coord.
          double y = iy + 0.5 - ((double)yDim)/2.0;
          double z = iz + 0.5 - ((double)yDim)/2.0;
          //calculate ray coord.
          double t = y * cos(angle) + z * sin(angle);
          
          if (t >= -yDim/2 && t <= yDim/2 )	//check if ray is inside projection
          {
            int rayIndex = floor((t + yDim/2));
            if (rayIndex >= 0 && rayIndex <= yDim-2)
            {
              //Linear interpolation
              double Q1 = data[ang*xDim*yDim + rayIndex * xDim + x ];
              double Q2 = data[ang*xDim*yDim + (rayIndex+1) * xDim + x ];
              double QDash = Q1 + (t-double(rayIndex-yDim/2))*(Q2-Q1);
              reconPtr[iz*outputSize[0]*outputSize[1] + iy*outputSize[0] + x] += QDash;
            }
          }
        }
      }
    }
  }
  
  //Normalize
  double normalizationFactor = PI/double(2*numOfTilts);
  for (int z = 0; z < outputSize[2]; ++z)
    for (int y = 0; y < outputSize[1]; ++y)
      for (int x = 0; x < outputSize[0]; ++x)
      {
        reconPtr[z*outputSize[0]*outputSize[1] + y*outputSize[0] + x] *= normalizationFactor;
      }
  
}
  


  
}
