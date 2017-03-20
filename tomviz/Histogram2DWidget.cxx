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

#include <vtkAxis.h>
#include <vtkChartHistogram2D.h>
#include <vtkColorTransferFunction.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkDataArray.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkTransferFunctionBoxItem.h>
#include <vtkVector.h>

#include <QVBoxLayout>
#include <QVTKOpenGLWidget.h>


namespace tomviz {

Histogram2DWidget::Histogram2DWidget(QWidget* parent_)
  : QWidget(parent_), m_qvtk(new QVTKOpenGLWidget(this))
{
  // Set up the chart
  vtkNew<vtkGenericOpenGLRenderWindow> window_;
  m_qvtk->SetRenderWindow(window_.Get());
  QSurfaceFormat glFormat = QVTKOpenGLWidget::defaultFormat();
  glFormat.setSamples(8);
  m_qvtk->setFormat(glFormat);
  m_histogramView->SetRenderWindow(window_.Get());
  m_histogramView->SetInteractor(m_qvtk->GetInteractor());
  m_histogramView->GetScene()->AddItem(m_chartHistogram2D.Get());

  vtkAxis* bottomAxis =
    m_chartHistogram2D->GetAxis(vtkAxis::BOTTOM);
  bottomAxis->SetTitle("Scalar Value");
  bottomAxis->SetBehavior(vtkAxis::FIXED);
  bottomAxis->SetVisible(false);
  bottomAxis->SetRange(0, 255);

  vtkAxis* leftAxis =
    m_chartHistogram2D->GetAxis(vtkAxis::LEFT);
  leftAxis->SetTitle("Gradient Magnitude");
  leftAxis->SetBehavior(vtkAxis::FIXED);
  leftAxis->SetVisible(false);
  leftAxis->SetRange(0, 255);

//   Connect events from the histogram color/opacity editor.
//  m_eventLink->Connect(m_chartHistogram2D.Get(),
//                       vtkCommand::EndEvent, this,
//                       SLOT(onOpacityFunctionChanged()));

  // Offset margins to align with HistogramWidget
  auto hLayout = new QVBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  setLayout(hLayout);
}

Histogram2DWidget::~Histogram2DWidget() = default;

void Histogram2DWidget::setInputData(vtkImageData* histogram)
{
  vtkDataArray* arr = histogram->GetPointData()->GetScalars();
  double range[2];
  arr->GetRange(range, 0);
  
  m_chartHistogram2D->SetInputData(histogram);

  // A minimum of 1.0 is used in order to clip off histogram bins with a
  // single occurrence. This is also necessary to enable Log10 scale (required
  // to have min > 0)
  vtkColorTransferFunction* transferFunction = vtkColorTransferFunction::New();
  transferFunction->AddRGBSegment(range[0] + 1.0, 0.0, 0.0, 0.0,
    range[1], 1.0, 1.0, 1.0);
  transferFunction->SetScaleToLog10();
  transferFunction->Build();
  m_chartHistogram2D->SetTransferFunction(transferFunction);
  transferFunction->Delete();

  m_histogramView->Render();
}

void Histogram2DWidget::addTransferFunction(
  vtkSmartPointer<vtkTransferFunctionBoxItem> item)
{
  m_chartHistogram2D->AddItem(item);
}

//void Histogram2DWidget::renderViews()
//{
//  pqView* view =
//    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
//  if (view) {
//    view->render();
//  }
//}
}
