/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizEmdFormat_h
#define tomvizEmdFormat_h

#include <string>

class vtkImageData;

namespace tomviz {

class DataSource;

class EmdFormat
{
public:
  bool read(const std::string& fileName, vtkImageData* data);
  bool write(const std::string& fileName, DataSource* source);
  bool write(const std::string& fileName, vtkImageData* image);

  // This will force the generic HDF5 format to ask for subsample
  void setAskForSubsample(bool b) { m_askForSubsample = b; }

private:
  bool m_askForSubsample = false;
};
} // namespace tomviz

#endif // tomvizEmdFormat_h
