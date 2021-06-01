/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorProxyBase_h
#define tomvizOperatorProxyBase_h

#include <string>

class vtkImageData;

namespace tomviz {

/** Pure virtual base class providing a proxy to the operator class. */
class OperatorProxyBase
{
public:
  OperatorProxyBase(void*) {}
  virtual ~OperatorProxyBase() = default;

  virtual bool canceled() = 0;

  virtual bool done() = 0;

  virtual void setTotalProgressSteps(int progress) = 0;

  virtual int totalProgressSteps() = 0;

  virtual void setProgressStep(int progress) = 0;

  virtual int progressStep() = 0;

  virtual void setProgressMessage(const std::string& message) = 0;

  virtual std::string progressMessage() = 0;

  virtual void setProgressData(vtkImageData* object) = 0;
};

class OperatorProxyBaseFactory
{
public:
  virtual ~OperatorProxyBaseFactory() = default;
  virtual OperatorProxyBase* create(void* o) = 0;
};

} // namespace tomviz

#endif