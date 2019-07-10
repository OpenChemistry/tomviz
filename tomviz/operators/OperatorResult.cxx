/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorResult.h"

#include "ActiveObjects.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"
#include "ModuleMolecule.h"

#include <vtkDataObject.h>
#include <vtkMolecule.h>
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

void OperatorResult::setName(QString name)
{
  m_name = name;
}

QString OperatorResult::name() const
{
  return m_name;
}

void OperatorResult::setLabel(QString label)
{
  m_label = label;
}

QString OperatorResult::label() const
{
  return m_label;
}

void OperatorResult::setDescription(QString desc)
{
  m_description = desc;
}

QString OperatorResult::description() const
{
  return m_description;
}

bool OperatorResult::finalize()
{
  deleteProxy();
  return true;
}

vtkDataObject* OperatorResult::dataObject()
{
  vtkDataObject* object = nullptr;
  if (m_producerProxy) {
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
  // If the result is a vtkMolecule, create a ModuleMolecule to display it
  if (vtkMolecule::SafeDownCast(object)) {
    auto view = ActiveObjects::instance().activeView();
    ModuleManager::instance().createAndAddModule("Molecule", this, view);
  }
}

vtkSMSourceProxy* OperatorResult::producerProxy()
{
  createProxyIfNeeded();
  return m_producerProxy;
}

void OperatorResult::createProxyIfNeeded()
{
  if (!m_producerProxy) {
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
  if (m_producerProxy) {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterPipelineProxy(m_producerProxy);
    m_producerProxy = nullptr;
  }
}

} // namespace tomviz
