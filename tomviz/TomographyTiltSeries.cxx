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
#include "TomographyTiltSeries.h"
#include "vtkImageData.h"
#include "vtkFieldData.h"
#include "vtkDataArray.h"
#define PI 3.14159265359
#include "vtkPointData.h"
#include "vtkFloatArray.h"
#include "vtkSmartPointer.h"

#include <QDebug>
//using namespace std;
namespace
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


void getSinogram(vtkImageData *tiltSeries, int sliceNumber,float* sinogram)
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
void getSinogram(vtkImageData *tiltSeries, int sliceNumber, float* sinogram, int Nray, double axisPosition = 0)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays in tilt series
  int zDim = extents[5] - extents[4] + 1; //number of tilts
    
  //Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(0)); //Get pointer to tilt series (of type float)
 
  double rayWidth = (double)yDim/(double)Nray;
  std::vector<float> weight1(Nray); //store weights for linear interpolation
  std::vector<float> weight2(Nray); //store weights for linear interpolation
  std::vector<int> index1(Nray); //store indices for linear interpolation
  std::vector<int> index2(Nray); //store indices for linear interpolation

  for (int r = 0; r < Nray; ++r) //loop through rays (y-direction)
  {
    double rayCoord = (double)(r-Nray/2)*rayWidth + axisPosition;
    index1[r] = floor(rayCoord)+ yDim/2;
    index2[r] = index2[r] + 1;
    weight1[r] = fabs(rayCoord - floor(rayCoord));
    weight2[r] = 1 - weight1[r] ;
  }

  double value1,value2;
  //Extract sinograms from tilt series. Make a deep copy
  for (int t = 0; t < zDim; ++t) //loop through tilts (z-direction)
    for (int r = 0; r < Nray; ++r) //loop through rays (y-direction)
    {
      if (index1[r]>=0 and index1[r]<yDim)
        value1 = dataPtr[t * xDim * yDim + index1[r] * xDim + sliceNumber];
      else
        value1 = 0;
      if (index2[r]>=0 and index2[r]<yDim)
        value2 = dataPtr[t * xDim * yDim + index2[r] * xDim + sliceNumber];
      else
        value2 = 0;
      sinogram[t * Nray + r] = value1*weight1[r] + value2*weight2[r];
        
    }
  }
  
} // end of getSinogram
  


//Extract sinograms from tilt series
void getSinogram(vtkImageData *tiltSeries, int sliceNumber, float* sinogram, int Nray, double axisPosition = 0)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays in tilt series
  int zDim = extents[5] - extents[4] + 1; //number of tilts
    
  //Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(0)); //Get pointer to tilt series (of type float)
    
  double rayWidth = (double)yDim/(double)Nray;
  std::vector<float> weight1(Nray); //store weights for linear interpolation
  std::vector<float> weight2(Nray); //store weights for linear interpolation
  std::vector<int> index1(Nray); //store indices for linear interpolation
  std::vector<int> index2(Nray); //store indices for linear interpolation
    
  for (int r = 0; r < Nray; ++r) //loop through rays (y-direction)
  {
    double rayCoord = (double)(r-Nray/2)*rayWidth + axisPosition;
    index1[r] = floor(rayCoord)+ yDim/2;
    index2[r] = index2[r] + 1;
    weight1[r] = fabs(rayCoord - floor(rayCoord));
    weight2[r] = 1 - weight1[r] ;
  }
    
  //Extract sinograms from tilt series. Make a deep copy
  for (int z = 0; z < zDim; ++z) //loop through tilts (z-direction)
    for (int r = 0; r < Nray; ++r) //loop through rays (y-direction)
    {
      sinogram[z * Nray + r] = 0;
      if (index1[r]>=0 and index1[r]<yDim)
        sinogram[z * Nray + r] += dataPtr[z * xDim * yDim + index1[r] * xDim + sliceNumber] * weight1[r];
      if (index2[r]>=0 and index2[r]<yDim)
        sinogram[z * Nray + r] += dataPtr[z * xDim * yDim + index2[r] * xDim + sliceNumber] * weight2[r];
    }
  
} // end of getSinogram

  
  
  
void getSinogram(vtkImageData *tiltSeries, int sliceNumber, float* sinogram,  int Nray, double axisPosition = 0, double axisAngle = 0)
{
  if (axisAngle==0)
    getSinogram(tiltSeries, sliceNumber, sinogram,   Nray, axisPosition );
  else
  {
    
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays in tilt series
  int zDim = extents[5] - extents[4] + 1; //number of tilts
    
  //Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(0)); //Get pointer to tilt series (of type float)
    
  double rayWidth = (double)yDim/(double)Nray;
  std::vector<float> weight1(Nray); //store weights for linear interpolation
  std::vector<float> weight2(Nray); //store weights for linear interpolation
  std::vector<float> weight3(Nray); //store weights for linear interpolation
  std::vector<float> weight4(Nray); //store weights for linear interpolation
    
  std::vector<int> index1(Nray); //store indices for linear interpolation
  std::vector<int> index2(Nray); //store indices for linear interpolation
  std::vector<int> index3(Nray); //store indices for linear interpolation
  std::vector<int> index4(Nray); //store indices for linear interpolation
    
  for (int r = 0; r < Nray; ++r) //loop through rays (y-direction)
  {
    double rayCoord_y = ((double)(r-Nray/2)*rayWidth + axisPosition)*cos(axisAngle*PI/180);
    double rayCoord_x = ((double)(r-Nray/2)*rayWidth + axisPosition)*sin(axisAngle*PI/180) + sliceNumber - xDim/2;
    index1[r] = floor(rayCoord_y) + yDim/2;
    index2[r] = index2[r] + 1;
    weight1[r] = fabs(rayCoord - floor(rayCoord));
    weight2[r] = 1 - weight1[r] ;
  }
    
    //Extract sinograms from tilt series. Make a deep copy
    for (int z = 0; z < zDim; ++z) //loop through tilts (z-direction)
      for (int r = 0; r < Nray; ++r) //loop through rays (y-direction)
      {
        sinogram[z * Nray + r] = 0;
        if (index1[r]>=0 and index1[r]<yDim)
          sinogram[z * Nray + r] += dataPtr[z * xDim * yDim + index1[r] * xDim + sliceNumber] * weight1[r];
        if (index2[r]>=0 and index2[r]<yDim)
          sinogram[z * Nray + r] += dataPtr[z * xDim * yDim + index2[r] * xDim + sliceNumber] * weight2[r];
      }
    

  }
  }
  
void averageTiltSeries(vtkImageData *tiltSeries, float* average) //Average all tilts
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays in tilt series
  int zDim = extents[5] - extents[4] + 1; //number of tilts
  
  //Convert tiltSeries type to float
  vtkSmartPointer<vtkFloatArray> dataAsFloats = convertToFloat(tiltSeries);
  float *dataPtr = static_cast<float*>(dataAsFloats->GetVoidPointer(0)); //Get pointer to tilt series (of type float)
  
  for (int z = 0; z < zDim; ++z)
    for (int y = 0; y < yDim; ++y)
      for (int x = 0; x < xDim; ++x)
      {
        if (z==0)
          average[y * xDim + x ] = 0;
        //Update
        average[y * xDim + x ] += dataPtr[z * xDim * yDim + y * xDim + x];

        if (z==zDim-1) //Normalize
          average[y * xDim + x ] /= zDim;
      }
} //end of averageTiltSeries
  
} // end of namespace TomographyTiltSeries
} //end of namespace tomviz