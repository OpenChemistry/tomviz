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
  double angle,y,z,t,Q1,Q2,QDash;
  int rayIndex;
  for (int x = 0; x < xDim; ++x) //loop through slices (x-direction)
  {
    //2D Back Projection
    for (int ang = 0; ang < numOfTilts; ++ang) //loop through tilt angles
    {
      angle = tiltAngles[ang]*PI/180;
      for (int iy = 0; iy < outputSize[1]; ++iy) //loop through all pixels in reconstructed image (y-z plane)
      {
        for (int iz = 0; iz < outputSize[2]; ++iz)
        {
          //Calcualte y,z coord.
          y = iy + 0.5 - ((double)yDim)/2.0;
          z = iz + 0.5 - ((double)yDim)/2.0;
          //calculate ray coord.
          t = y * cos(angle) + z * sin(angle);
          
          if (t >= -yDim/2 && t <= yDim/2 )	//check if ray is inside projection
          {
            rayIndex = floor((t + yDim/2));	//determine index txi such that tx <= t <= tx+ddet
            if (rayIndex >= 0 && rayIndex <= yDim-2) //check that tx and tx+1 are within data range
            {
              //Linear interpolation
              Q1 = data[ang*xDim*yDim + rayIndex * xDim + x ];
              Q2 = data[ang*xDim*yDim + (rayIndex+1) * xDim + x ];
              QDash = Q1 + (t-double(rayIndex-yDim/2))*(Q2-Q1);
              reconPtr[iz*outputSize[0]*outputSize[1] + iy*outputSize[0] + x] += QDash;
            }
          }
        }
      }
    }
  }
  
  //Normalize
  double n = PI/double(2*numOfTilts);
  for (int xx = 0; xx < outputSize[0]; ++xx)
  {
    for (int yy = 0; yy < outputSize[1]; ++yy)
    {
      for (int zz = 0; zz < outputSize[2]; ++zz)
      {
        reconPtr[zz*outputSize[0]*outputSize[1] + yy*outputSize[0] + xx] *= n;
      }
    }
  }
  
  
}
  


  
}
