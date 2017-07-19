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
#include "Histogram2DWidget.h"

#include "QVTKGLWidget.h"

#include <vtkAxis.h>
#include <vtkChartTransfer2DEditor.h>
#include <vtkColorTransferFunction.h>
#include <vtkContextMouseEvent.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkDataArray.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkTransferFunctionBoxItem.h>
#include <vtkVector.h>

#include <pqApplicationCore.h>
#include <pqServerManagerModel.h>
#include <pqView.h>

#include <QList>
#include <QVBoxLayout>

namespace tomviz {

Histogram2DWidget::Histogram2DWidget(QWidget* parent_)
  : QWidget(parent_), m_qvtk(new QVTKGLWidget(this))
{
  // Set up the chart
  m_histogramView->SetRenderWindow(m_qvtk->GetRenderWindow());
  m_histogramView->SetInteractor(m_qvtk->GetInteractor());
  m_histogramView->GetScene()->AddItem(m_chartHistogram2D.Get());

  m_chartHistogram2D->SetRenderEmpty(true);
  m_chartHistogram2D->SetAutoAxes(false);
  m_chartHistogram2D->ZoomWithMouseWheelOff();
  m_chartHistogram2D->SetActionToButton(vtkChart::PAN, -1);

  auto axis = m_chartHistogram2D->GetAxis(vtkAxis::BOTTOM);
  axis->SetTitle("Scalar Value");
  axis->SetBehavior(vtkAxis::FIXED);
  axis->SetRange(0, 255);

  axis = m_chartHistogram2D->GetAxis(vtkAxis::LEFT);
  axis->SetTitle("Gradient Magnitude");
  axis->SetBehavior(vtkAxis::FIXED);
  axis->SetRange(0, 255);

  m_chartHistogram2D->GetAxis(vtkAxis::LEFT)->SetBehavior(vtkAxis::FIXED);
  m_chartHistogram2D->GetAxis(vtkAxis::RIGHT)->SetBehavior(vtkAxis::FIXED);
  m_chartHistogram2D->GetAxis(vtkAxis::BOTTOM)->SetBehavior(vtkAxis::FIXED);
  m_chartHistogram2D->GetAxis(vtkAxis::TOP)->SetBehavior(vtkAxis::FIXED);

  m_eventLink->Connect(m_chartHistogram2D.Get(), vtkCommand::EndEvent, this,
                       SLOT(onTransfer2DChanged()));

  // Offset margins to align with HistogramWidget
  auto hLayout = new QVBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  setLayout(hLayout);
}

Histogram2DWidget::~Histogram2DWidget() = default;

void Histogram2DWidget::setHistogram(vtkImageData* histogram)
{
  vtkDataArray* arr = histogram->GetPointData()->GetScalars();
  double range[2];
  arr->GetRange(range, 0);

  m_chartHistogram2D->SetInputData(histogram);

  // A minimum of 1.0 is used in order to clip off histogram bins with a
  // single occurrence. This is also necessary to enable Log10 scale (required
  // to have min > 0)
  vtkNew<vtkColorTransferFunction> transferFunction;
  transferFunction->AddRGBSegment(range[0] + 1.0, 0.0, 0.0, 0.0, range[1], 1.0,
                                  1.0, 1.0);

  transferFunction->SetScaleToLog10();
  transferFunction->Build();
  m_chartHistogram2D->SetTransferFunction(transferFunction.Get());

  m_histogramView->Render();
}

void Histogram2DWidget::addFunctionItem(
  vtkSmartPointer<vtkTransferFunctionBoxItem> item)
{
  m_chartHistogram2D->AddFunction(item);
}

void Histogram2DWidget::setTransfer2D(vtkImageData* transfer2D)
{
  m_chartHistogram2D->SetTransfer2D(transfer2D);
  m_histogramView->Render();
}

void Histogram2DWidget::onTransfer2DChanged()
{
  auto core = pqApplicationCore::instance();
  auto smModel = core->getServerManagerModel();
  QList<pqView*> views = smModel->findItems<pqView*>();
  foreach (pqView* view, views) {
    view->render();
  }

  m_histogramView->GetRenderWindow()->Render();
}

void Histogram2DWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  m_chartHistogram2D->GenerateTransfer2D();
}
}
