/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorPythonWrapper.h"

#include "core/OperatorProxyBase.h"
#include "core/PythonFactory.h"

using namespace tomviz;

OperatorPythonWrapper::OperatorPythonWrapper(void* o)
{
  m_op = PythonFactory::instance().createOperatorProxy(o);
}

bool OperatorPythonWrapper::canceled()
{
  return m_op->canceled();
}

bool OperatorPythonWrapper::done()
{
  return m_op->done();
}

void OperatorPythonWrapper::setTotalProgressSteps(int progress)
{
  m_op->setTotalProgressSteps(progress);
}

int OperatorPythonWrapper::totalProgressSteps()
{
  return m_op->totalProgressSteps();
}

void OperatorPythonWrapper::setProgressStep(int progress)
{
  m_op->setProgressStep(progress);
}

int OperatorPythonWrapper::progressStep()
{
  return m_op->progressStep();
}

void OperatorPythonWrapper::setProgressMessage(const std::string& message)
{
  m_op->setProgressMessage(message);
}

std::string OperatorPythonWrapper::progressMessage()
{
  return m_op->progressMessage();
}

void OperatorPythonWrapper::progressData() {}

void OperatorPythonWrapper::setProgressData(vtkImageData* imageData)
{
  m_op->setProgressData(imageData);
}
