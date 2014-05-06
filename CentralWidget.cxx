/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "CentralWidget.h"
#include "ui_CentralWidget.h"

#include <vtkSMSourceProxy.h>

#include <vtkContextView.h>
#include <vtkContextScene.h>
#include <vtkChartXY.h>
#include <vtkAxis.h>

#include <vtkTrivialProducer.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h>
#include <vtkTable.h>
#include <vtkMathUtilities.h>
#include <vtkPlotBar.h>
#include <vtkTransform2D.h>
#include <vtkContextMouseEvent.h>
#include <vtkObjectFactory.h>
#include <vtkEventQtSlotConnect.h>

#include <QtDebug>
#include <QThread>

#include "ComputeHistogram.h"

namespace TEM
{

//-----------------------------------------------------------------------------
// This is just here for now - quick and dirty historgram calculations...
void PopulateHistogram(vtkImageData *input, vtkTable *output)
{
  // The output table will have the twice the number of columns, they will be
  // the x and y for input column. This is the bin centers, and the population.
  double minmax[2] = { 0.0, 0.0 };
  const int numberOfBins = 200;

  // The bin values are the centers, extending +/- half an inc either side
  switch (input->GetScalarType())
    {
    vtkTemplateMacro(
          TEM::GetScalarRange(reinterpret_cast<VTK_TT *>(input->GetPointData()->GetScalars()->GetVoidPointer(0)),
                         input->GetPointData()->GetScalars()->GetNumberOfTuples(),
                         minmax));
    default:
      break;
    }
  if (minmax[0] == minmax[1])
    {
    minmax[1] = minmax[0] + 1.0;
    }

  double inc = (minmax[1] - minmax[0]) / numberOfBins;
  double halfInc = inc / 2.0;
  vtkSmartPointer<vtkFloatArray> extents =
      vtkFloatArray::SafeDownCast(
        output->GetColumnByName(vtkStdString("image_extents").c_str()));
  if (!extents)
    {
    extents = vtkSmartPointer<vtkFloatArray>::New();
    extents->SetName(vtkStdString("image_extents").c_str());
    }
  extents->SetNumberOfTuples(numberOfBins);
  double min = minmax[0] + halfInc;
  for (int j = 0; j < numberOfBins; ++j)
    {
    extents->SetValue(j, min + j * inc);
    }
  vtkSmartPointer<vtkIntArray> populations =
      vtkIntArray::SafeDownCast(
        output->GetColumnByName(vtkStdString("image_pops").c_str()));
  if (!populations)
    {
    populations = vtkSmartPointer<vtkIntArray>::New();
    populations->SetName(vtkStdString("image_pops").c_str());
    }
  populations->SetNumberOfTuples(numberOfBins);
  int *pops = static_cast<int *>(populations->GetVoidPointer(0));
  for (int k = 0; k < numberOfBins; ++k)
    {
    pops[k] = 0;
    }

  switch (input->GetScalarType())
    {
    vtkTemplateMacro(
          TEM::CalculateHistogram(reinterpret_cast<VTK_TT *>(input->GetPointData()->GetScalars()->GetVoidPointer(0)),
                             input->GetPointData()->GetScalars()->GetNumberOfTuples(),
                             minmax[0], pops, inc, numberOfBins));
    default:
      cout << "UpdateFromFile: Unknown data type" << endl;
    }

#ifndef NDEBUG
  vtkIdType total = 0;
  for (int i = 0; i < numberOfBins; ++i)
    total += pops[i];
  assert(total == input->GetPointData()->GetScalars()->GetNumberOfTuples());
#endif

  output->AddColumn(extents.GetPointer());
  output->AddColumn(populations.GetPointer());
}

// Quick background thread for the histogram calculation.
class HistogramWorker : public QThread
{
  Q_OBJECT

  void run();

public:
  HistogramWorker(QObject *p = 0) : QThread(p) {}

  vtkSmartPointer<vtkImageData> input;
  vtkSmartPointer<vtkTable> output;
};

void HistogramWorker::run()
{
  if (input && output)
    {
    PopulateHistogram(input.Get(), output.Get());
    }
}

class CentralWidget::CWInternals
{
public:
  Ui::CentralWidget Ui;
};

class vtkChartHistogram : public vtkChartXY
{
public:
  static vtkChartHistogram * New();

  bool MouseDoubleClickEvent(const vtkContextMouseEvent &mouse);

