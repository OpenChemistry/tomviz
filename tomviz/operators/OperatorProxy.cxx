/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorProxy.h"

#include "core/PythonFactory.h"

#include "OperatorPython.h"

#include <vtkImageData.h>

namespace tomviz {

OperatorProxy::OperatorProxy(void* o) : OperatorProxyBase(o)
{
  m_op = static_cast<OperatorPython*>(o);
}

bool OperatorProxy::canceled()
{
  return m_op->isCanceled();
}

bool OperatorProxy::earlyCompleted()
{
  return m_op->isEarlyCompleted();
}

void OperatorProxy::setTotalProgressSteps(int progress)
{
  m_op->setTotalProgressSteps(progress);
}

int OperatorProxy::totalProgressSteps()
{
  return m_op->totalProgressSteps();
}

void OperatorProxy::setProgressStep(int progress)
{
  m_op->setProgressStep(progress);
}

int OperatorProxy::progressStep()
{
  return m_op->progressStep();
}

void OperatorProxy::setProgressMessage(const std::string& message)
{
  QString msg = QString::fromStdString(message);
  m_op->setProgressMessage(msg);
}

std::string OperatorProxy::progressMessage()
{
  return m_op->progressMessage().toStdString();
}

void OperatorProxy::setProgressData(vtkImageData* imageData)
{
  TemporarilyReleaseGil releaseMe;
  emit m_op->childDataSourceUpdated(imageData);
}

OperatorProxyBase* OperatorProxyFactory::create(void* o)
{
  return new OperatorProxy(o);
}

void OperatorProxyFactory::registerWithFactory()
{
  PythonFactory::instance().setOperatorProxyFactory(new OperatorProxyFactory);
}

} // namespace tomviz
