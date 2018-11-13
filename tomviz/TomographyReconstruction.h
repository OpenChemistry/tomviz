/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTomographyReconstruction_h
#define tomvizTomographyReconstruction_h

#include <pqReaction.h>
#include <vtkImageData.h>

namespace tomviz {
class DataSource;

namespace TomographyReconstruction {

// This takes an image tiltSeries and a vtkImageData in which to place the
// output (recon)
void weightedBackProjection3(vtkImageData* tiltSeries,
                             vtkImageData* recon); // 3D WBP recon

// This function takes a y-z slice (sinogram) and the tilt angles as input and
// creates a slice throught the reconstruction space.  The numOfTilts parameter
// is the size of the z dimension.
//
// The numOfRays parameter is the size in the y dimension.  The tilt angles is a
// one dimensional array with length numOfTilts.
//
// The output image will be stored in recon and will be square with size
// numOfRays by numOfRays.
void unweightedBackProjection2(float* sinogram, double* tiltAngles,
                               float* recon, int numOfTilts,
                               int numOfRays); // 2D WBP recon
} // namespace TomographyReconstruction
} // namespace tomviz

#endif
