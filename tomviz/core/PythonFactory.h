/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPythonFactory_h
#define tomvizPythonFactory_h

namespace tomviz {

class OperatorProxyBase;
class OperatorProxyBaseFactory;
class PipelineProxyBase;
class PipelineProxyBaseFactory;

class PythonFactory
{
public:
  /// Returns reference to the singleton instance.
  static PythonFactory& instance();

  /// Get an instance of the OperatorProxy class for this runtime.
  OperatorProxyBase* createOperatorProxy(void* o);

  /// Set the proxy factory for the singleton.
  void setOperatorProxyFactory(OperatorProxyBaseFactory* factory)
  {
    m_operatorFactory = factory;
  }

  /// Get an instance of the PipelineProxy class for this runtime.
  PipelineProxyBase* createPipelineProxy();

  /// Set the proxy factory for the singleton.
  void setPipelineProxyFactory(PipelineProxyBaseFactory* factory)
  {
    m_pipelineFactory = factory;
  }

private:
  PythonFactory() = default;
  ~PythonFactory();
  PythonFactory(const PythonFactory&) = delete;
  PythonFactory& operator=(const PythonFactory&) = delete;

  OperatorProxyBaseFactory* m_operatorFactory = nullptr;
  PipelineProxyBaseFactory* m_pipelineFactory = nullptr;
};

} // namespace tomviz

#endif