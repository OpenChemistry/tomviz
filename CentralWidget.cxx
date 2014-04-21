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

#include <QtDebug>

namespace TEM
{

class CentralWidget::CWInternals
{
public:
  Ui::CentralWidget Ui;
};

//-----------------------------------------------------------------------------
CentralWidget::CentralWidget(QWidget* parentObject, Qt::WindowFlags wflags)
  : Superclass(parentObject, wflags),
    Internals(new CentralWidget::CWInternals())
{
  this->Internals->Ui.setupUi(this);

  // Set up our little chart.
  this->Histogram
      ->SetInteractor(this->Internals->Ui.histogramWidget->GetInteractor());
  this->Internals->Ui.histogramWidget
      ->SetRenderWindow(this->Histogram->GetRenderWindow());
  vtkChartXY* chart = this->Chart.Get();
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
}

//-----------------------------------------------------------------------------
CentralWidget::~CentralWidget()
{
}

template<typename T>
void CalculateHistogram(T *values, unsigned int n, float min, int *pops,
                        float inc, int numberOfBins)
{
  for (unsigned int j = 0; j < n; ++j)
    {
    int index = static_cast<int>((*values - min) / inc);
    if (index < 0 || index > 200)
      cout << "Index of " << index << " for value of " << *values << endl;
    if (index >= numberOfBins)
      {
      index = numberOfBins - 1;
      }
    ++pops[index];
    ++values;
    }
}

//-----------------------------------------------------------------------------
// This is just here for now - quick and dirty historgram calculations...
bool PopulateHistogram(vtkImageData *input, vtkTable *output)
{
  // The output table will have the twice the number of columns, they will be
  // the x and y for input column. This is the bin centers, and the population.
  double minmax[2] = { 0.0, 0.0 };
  double numberOfBins = 200;
  // The bin values are the centers, extending +/- half an inc either side
  input->GetScalarRange(minmax);
  if (minmax[0] == minmax[1])
    {
    minmax[1] = minmax[0] + 1.0;
    }
  //minmax[1] = 40;
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
          CalculateHistogram(reinterpret_cast<VTK_TT *>(input->GetPointData()->GetScalars()->GetVoidPointer(0)),
                             input->GetPointData()->GetScalars()->GetNumberOfTuples(),
                             minmax[0], pops, inc, numberOfBins));
    default:
      cout << "UpdateFromFile: Unknown data type" << endl;
    }
  double total = 0;
  for (int i = 0; i < numberOfBins; ++i)
    total += pops[i];

  output->AddColumn(extents.GetPointer());
  output->AddColumn(populations.GetPointer());
  return true;
}

//-----------------------------------------------------------------------------
void CentralWidget::setDataSource(vtkSMSourceProxy* source)
{
  // Get the actual data source, build a histogram out of it.
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(source->GetClientSideObject());
  vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  // Calculate a histogram.
  vtkNew<vtkTable> table;
  PopulateHistogram(data, table.Get());
  this->Chart->ClearPlots();
  vtkPlot *plot = this->Chart->AddPlot(vtkChart::BAR);
  plot->SetInputData(table.Get(), "image_extents", "image_pops");
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
}

} // end of namespace TEM
