/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef vtkOMETiffReader_h
#define vtkOMETiffReader_h

#include "vtkImageReader2.h"
#include "vtkOMETiffReaderModule.h"

namespace tomviz
{

class VTKOMETIFFREADER_EXPORT vtkOMETiffReader : public vtkImageReader2
{
public:
  static vtkOMETiffReader *New();
  vtkTypeMacro(vtkOMETiffReader, vtkImageReader2)
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /**
   * Is the given file name a tiff file?
   */
  int CanReadFile(const char* fname) VTK_OVERRIDE;

  /**
   * Get the file extensions for this format.
   * Returns a string with a space separated list of extensions in
   * the format .extension
   */
  const char* GetFileExtensions() VTK_OVERRIDE
  {
    return ".tif .tiff";
  }

  /**
   * Return a descriptive name for the file format that might be useful
   * in a GUI.
   */
  const char* GetDescriptiveName() VTK_OVERRIDE
  {
    return "TIFF";
  }

protected:
  vtkOMETiffReader();
  ~vtkOMETiffReader() VTK_OVERRIDE;

  enum { NOFORMAT, RGB, GRAYSCALE, PALETTE_RGB, PALETTE_GRAYSCALE, OTHER };

  void ExecuteInformation() VTK_OVERRIDE;
  void ExecuteDataWithInformation(vtkDataObject *out, vtkInformation *outInfo) VTK_OVERRIDE;

private:
  vtkOMETiffReader(const vtkOMETiffReader&) VTK_DELETE_FUNCTION;
  void operator=(const vtkOMETiffReader&) VTK_DELETE_FUNCTION;

  /**
   * Evaluates the image at a single pixel location.
   */
  template<typename T>
  int EvaluateImageAt(T* out, T* in);

  /**
   * Look up color paletter values.
   */
  void GetColor(int index,
                unsigned short *r, unsigned short *g, unsigned short *b);

  // To support Zeiss images
  void ReadTwoSamplesPerPixelImage(void *out,
                                   unsigned int vtkNotUsed(width),
                                   unsigned int height);

  unsigned int GetFormat();

  /**
   * Auxiliary methods used by the reader internally.
   */
  void Initialize();

  /**
   * Internal method, do not use.
   */
  template<typename T>
  void ReadImageInternal(T* buffer);

  /**
   * Reads 3D data from multi-pages tiff.
   */
  template<typename T>
  void ReadVolume(T* buffer);

  /**
   * Reads 3D data from tiled tiff
   */
  void ReadTiles(void* buffer);

  /**
   * Reads a generic image.
   */
  template<typename T>
  void ReadGenericImage(T* out, unsigned int width, unsigned int height);

  /**
   * Dispatch template to determine pixel type and decide on reader actions.
   */
  template <typename T>
  void Process(T *outPtr, int outExtent[6], vtkIdType outIncr[3]);

  /**
   * Second layer of dispatch necessary for some TIFF types.
   */
  template <typename T>
  void Process2(T *outPtr, int *outExt);

  class vtkOMETiffReaderInternal;

  unsigned short *ColorRed;
  unsigned short *ColorGreen;
  unsigned short *ColorBlue;
  int TotalColors;
  unsigned int ImageFormat;
  vtkOMETiffReaderInternal *InternalImage;
  int OutputExtent[6];
  vtkIdType OutputIncrements[3];
  unsigned int OrientationType;
  bool OrientationTypeSpecifiedFlag;
  bool OriginSpecifiedFlag;
  bool SpacingSpecifiedFlag;
};

}
#endif
