/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleVolume.h"
#include "ModuleVolumeWidget.h"

#include "DataSource.h"
#include "HistogramManager.h"
#include "ScalarsComboBox.h"
#include "vtkTransferFunctionBoxItem.h"

#include <vtkColorTransferFunction.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageClip.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPlane.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>
#include <vtkVector.h>
#include <vtkView.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <vtkPVRenderView.h>
#include <vtkPointData.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace tomviz {

ModuleVolume::ModuleVolume(QObject* parentObject) : Module(parentObject)
{
  connect(&HistogramManager::instance(), &HistogramManager::histogram2DReady,
          this, [=](vtkSmartPointer<vtkImageData> image,
                    vtkSmartPointer<vtkImageData> histogram2D) {
            // Force the transfer function 2D to update.
            if (image ==
                vtkImageData::SafeDownCast(dataSource()->dataObject())) {
              auto colorMap = vtkColorTransferFunction::SafeDownCast(
                this->colorMap()->GetClientSideObject());
              auto opacityMap = vtkPiecewiseFunction::SafeDownCast(
                this->opacityMap()->GetClientSideObject());
              vtkTransferFunctionBoxItem::rasterTransferFunction2DBox(
                histogram2D, *this->transferFunction2DBox(),
                transferFunction2D(), colorMap, opacityMap);
            }
            // Update the volume mapper.
            this->updateColorMap();
            emit this->renderNeeded();
          });
}

ModuleVolume::~ModuleVolume()
{
  finalize();
}

QIcon ModuleVolume::icon() const
{
  return QIcon(":/icons/pqVolumeData.png");
}

void ModuleVolume::initializeMapper(DataSource* data)
{
  vtkAlgorithmOutput *output = nullptr;
  if (data == nullptr) {
    output = m_volumeMapper->GetInputConnection(0, 0);
  }
  else {
    output = data->producer()->GetOutputPort();
  }

  m_volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
  m_volumeMapper->SetInputConnection(output);
  m_volumeMapper->SetScalarModeToUsePointFieldData();
  m_volumeMapper->SelectScalarArray(scalarsIndex());
  m_volume->SetMapper(m_volumeMapper);
  m_volumeMapper->UseJitteringOn();
  m_volumeMapper->SetBlendMode(vtkVolumeMapper::COMPOSITE_BLEND);
  if (m_view != nullptr) {
    m_view->Update();
  }
}

bool ModuleVolume::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  initializeMapper(data);
  m_volume->SetProperty(m_volumeProperty);
  const double* displayPosition = data->displayPosition();
  m_volume->SetPosition(displayPosition[0], displayPosition[1],
                        displayPosition[2]);
  m_volumeProperty->SetInterpolationType(VTK_LINEAR_INTERPOLATION);
  m_volumeProperty->SetAmbient(0.0);
  m_volumeProperty->SetDiffuse(1.0);
  m_volumeProperty->SetSpecular(1.0);
  m_volumeProperty->SetSpecularPower(100.0);

  updateColorMap();

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_view->AddPropToRenderer(m_volume);
  m_view->Update();

  connect(data, &DataSource::activeScalarsChanged, this,
          &ModuleVolume::onScalarArrayChanged);

  connect(this, &ModuleVolume::updateClipFilter, this, 
          &ModuleVolume::onClipFilterChanged);
  // Work around mapper bug on the mac, see the following issue for details:
  // https://github.com/OpenChemistry/tomviz/issues/1776
  // Should be removed when this is fixed.
#if defined(Q_OS_MAC)
  connect(data, &DataSource::dataChanged, this,
          [this]() { this->initializeMapper(); });
#endif
  return true;
}