  vtkNew<vtkTransform2D> Transform;
  double PositionX;
};

vtkStandardNewMacro(vtkChartHistogram)

bool vtkChartHistogram::MouseDoubleClickEvent(const vtkContextMouseEvent &m)
{
  // Determine the location of the click, and emit something we can listen to!
  vtkPlotBar *histo = 0;
  if (this->GetNumberOfPlots() == 1)
    {
    histo = vtkPlotBar::SafeDownCast(this->GetPlot(0));
    }
  if (!histo)
    {
    return false;
    }
  if (this->Transform->GetMTime() < histo->GetMTime())
    {
    this->CalculateUnscaledPlotTransform(histo->GetXAxis(), histo->GetYAxis(),
                                         this->Transform.Get());
    }
  vtkVector2f pos;
  this->Transform->InverseTransformPoints(m.ScenePos.GetData(), pos.GetData(),
                                          1);
  this->PositionX = pos.GetX();
  this->InvokeEvent(vtkCommand::CursorChangedEvent);
  return true;
}

//-----------------------------------------------------------------------------
CentralWidget::CentralWidget(QWidget* parentObject, Qt::WindowFlags wflags)
  : Superclass(parentObject, wflags),
    Internals(new CentralWidget::CWInternals()),
    Worker(NULL)
{
  this->Internals->Ui.setupUi(this);

  // Set up our little chart.
  this->Histogram
      ->SetInteractor(this->Internals->Ui.histogramWidget->GetInteractor());
  this->Internals->Ui.histogramWidget
      ->SetRenderWindow(this->Histogram->GetRenderWindow());
  vtkChartHistogram* chart = this->Chart.Get();
  this->Histogram->GetScene()->AddItem(chart);
  chart->SetBarWidthFraction(0.95);
  chart->SetRenderEmpty(true);
  chart->SetAutoAxes(false);
  chart->GetAxis(vtkAxis::LEFT)->SetTitle("");
  chart->GetAxis(vtkAxis::BOTTOM)->SetTitle("");
  chart->GetAxis(vtkAxis::LEFT)->SetBehavior(vtkAxis::FIXED);
  chart->GetAxis(vtkAxis::LEFT)->SetRange(0.0001, 10);
  chart->GetAxis(vtkAxis::LEFT)->SetMinimumLimit(1);
  chart->GetAxis(vtkAxis::LEFT)->SetLogScale(true);

  this->EventLink->Connect(chart, vtkCommand::CursorChangedEvent, this,
                           SLOT(histogramClicked(vtkObject*)));
}

//-----------------------------------------------------------------------------
CentralWidget::~CentralWidget()
{
}

//-----------------------------------------------------------------------------
void CentralWidget::setDataSource(vtkSMSourceProxy* source)
{
  // Get the actual data source, build a histogram out of it.
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(source->GetClientSideObject());
  vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  // Calculate a histogram.
  vtkNew<vtkTable> table;

  if (!this->Worker)
    {
    this->Worker = new HistogramWorker(this);
    connect(this->Worker, SIGNAL(finished()), SLOT(histogramReady()));
    }
  else if (this->Worker->isRunning())
    {
    // FIXME: Queue, abort, something.
    qDebug() << "Worker already running, skipping this one.";
    return;
    }
  this->Worker->input = data;
  this->Worker->output = table.Get();
  this->Worker->start();
}

void CentralWidget::histogramReady()
{
  if (!this->Worker || !this->Worker->input || !this->Worker->output)
    return;

  vtkTable *table = this->Worker->output.Get();
  this->Chart->ClearPlots();
  vtkPlot *plot = this->Chart->AddPlot(vtkChart::BAR);
  plot->SetInputData(table, "image_extents", "image_pops");
  vtkDataArray *arr =
      vtkDataArray::SafeDownCast(table->GetColumnByName("image_pops"));
  if (arr)
    {
    double max = log10(arr->GetRange()[1]);
    vtkAxis *axis = this->Chart->GetAxis(vtkAxis::LEFT);
    axis->SetUnscaledMinimum(1.0);
    axis->SetMaximumLimit(max + 2.0);
    axis->SetMaximum(static_cast<int>(max) + 1.0);
    }

  this->Worker->input = NULL;
  this->Worker->output = NULL;
}

void CentralWidget::histogramClicked(vtkObject *caller)
{
  qDebug() << "Histogram clicked at" << this->Chart->PositionX
           << "making this a great spot to ask for an isosurface at value"
           << this->Chart->PositionX;
}

} // end of namespace TEM

#include "CentralWidget.moc"
