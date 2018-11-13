/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorPythonWrapper_h
#define tomvizOperatorPythonWrapper_h

#include <string>

class vtkImageData;

namespace tomviz {
class OperatorPython;
}

struct OperatorPythonWrapper
{
  OperatorPythonWrapper(void* o);
  bool canceled();
  void setTotalProgressSteps(int progress);
  int totalProgressSteps();
  void setProgressStep(int progress);
  int progressStep();
  void setProgressMessage(const std::string& message);
  std::string progressMessage();
  // Dummy getter needed by our pybind11 version. TODO: Update to latest
  // pybind11 as it supports write-only properties.
  void progressData();
  void setProgressData(vtkImageData* object);

  tomviz::OperatorPython* op = nullptr;
};

#endif