void ModuleVolume::updateColorMap()
{
  m_volumeProperty->SetScalarOpacity(
    vtkPiecewiseFunction::SafeDownCast(opacityMap()->GetClientSideObject()));
  m_volumeProperty->SetColor(
    vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject()));

  int propertyMode = vtkVolumeProperty::TF_1D;
  const Module::TransferMode mode = getTransferMode();
  switch (mode) {
    case (Module::SCALAR):
      m_volumeProperty->SetGradientOpacity(nullptr);
      break;
    case (Module::GRADIENT_1D):
      m_volumeProperty->SetGradientOpacity(gradientOpacityMap());
      break;
    case (Module::GRADIENT_2D):
      if (transferFunction2D() && transferFunction2D()->GetExtent()[1] > 0) {
        propertyMode = vtkVolumeProperty::TF_2D;
        m_volumeProperty->SetTransferFunction2D(transferFunction2D());
      } else {
        vtkSmartPointer<vtkImageData> image =
          vtkImageData::SafeDownCast(dataSource()->dataObject());
        // See if the histogram is done, if it is then update the transfer
        // function.
        if (auto histogram2D =
              HistogramManager::instance().getHistogram2D(image)) {
          auto colorMap = vtkColorTransferFunction::SafeDownCast(
            this->colorMap()->GetClientSideObject());
          auto opacityMap = vtkPiecewiseFunction::SafeDownCast(
            this->opacityMap()->GetClientSideObject());
          vtkTransferFunctionBoxItem::rasterTransferFunction2DBox(
            histogram2D, *this->transferFunction2DBox(), transferFunction2D(),
            colorMap, opacityMap);
          propertyMode = vtkVolumeProperty::TF_2D;
          m_volumeProperty->SetTransferFunction2D(transferFunction2D());
        }
        // If this histogram is not ready, then it finishing will trigger the
        // functor created in the constructor and the volume mapper will be
        // updated when the histogram2D is done
      }
      break;
  }

  m_volumeProperty->SetTransferFunctionMode(propertyMode);

  // BUG: volume mappers don't update property when LUT is changed and has an
  // older Mtime. Fix for now by forcing the LUT to update.
  vtkObject::SafeDownCast(colorMap()->GetClientSideObject())->Modified();
}

bool ModuleVolume::finalize()
{
  if (m_view) {
    m_view->RemovePropFromRenderer(m_volume);
  }

  return true;
}

bool ModuleVolume::setVisibility(bool val)
{
  m_volume->SetVisibility(val ? 1 : 0);

  Module::setVisibility(val);

  return true;
}

bool ModuleVolume::visibility() const
{
  return m_volume->GetVisibility() != 0;
}

QJsonObject ModuleVolume::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["transferMode"] = getTransferMode();
  props["interpolation"] = m_volumeProperty->GetInterpolationType();
  props["blendingMode"] = m_volumeMapper->GetBlendMode();
  props["rayJittering"] = m_volumeMapper->GetUseJittering() == 1;

  QJsonObject lighting;
  lighting["enabled"] = m_volumeProperty->GetShade() == 1;
  lighting["ambient"] = m_volumeProperty->GetAmbient();
  lighting["diffuse"] = m_volumeProperty->GetDiffuse();
  lighting["specular"] = m_volumeProperty->GetSpecular();
  lighting["specularPower"] = m_volumeProperty->GetSpecularPower();
  props["lighting"] = lighting;

  json["properties"] = props;
  return json;
}

bool ModuleVolume::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();

    setTransferMode(
      static_cast<Module::TransferMode>(props["transferMode"].toInt()));
    onInterpolationChanged(props["interpolation"].toInt());
    setBlendingMode(props["blendingMode"].toInt());
    setJittering(props["rayJittering"].toBool());

    if (props["lighting"].isObject()) {
      auto lighting = props["lighting"].toObject();
      setLighting(lighting["enabled"].toBool());
      onAmbientChanged(lighting["ambient"].toDouble());
      onDiffuseChanged(lighting["diffuse"].toDouble());
      onSpecularChanged(lighting["specular"].toDouble());
      onSpecularPowerChanged(lighting["specularPower"].toDouble());
    }

    updatePanel();
    onScalarArrayChanged();
    return true;
  }
  return false;
}

void ModuleVolume::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }
  if (!m_controllers) {
    m_controllers = new ModuleVolumeWidget;
  }

  m_scalarsCombo = new ScalarsComboBox();
  m_scalarsCombo->setOptions(dataSource(), this);
  m_controllers->formLayout()->insertRow(0, "Active Scalars", m_scalarsCombo);

  QVBoxLayout* layout = new QVBoxLayout;
  panel->setLayout(layout);

  // Create, update and connect
  layout->addWidget(m_controllers);
  updatePanel();

  connect(m_controllers, SIGNAL(jitteringToggled(const bool)), this,
          SLOT(setJittering(const bool)));
  connect(m_controllers, SIGNAL(lightingToggled(const bool)), this,
          SLOT(setLighting(const bool)));
  connect(m_controllers, SIGNAL(blendingChanged(const int)), this,
          SLOT(setBlendingMode(const int)));
  connect(m_controllers, SIGNAL(interpolationChanged(const int)), this,
          SLOT(onInterpolationChanged(const int)));
  connect(m_controllers, SIGNAL(ambientChanged(const double)), this,
          SLOT(onAmbientChanged(const double)));
  connect(m_controllers, SIGNAL(diffuseChanged(const double)), this,
          SLOT(onDiffuseChanged(const double)));
  connect(m_controllers, SIGNAL(specularChanged(const double)), this,
          SLOT(onSpecularChanged(const double)));
  connect(m_controllers, SIGNAL(specularPowerChanged(const double)), this,
          SLOT(onSpecularPowerChanged(const double)));
  connect(m_controllers, SIGNAL(transferModeChanged(const int)), this,
          SLOT(onTransferModeChanged(const int)));
  connect(m_scalarsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            setActiveScalars(m_scalarsCombo->itemData(idx).toInt());
            onScalarArrayChanged();
          });
}

