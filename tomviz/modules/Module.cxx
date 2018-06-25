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
#include "Module.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "OperatorResult.h"
#include "Utilities.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqCoreUtilities.h>
#include <pqPVApplicationCore.h>
#include <pqProxiesWidget.h>
#include <pqView.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>

namespace tomviz {

class Module::MInternals
{
  vtkSmartPointer<vtkSMProxy> m_detachedColorMap;
  vtkSmartPointer<vtkSMProxy> m_detachedOpacityMap;

public:
  vtkWeakPointer<vtkSMProxy> m_colorMap;
  vtkWeakPointer<vtkSMProxy> m_opacityMap;
  vtkNew<vtkPiecewiseFunction> m_gradientOpacityMap;

  Module::TransferMode m_transferMode;
  vtkNew<vtkImageData> m_transfer2D;
  vtkSMProxy* detachedColorMap()
  {
    if (!m_detachedColorMap) {
      static unsigned int colorMapCounter = 0;
      ++colorMapCounter;

      auto pxm = ActiveObjects::instance().proxyManager();

      vtkNew<vtkSMTransferFunctionManager> tfmgr;
      m_detachedColorMap = tfmgr->GetColorTransferFunction(
        QString("ModuleColorMap%1").arg(colorMapCounter).toLatin1().data(),
        pxm);
      m_detachedOpacityMap =
        vtkSMPropertyHelper(m_detachedColorMap, "ScalarOpacityFunction")
          .GetAsProxy();
    }
    return m_detachedColorMap;
  }

  vtkSMProxy* detachedOpacityMap()
  {
    detachedColorMap();
    return m_detachedOpacityMap;
  }
};

Module::Module(QObject* parentObject)
  : QObject(parentObject), d(new Module::MInternals())
{}

Module::~Module() = default;

bool Module::initialize(DataSource* data, vtkSMViewProxy* vtkView, OperatorResult* result)
{
  m_operatorResult = result;
  return initialize(data, vtkView);
}

bool Module::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  m_view = vtkView;
  m_activeDataSource = data;
  d->m_gradientOpacityMap->RemoveAllPoints();
  d->m_transfer2D->SetDimensions(1, 1, 1);
  d->m_transfer2D->AllocateScalars(VTK_FLOAT, 4);

  if (m_view && m_activeDataSource) {
    // FIXME: we're connecting this too many times. Fix it.
    tomviz::convert<pqView*>(vtkView)->connect(
      m_activeDataSource, SIGNAL(dataChanged()), SLOT(render()));
    connect(m_activeDataSource, SIGNAL(dataChanged()), this,
            SIGNAL(dataSourceChanged()));
    connect(m_activeDataSource,
            SIGNAL(displayPositionChanged(double, double, double)),
            SLOT(dataSourceMoved(double, double, double)));
  }
  return (m_view && m_activeDataSource);
}

vtkSMViewProxy* Module::view() const
{
  return m_view;
}

DataSource* Module::dataSource() const
{
  return m_activeDataSource;
}

OperatorResult* Module::operatorResult() const
{
  return m_operatorResult;
}

void Module::addToPanel(QWidget* vtkNotUsed(panel)) {}

void Module::prepareToRemoveFromPanel(QWidget* vtkNotUsed(panel)) {}

void Module::setUseDetachedColorMap(bool val)
{
  m_useDetachedColorMap = val;
  if (isColorMapNeeded() == false) {
    return;
  }

  if (m_useDetachedColorMap) {
    d->m_colorMap = d->detachedColorMap();
    d->m_opacityMap = d->detachedOpacityMap();

    tomviz::rescaleColorMap(d->m_colorMap, dataSource());
    pqCoreUtilities::connect(d->m_colorMap, vtkCommand::ModifiedEvent, this,
                             SLOT(onColorMapChanged()));
  } else {
    d->m_colorMap = nullptr;
    d->m_opacityMap = nullptr;
  }
  updateColorMap();
  emit colorMapChanged();
}

vtkSMProxy* Module::colorMap() const
{
  return useDetachedColorMap() ? d->m_colorMap.GetPointer()
                               : colorMapDataSource()->colorMap();
}

vtkSMProxy* Module::opacityMap() const
{
  Q_ASSERT(d->m_colorMap || !m_useDetachedColorMap);
  return useDetachedColorMap() ? d->m_opacityMap.GetPointer()
                               : dataSource()->opacityMap();
}

vtkPiecewiseFunction* Module::gradientOpacityMap() const
{
  vtkPiecewiseFunction* gof = useDetachedColorMap()
                                ? d->m_gradientOpacityMap.GetPointer()
                                : dataSource()->gradientOpacityMap();

  // Set default values
  const int numPoints = gof->GetSize();
  if (numPoints == 0) {
    vtkColorTransferFunction* lut =
      vtkColorTransferFunction::SafeDownCast(colorMap()->GetClientSideObject());

    double range[2];
    lut->GetRange(range);

    // For gradient magnitude, the volume mapper's fragment shader expects a
    // range of [0, DataMax/4].
    const double maxValue = (range[1] - range[0]) / 4.0;
    gof->AddPoint(0.0, 0.0);
    gof->AddPoint(maxValue, 1.0);
  }

  return gof;
}

vtkImageData* Module::transferFunction2D() const
{
  return useDetachedColorMap() ? d->m_transfer2D.GetPointer()
                               : colorMapDataSource()->transferFunction2D();
}

QJsonObject Module::serialize() const
{
  QJsonObject json;
  QJsonObject props;
  props["visibility"] = visibility();
  if (isColorMapNeeded()) {
    json["useDetachedColorMap"] = m_useDetachedColorMap;
    if (m_useDetachedColorMap) {
      json["colorOpacityMap"] = tomviz::serialize(d->detachedColorMap());
      json["gradientOpacityMap"] = tomviz::serialize(gradientOpacityMap());
    }
  }
  json["properties"] = props;
  return json;
}

bool Module::deserialize(const QJsonObject& json)
{
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    setVisibility(props["visibility"].toBool());
  }

  if (isColorMapNeeded() && json.contains("useDetachedColorMap")) {
    bool useDetachedColorMap = json["useDetachedColorMap"].toBool();
    if (useDetachedColorMap) {
      if (json.contains("colorOpacityMap")) {
        auto colorMap = json["colorOpacityMap"].toObject();
        tomviz::deserialize(d->detachedColorMap(), colorMap);
      }
      if (json.contains("gradientOpacityMap")) {
        auto gradientOpacityMap = json["gradientOpacityMap"].toObject();
        tomviz::deserialize(d->m_gradientOpacityMap, gradientOpacityMap);
      }
    }
    setUseDetachedColorMap(useDetachedColorMap);
  }

  return true;
}

void Module::onColorMapChanged()
{
  emit colorMapChanged();
}

void Module::setTransferMode(const TransferMode mode)
{
  d->m_transferMode = static_cast<Module::TransferMode>(mode);
  this->updateColorMap();

  emit transferModeChanged(static_cast<int>(mode));
}

Module::TransferMode Module::getTransferMode() const
{
  return d->m_transferMode;
}

vtkSmartPointer<vtkDataObject> Module::getDataToExport()
{
  return nullptr;
}

} // end of namespace tomviz
