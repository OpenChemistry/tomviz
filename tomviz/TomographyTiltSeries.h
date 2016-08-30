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

#ifndef tomvizTomographyTiltSeries_h
#define tomvizTomographyTiltSeries_h

#include "pqReaction.h"
#include "vtkImageData.h"

namespace tomviz {

class DataSource;

namespace TomographyTiltSeries {

/// Extract sinogram from tilt series. This takes as input an image and a slice
/// number.  If the input image has dimensions [x, y, z] the slice number must
/// be in the interval [0,y-1].  The output is stored in the sinogram pointer,
/// which should be a pointer to an array with dimensions [y, z, 1].
/// Simply takes a y-z slice of the input image. Useful for reconstruction
void getSinogram(vtkImageData* tiltSeries, int, float* sinogram);

/// Interpolate a sinogram of given size and rotation axis. Useful for axis
/// alignment
void getSinogram(vtkImageData* tiltSeries, int, float* sinogram, int Nray,
                 double axisPosition = 0);

// void getSinogram(vtkImageData *tiltSeries, int, float* sinogram,  int Nray,
// double axisPosition = 0, double axisAngle = 0);

/// Generate tiltseries from a volume
// void generaeTiltSeries(vtkImageData *volume, vtkImageData* tiltSeries);

void averageTiltSeries(vtkImageData* tiltSeries,
                       float* average); // Average all tilts
}
}

#endif
