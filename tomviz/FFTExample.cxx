#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkImageFFT.h>


// So I think this is the basic version of what you want to do.  I'm not sure though.
// If you don't have a vtkImageData you would need to make one (see other function
//
// I don't know what the other options on VTK's FFT do... And you may want to set the dimensionality to 2
// Here are the other options:
// http://www.vtk.org/doc/nightly/html/classvtkImageFFT.html
void fftOfImage(vtkImageData *image)
{
  vtkNew<vtkImageFFT> fft;
  fft->SetInputData(image);
  fft->SetDimensionality(3);
  fft->Update();
  image->ShallowCopy(fft->GetOutput());
}

// This should probably be rewritten to use FFTW directly since that will be faster...
vtkImageData *fftOfArray(float *array, int dimensions[3])
{
  // Allocate the image data
  vtkImageData *data = vtkImageData::New();
  data->SetExtent(0, dimensions[0] - 1, 0, dimensions[1] - 1, 0, dimensions[2] - 1);
  data->AllocateScalars(VTK_FLOAT, 1);
  vtkDataArray *dataArray = data->GetPointData()->GetScalars();
  float *d = (float*) dataArray->GetVoidPointer(0);
  // copy the data into the allocated array
  std::copy(array, array + (((size_t)(dimensions[0]) * dimensions[1]) * dimensions[2]), d);

  fftOfImage(data);

  return data;
}
