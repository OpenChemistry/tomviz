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
#include "GradientOpacityWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleContour.h"
#include "ModuleManager.h"
#include "Utilities.h"

#include "vtkChartGradientOpacityEditor.h"

#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkControlPointsItem.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkVector.h>

#include <QVTKOpenGLWidget.h>

#include <pqApplicationCore.h>
#include <pqCoreUtilities.h>
#include <pqPresetDialog.h>
#include <pqRescaleRange.h>
#include <pqResetScalarRangeReaction.h>
#include <pqServerManagerModel.h>
#include <pqView.h>

#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMViewProxy.h>

#include <QColorDialog>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <QDebug>

namespace tomviz {

GradientOpacityWidget::GradientOpacityWidget(QWidget* parent_)
  : QWidget(parent_), m_qvtk(new QVTKOpenGLWidget(this))
{
  // Set up our little chart.
  vtkNew<vtkGenericOpenGLRenderWindow> window_;
  m_qvtk->SetRenderWindow(window_.Get());
  m_histogramView->SetRenderWindow(window_.Get());
  m_histogramView->SetInteractor(m_qvtk->GetInteractor());
  m_histogramView->GetScene()->AddItem(m_histogramColorOpacityEditor.Get());

  // Connect events from the histogram color/opacity editor.
  m_eventLink->Connect(m_histogramColorOpacityEditor.Get(),
                       vtkCommand::EndEvent, this,
                       SLOT(onOpacityFunctionChanged()));

  // Offset margins to align with HistogramWidget
  auto hLayout = new QHBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  hLayout->setContentsMargins(0, 0, 35, 0);

  setLayout(hLayout);
}

GradientOpacityWidget::~GradientOpacityWidget() = default;

void GradientOpacityWidget::setLUT(vtkPiecewiseFunction* gradientOpac,
                                   vtkSMProxy* proxy)
{
  if (m_scalarOpacityFunction) {
    m_eventLink->Disconnect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent,
                            this, SLOT(onOpacityFunctionChanged()));
  }

  m_scalarOpacityFunction = gradientOpac;
  if (!m_scalarOpacityFunction) {
    return;
  }

  m_eventLink->Connect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent, this,
                       SLOT(onOpacityFunctionChanged()));

  // Set the default if it is an empty function
  vtkPVDiscretizableColorTransferFunction* lut =
    vtkPVDiscretizableColorTransferFunction::SafeDownCast(
      proxy->GetClientSideObject());
  const int numPoints = m_scalarOpacityFunction->GetSize();
  if (numPoints == 0) {
    double range[2];
    lut->GetRange(range);
    m_scalarOpacityFunction->AddPoint(range[0], 1.0);
    m_scalarOpacityFunction->AddPoint(range[1], 1.0);
  }
}

void GradientOpacityWidget::setInputData(vtkTable* table, const char* x_,
                                         const char* y_)
{
  m_histogramColorOpacityEditor->SetHistogramInputData(table, x_, y_);
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  m_histogramView->Render();
}

void GradientOpacityWidget::onOpacityFunctionChanged()
{
  auto core = pqApplicationCore::instance();
  auto smModel = core->getServerManagerModel();
  QList<pqView*> views = smModel->findItems<pqView*>();
  foreach (pqView* view, views) {
    view->render();
  }
  m_histogramView->GetRenderWindow()->Render();
}

void GradientOpacityWidget::renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}
}
