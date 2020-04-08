/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonFactory.h"

#include "OperatorProxyBase.h"
#include "PipelineProxyBase.h"

namespace tomviz {

PythonFactory::~PythonFactory()
{
  delete m_operatorFactory;
}

PythonFactory& PythonFactory::instance()
{
  static PythonFactory theInstance;
  return theInstance;
}

OperatorProxyBase* PythonFactory::createOperatorProxy(void* o)
{
  if (m_operatorFactory)
    return m_operatorFactory->create(o);
  return nullptr;
}

PipelineProxyBase* PythonFactory::createPipelineProxy()
{
  if (m_pipelineFactory)
    return m_pipelineFactory->create();
  return nullptr;
}

} // namespace tomviz