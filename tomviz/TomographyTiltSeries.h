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

namespace tomviz
{
class DataSource;

namespace TomographyTiltSeries
{
    
void getSinogram(vtkImageData *tiltSeries, int, float* sinogram); //Useful for recon
void getSinogram(vtkImageData *tiltSeries, int, float* sinogram,  int Nray, double axisPosition = 0); //Extract sinograms from tilt series

void getSinogram(vtkImageData *tiltSeries, int, float* sinogram,  int Nray, double axisPosition = 0, double axisAngle = 0); //Extract sinograms from tilt series

//Generate tiltseries from a volume
//void generaeTiltSeries(vtkImageData *volume, vtkImageData* tiltSeries);
  
void averageTiltSeries(vtkImageData *tiltSeries, float* average); //Average all tilts


}
}

#endif
