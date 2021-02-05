/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumeManager.h"

#include "ModuleManager.h"
#include "ModuleVolume.h"

#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkMultiVolume.h>
#include <vtkNew.h>
#include <vtkTrivialProducer.h>

#include <vtkColorTransferFunction.h>
#include <vtkDataArray.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkVolumeProperty.h>

#include <vtkPVRenderView.h>
#include <vtkSMViewProxy.h>

namespace tomviz {

class ViewVolumes
{
public:
  vtkNew<vtkMultiVolume> multiVolume;
  vtkNew<vtkGPUVolumeRayCastMapper> mapper;
  vtkNew<vtkVolume> auxVolume;
  vtkNew<vtkImageData> auxData;
  vtkNew<vtkTrivialProducer> auxProducer;
  vtkNew<vtkVolumeProperty> auxProperty;
  vtkNew<vtkPiecewiseFunction> auxOpacity;
  vtkNew<vtkPiecewiseFunction> auxGradientOpacity;
  vtkNew<vtkColorTransferFunction> auxColors;
  QMap<ModuleVolume*, int> volumePorts;
  int currentPort = 1;
  bool allowMultiVolume = true;
  bool usingMultiVolume = false;
};

class VolumeManager::Internals
{
public:
  QMap<vtkSMViewProxy*, QSharedPointer<ViewVolumes>> views;
};

VolumeManager::VolumeManager(QObject* parentObject)
  : Superclass(parentObject), d(new VolumeManager::Internals())
{
  connect(&ModuleManager::instance(), &ModuleManager::moduleAdded, this,
          &VolumeManager::onModuleAdded);
  connect(&ModuleManager::instance(), &ModuleManager::moduleRemoved, this,
          &VolumeManager::onModuleRemoved);
}

VolumeManager::~VolumeManager() = default;

VolumeManager& VolumeManager::instance()
{
  static VolumeManager theInstance;
  return theInstance;
}

void VolumeManager::onModuleAdded(Module* module)
{
  auto volume = qobject_cast<ModuleVolume*>(module);
  if (volume) {
    connect(volume, &Module::visibilityChanged, this,
            &VolumeManager::onVisibilityChanged);
    auto view = volume->view();

    if (!this->d->views.contains(view)) {
      QSharedPointer<ViewVolumes> viewVolumes(new ViewVolumes());
      viewVolumes->auxData->SetDimensions(1, 1, 1);
      viewVolumes->auxData->AllocateScalars(VTK_FLOAT, 1);
      viewVolumes->auxData->GetPointData()->GetScalars()->Fill(1);
      viewVolumes->auxProducer->SetOutput(viewVolumes->auxData);
      viewVolumes->mapper->SetInputConnection(
        0, viewVolumes->auxProducer->GetOutputPort());

      viewVolumes->auxOpacity->AddPoint(0.0, 0.0);
      viewVolumes->auxGradientOpacity->AddPoint(0.0, 1.0);

      viewVolumes->auxColors->AddRGBPoint(0.0, 0.0, 0.0, 0.0);

      viewVolumes->auxProperty->SetColor(viewVolumes->auxColors);
      viewVolumes->auxProperty->SetScalarOpacity(viewVolumes->auxOpacity);
      // NOTE: Due to a bug in vtkMultiVolume, a gradient opacity function must
      // be set or the shader will fail to compile.
      viewVolumes->auxProperty->SetGradientOpacity(
        viewVolumes->auxGradientOpacity);
      viewVolumes->auxVolume->SetProperty(viewVolumes->auxProperty);

      viewVolumes->multiVolume->SetVolume(viewVolumes->auxVolume, 0);
      viewVolumes->multiVolume->SetMapper(viewVolumes->mapper);
      this->d->views.insert(view, viewVolumes);
    }

    this->d->views[view]->volumePorts[volume] =
      this->d->views[view]->currentPort++;
    auto allowMultiVolume = this->d->views[view]->allowMultiVolume;

    if (this->d->views[view]->volumePorts.size() >= MULTI_VOLUME_SWITCH &&
        allowMultiVolume) {
      this->multiVolumeOn(view);
    }

    emit volumeCountChanged(view, this->d->views[view]->volumePorts.size());
  }
}

void VolumeManager::onModuleRemoved(Module* module)
{
  auto volume = qobject_cast<ModuleVolume*>(module);
  if (volume) {
    auto view = volume->view();

    if (!this->d->views.contains(view)) {
      return;
    }

    auto& volumePorts = this->d->views[view]->volumePorts;
    auto& mapper = this->d->views[view]->mapper;
    auto& multiVolume = this->d->views[view]->multiVolume;

    auto it = volumePorts.find(volume);
    if (it != volumePorts.end()) {
      auto mod = it.key();
      auto port = it.value();
      auto vol = mod->getVolume();
      multiVolume->RemoveVolume(port);
      mapper->RemoveInputConnection(port,
                                    vol->GetMapper()->GetInputConnection(0, 0));
      volumePorts.erase(it);
    }

    if (this->d->views[view]->volumePorts.size() < MULTI_VOLUME_SWITCH) {
      this->multiVolumeOff(view);
    }

    emit volumeCountChanged(view, this->d->views[view]->volumePorts.size());
  }
}

void VolumeManager::multiVolumeOn(vtkSMViewProxy* view)
{
  if (!this->d->views.contains(view)) {
    return;
  }

  auto& volumePorts = this->d->views[view]->volumePorts;
  auto& mapper = this->d->views[view]->mapper;
  auto& multiVolume = this->d->views[view]->multiVolume;
  auto v = vtkPVRenderView::SafeDownCast(view->GetClientSideView());

  for (auto it = volumePorts.begin(); it != volumePorts.end(); ++it) {
    auto mod = it.key();
    auto port = it.value();
    auto vol = mod->getVolume();

    if (mod->visibility()) {
      mapper->SetInputConnection(port,
                                 vol->GetMapper()->GetInputConnection(0, 0));
      multiVolume->SetVolume(vol, port);
    }
    v->RemovePropFromRenderer(vol);
  }

  v->AddPropToRenderer(multiVolume);

  if (!this->d->views[view]->usingMultiVolume) {
    this->d->views[view]->usingMultiVolume = true;
    emit usingMultiVolumeChanged(view, true);
  }
}

void VolumeManager::multiVolumeOff(vtkSMViewProxy* view)
{
  if (!this->d->views.contains(view)) {
    return;
  }

  auto& volumePorts = this->d->views[view]->volumePorts;
  auto& mapper = this->d->views[view]->mapper;
  auto& multiVolume = this->d->views[view]->multiVolume;
  auto v = vtkPVRenderView::SafeDownCast(view->GetClientSideView());

  for (auto it = volumePorts.begin(); it != volumePorts.end(); ++it) {
    auto mod = it.key();
    auto port = it.value();
    auto vol = mod->getVolume();

    multiVolume->RemoveVolume(port);
    mapper->RemoveInputConnection(port,
                                  vol->GetMapper()->GetInputConnection(0, 0));

    v->AddPropToRenderer(vol);
  }

  v->RemovePropFromRenderer(multiVolume);

  if (this->d->views[view]->usingMultiVolume) {
    this->d->views[view]->usingMultiVolume = false;
    emit usingMultiVolumeChanged(view, false);
  }
}

void VolumeManager::allowMultiVolume(bool allowMultiVolume,
                                     vtkSMViewProxy* view)
{
  if (!this->d->views.contains(view)) {
    return;
  }

  auto usingMultiVolume = this->d->views[view]->usingMultiVolume;

  if (allowMultiVolume) {
    auto nVolumes = this->d->views[view]->volumePorts.size();
    if (!usingMultiVolume && nVolumes >= MULTI_VOLUME_SWITCH) {
      this->multiVolumeOn(view);
    }
  } else {
    if (usingMultiVolume) {
      this->multiVolumeOff(view);
    }
  }

  if (this->d->views[view]->allowMultiVolume != allowMultiVolume) {
    this->d->views[view]->allowMultiVolume = allowMultiVolume;
    emit allowMultiVolumeChanged(view, allowMultiVolume);
  }
}

bool VolumeManager::allowMultiVolume(vtkSMViewProxy* view) const
{
  if (!this->d->views.contains(view)) {
    return true;
  }

  return this->d->views[view]->usingMultiVolume;
}

int VolumeManager::volumeCount(vtkSMViewProxy* view) const
{
  if (!this->d->views.contains(view)) {
    return 0;
  }

  return this->d->views[view]->volumePorts.size();
}

void VolumeManager::onVisibilityChanged(bool visible)
{
  if (auto module = qobject_cast<ModuleVolume*>(sender())) {
    auto view = module->view();

    if (!this->d->views.contains(view)) {
      return;
    }

    auto& volumePorts = this->d->views[view]->volumePorts;
    auto& mapper = this->d->views[view]->mapper;
    auto& multiVolume = this->d->views[view]->multiVolume;
    auto usingMultiVolume = this->d->views[view]->usingMultiVolume;

    auto it = volumePorts.find(module);
    if (it == volumePorts.end()) {
      return;
    }

    auto port = it.value();
    auto vol = module->getVolume();

    if (usingMultiVolume) {
      if (visible) {
        mapper->SetInputConnection(port,
                                   vol->GetMapper()->GetInputConnection(0, 0));
        multiVolume->SetVolume(vol, port);
      } else {
        multiVolume->RemoveVolume(port);
        mapper->RemoveInputConnection(
          port, vol->GetMapper()->GetInputConnection(0, 0));
      }
    }
  }
}

} // namespace tomviz
