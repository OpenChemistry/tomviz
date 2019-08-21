/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
/// "tiltAxis" is 0 if the tilt axis is X, and 1 if the tilt axis is Y
void getSinogram(vtkImageData* tiltSeries, int, float* sinogram, int Nray,
                 double axisPosition = 0, int tiltAxis = 0);

// void getSinogram(vtkImageData *tiltSeries, int, float* sinogram,  int Nray,
// double axisPosition = 0, double axisAngle = 0);

/// Generate tiltseries from a volume
// void generaeTiltSeries(vtkImageData *volume, vtkImageData* tiltSeries);

void averageTiltSeries(vtkImageData* tiltSeries,
                       float* average); // Average all tilts
} // namespace TomographyTiltSeries
} // namespace tomviz

#endif
