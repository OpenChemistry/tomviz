/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "OperatorResult.h"

#include <vtkDataObject.h>
#include <vtkNew.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkTrivialProducer.h>

namespace tomviz {

OperatorResult::OperatorResult(QObject* parent)
  : Superclass(parent), m_name(tr("Unnamed"))
{}

OperatorResult::~OperatorResult()
{
  finalize();
}

void OperatorResult::setName(const QString& name)
{
  m_name = name;
}

const QString& OperatorResult::name()
{
  return m_name;
}

void OperatorResult::setLabel(const QString& label)
{
  m_label = label;
}

const QString& OperatorResult::label()
{
  return m_label;
}

bool OperatorResult::finalize()
{
  deleteProxy();
  return true;
}

vtkDataObject* OperatorResult::dataObject()
{
  vtkDataObject* object = nullptr;
  if (m_producerProxy.Get()) {
    vtkObjectBase* clientSideObject = m_producerProxy->GetClientSideObject();
    vtkTrivialProducer* producer =
      vtkTrivialProducer::SafeDownCast(clientSideObject);
    object = producer->GetOutputDataObject(0);
  }

  return object;
}

void OperatorResult::setDataObject(vtkDataObject* object)
{
  vtkDataObject* previousObject = dataObject();

  if (object == previousObject) {
    // Nothing to do
    return;
  }

  if (object == nullptr) {
    deleteProxy();
    return;
  }

  createProxyIfNeeded();

  // Set the output in the producer
  vtkObjectBase* clientSideObject = m_producerProxy->GetClientSideObject();
  vtkTrivialProducer* producer =
    vtkTrivialProducer::SafeDownCast(clientSideObject);
  producer->SetOutput(object);
}

vtkSMSourceProxy* OperatorResult::producerProxy()
{
  createProxyIfNeeded();
  return m_producerProxy;
}

void OperatorResult::createProxyIfNeeded()
{
  if (!m_producerProxy.Get()) {
    vtkSMProxyManager* proxyManager = vtkSMProxyManager::GetProxyManager();
    vtkSMSessionProxyManager* sessionProxyManager =
      proxyManager->GetActiveSessionProxyManager();

    vtkSmartPointer<vtkSMProxy> producerProxy;
    producerProxy.TakeReference(
      sessionProxyManager->NewProxy("sources", "TrivialProducer"));
    m_producerProxy = vtkSMSourceProxy::SafeDownCast(producerProxy);
    m_producerProxy->UpdateVTKObjects();

    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->PreInitializeProxy(m_producerProxy);
    controller->PostInitializeProxy(m_producerProxy);
    controller->RegisterPipelineProxy(m_producerProxy);
  }
}

void OperatorResult::deleteProxy()
{
  if (m_producerProxy.Get()) {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterPipelineProxy(m_producerProxy);
    m_producerProxy = nullptr;
  }
}

} // namespace tomviz
