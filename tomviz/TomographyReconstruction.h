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

#ifndef tomvizTomographyReconstruction_h
#define tomvizTomographyReconstruction_h

#include "pqReaction.h"
#include "vtkImageData.h"

namespace tomviz
{
class DataSource;

namespace TomographyReconstruction
{
    
// This takes an image tiltSeries and a vtkImageData in which to place the output (recon)
void weightedBackProjection3(vtkImageData *tiltSeries,vtkImageData *recon); //3D WBP recon

// This function takes a y-z slice (sinogram) and the tilt angles as input and creates a
// slice throught the reconstruction space.  The numOfTilts parameter is the size of the z dimension.
// The numOfRays parameter is the size in the y dimension.  The tilt angles is a one dimensional array
// with length numOfTilts.
// The output image will be stored in recon and will be square with size numOfRays by numOfRays.
void unweightedBackProjection2(float *sinogram, double *tiltAngles, float *recon,int numOfTilts, int numOfRays ); //2D WBP recon

// This takes as input an image and a slice number.  If the input image has dimensions
// [x, y, z] the slice number must be in the interval [0,y-1].  The output is stored in
// the sinogram pointer, which should be a pointer to an array with dimensions [y, z, 1].
// Essentially, this extracts a y-z slice of the input image.
void tiltSeriesToSinogram(vtkImageData *tiltSeries, int, float* sinogram); //Extract sinograms from tilt series

}
}

#endif
