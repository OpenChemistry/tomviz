/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorProxy_h
#define tomvizOperatorProxy_h

#include "core/OperatorProxyBase.h"

namespace tomviz {

class OperatorPython;

/** Pure virtual base class providing a proxy to the operator class. */
class OperatorProxy : public OperatorProxyBase
{
public:
  OperatorProxy(void* o);

  bool canceled() override;

  bool earlyCompleted() override;

  void setTotalProgressSteps(int progress) override;

  int totalProgressSteps() override;

  void setProgressStep(int progress) override;

  int progressStep() override;

  void setProgressMessage(const std::string& message) override;

  std::string progressMessage() override;

  void setProgressData(vtkImageData* object) override;

private:
  OperatorPython* m_op = nullptr;
};

class OperatorProxyFactory : public OperatorProxyBaseFactory
{
public:
  OperatorProxyBase* create(void* o) override;
  static void registerWithFactory();
};

} // namespace tomviz

#endif