void ModuleVolume::updatePanel()
{
  // If m_controllers is present update the values, if not they will be updated
  // when it is created and shown.
  if (!m_controllers || !m_volumeMapper || !m_volumeProperty ||
      !m_scalarsCombo) {
    return;
  }
  m_controllers->setJittering(
    static_cast<bool>(m_volumeMapper->GetUseJittering()));
  m_controllers->setLighting(static_cast<bool>(m_volumeProperty->GetShade()));
  m_controllers->setBlendingMode(m_volumeMapper->GetBlendMode());
  m_controllers->setAmbient(m_volumeProperty->GetAmbient());
  m_controllers->setDiffuse(m_volumeProperty->GetDiffuse());
  m_controllers->setSpecular(m_volumeProperty->GetSpecular());
  m_controllers->setSpecularPower(m_volumeProperty->GetSpecularPower());
  m_controllers->setInterpolationType(m_volumeProperty->GetInterpolationType());

  const auto tfMode = getTransferMode();
  m_controllers->setTransferMode(tfMode);

  m_scalarsCombo->setOptions(dataSource(), this);
}

void ModuleVolume::onTransferModeChanged(const int mode)
{
  setTransferMode(static_cast<Module::TransferMode>(mode));
  updateColorMap();

  emit transferModeChanged(mode);
  emit renderNeeded();
}

vtkDataObject* ModuleVolume::dataToExport()
{
  auto trv = dataSource()->producer();
  return trv->GetOutputDataObject(0);
}

void ModuleVolume::onAmbientChanged(const double value)
{
  m_volumeProperty->SetAmbient(value);
  emit renderNeeded();
}

void ModuleVolume::onDiffuseChanged(const double value)
{
  m_volumeProperty->SetDiffuse(value);
  emit renderNeeded();
}

void ModuleVolume::onSpecularChanged(const double value)
{
  m_volumeProperty->SetSpecular(value);
  emit renderNeeded();
}

void ModuleVolume::onSpecularPowerChanged(const double value)
{
  m_volumeProperty->SetSpecularPower(value);
  emit renderNeeded();
}

void ModuleVolume::onInterpolationChanged(const int type)
{
  m_volumeProperty->SetInterpolationType(type);
  emit renderNeeded();
}

void ModuleVolume::dataSourceMoved(double newX, double newY, double newZ)
{
  vtkVector3d pos(newX, newY, newZ);
  m_volume->SetPosition(pos.GetData());
}

void ModuleVolume::setLighting(const bool val)
{
  m_volumeProperty->SetShade(val ? 1 : 0);
  emit renderNeeded();
}

void ModuleVolume::setBlendingMode(const int mode)
{
  m_volumeMapper->SetBlendMode(mode);
  emit renderNeeded();
}

void ModuleVolume::setJittering(const bool val)
{
  m_volumeMapper->SetUseJittering(val ? 1 : 0);
  emit renderNeeded();
}

void ModuleVolume::onScalarArrayChanged()
{
  m_volumeMapper->SelectScalarArray(scalarsIndex());
  auto tp = dataSource()->producer();
  if (tp) {
    tp->GetOutputDataObject(0)->Modified();
  }
  emit renderNeeded();
}

int ModuleVolume::scalarsIndex()
{
  int index;
  if (activeScalars() == Module::DEFAULT_SCALARS) {
    index = dataSource()->activeScalarsIdx();
  } else {
    index = activeScalars();
  }
  return index;
}

void ModuleVolume::onClipFilterChanged(vtkPlane* plane, bool newFilter) {
  if (m_volumeMapper->GetNumberOfClippingPlanes()) {
    m_volumeMapper->RemoveClippingPlane(plane);
  }
  if (!newFilter) {
    m_volumeMapper->AddClippingPlane(plane);
  }

  emit renderNeeded();
}

} // end of namespace tomviz
