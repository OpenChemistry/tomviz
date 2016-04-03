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
#include "TomographyTiltSeries.h"

#include "vtkImageData.h"
#include "vtkFieldData.h"
#include "vtkDataArray.h"
#define PI 3.14159265359
#include "vtkPointData.h"
#include "vtkFloatArray.h"
#include "vtkSmartPointer.h"

#include <QDebug>

namespace
{

// Conversion code
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
}

namespace tomviz
{

namespace TomographyReconstruction
{

// 3D Weighted Back Projection reconstruction
void weightedBackProjection3(vtkImageData *tiltSeries,vtkImageData *recon)
{
  int extents[6];
  tiltSeries->GetExtent(extents);
  int xDim = extents[1] - extents[0] + 1; //number of slices
  int yDim = extents[3] - extents[2] + 1; //number of rays
  int zDim = extents[5] - extents[4] + 1; //number of tilts

  // Get tilt angles
  vtkDataArray *tiltAnglesArray = tiltSeries->GetFieldData()->GetArray("tilt_angles");
  double *tiltAngles = static_cast<double*>(tiltAnglesArray->GetVoidPointer(0));

  // Creating the output volume and getting a pointer to it
  int outputSize[3] = { xDim, yDim, yDim};
  recon->SetExtent(0, outputSize[0] - 1, 0, outputSize[1] - 1, 0, outputSize[2] - 1);
  recon->AllocateScalars(VTK_FLOAT, 1); // 1 is for one component (i.e. not vector)
  float *reconPtr = static_cast<float*>(recon->GetScalarPointer());

  // Reconstruction
  float *sinogram = new float[yDim*zDim]; // Placeholder for 2D sinogram
  float *recon2d = new float[yDim*yDim];  // Placeholder for 2D reconstruction (y-z plane)
  for (int s = 0; s < xDim; ++s) // Loop through slices (x-direction)
  {
    // Get sinogram
    TomographyTiltSeries::getSinogram(tiltSeries, s, sinogram);
    // 2D back projection
    TomographyReconstruction::unweightedBackProjection2(sinogram, tiltAngles, recon2d, zDim, yDim);
    // Put recon into
    for (int iy = 0; iy < outputSize[1]; ++iy) // Loop through all pixels in reconstructed image (y-z plane)
      for (int iz = 0; iz < outputSize[2]; ++iz)
      {
        reconPtr[iz*outputSize[0]*outputSize[1] + iy*outputSize[0] + s] = recon2d[iy*outputSize[1] + iz];
      }
  }
}

// 2D WBP recon
void unweightedBackProjection2(float *sinogram, double *tiltAngles,
                               float* image, int numOfTilts, int numOfRays)
{
  for (int i = 0; i < numOfRays*numOfRays; ++i)
  {
    image[i] = 0; //Set all pixels to zero
  }

  // 2D unweighted Back Projection
  for (int tt = 0; tt < numOfTilts; ++tt) // Loop through tilts
  {
    double angle = tiltAngles[tt]*PI/180;
    for (int iy = 0; iy < numOfRays; ++iy) // Loop through all pixels in reconstructed image (y-z plane for a tilt series)
      for (int iz = 0; iz < numOfRays; ++iz)
      {
        // Calcualte y,z coord.
        double y = iy + 0.5 - ((double)numOfRays)/2.0;
        double z = iz + 0.5 - ((double)numOfRays)/2.0;
        // Calculate ray coord.
        double t = y * cos(angle) + z * sin(angle);

        if (t >= -numOfRays/2 && t <= numOfRays/2 )	//check if ray is inside projection
        {
          int rayIndex = floor((t + numOfRays/2));
          if (rayIndex >= 0 && rayIndex <= numOfRays-2)
          {
            // Linear interpolation
            double Q1 = sinogram[tt*numOfRays + rayIndex ];
            double Q2 = sinogram[tt*numOfRays + rayIndex+1 ];
            double QDash = Q1 + (t-double(rayIndex-numOfRays/2))*(Q2-Q1);
            image[iy*numOfRays + iz] += QDash;
          }
        }
      }
  }

  double normalizationFactor = PI/double(2*numOfTilts);
  for (int i = 0; i < numOfRays*numOfRays; ++i)
  {
    image[i] *= normalizationFactor;
  }
}

}
}
