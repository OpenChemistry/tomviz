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
#include "CentralWidget.h"
#include "ui_CentralWidget.h"

#include <pqView.h>
#include <vtkAxis.h>
#include <vtkChartXY.h>
#include <vtkContextMouseEvent.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMathUtilities.h>
#include <vtkObjectFactory.h>
#include <vtkPlotBar.h>
#include <vtkPointData.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTable.h>
#include <vtkTransform2D.h>
#include <vtkTrivialProducer.h>
#include <vtkContext2D.h>
#include <vtkPen.h>
#include <vtkScalarsToColors.h>

#include <QtDebug>
#include <QThread>

#include "ActiveObjects.h"
#include "ComputeHistogram.h"
#include "DataSource.h"
#include "ModuleContour.h"
#include "ModuleManager.h"
#include "Utilities.h"

#ifdef DAX_DEVICE_ADAPTER
# include "dax/ModuleStreamingContour.h"
#endif

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)
Q_DECLARE_METATYPE(vtkSmartPointer<vtkTable>)

namespace tomviz
{

//-----------------------------------------------------------------------------
// This is just here for now - quick and dirty historgram calculations...
void PopulateHistogram(vtkImageData *input, vtkTable *output)
{
  // The output table will have the twice the number of columns, they will be
  // the x and y for input column. This is the bin centers, and the population.
  double minmax[2] = { 0.0, 0.0 };
  const int numberOfBins = 256;

  // The bin values are the centers, extending +/- half an inc either side
  switch (input->GetScalarType())
    {
    vtkTemplateMacro(
          tomviz::GetScalarRange(reinterpret_cast<VTK_TT *>(input->GetPointData()->GetScalars()->GetVoidPointer(0)),
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
          tomviz::CalculateHistogram(reinterpret_cast<VTK_TT *>(input->GetPointData()->GetScalars()->GetVoidPointer(0)),
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

// This is a QObject that will be owned by the background thread
// and use signals/slots to create histograms
class HistogramMaker : public QObject
{
  Q_OBJECT

  void run();

public:
  HistogramMaker(QObject *p = 0) : QObject(p) {}

public slots:
  void makeHistogram(vtkSmartPointer<vtkImageData> input,
                     vtkSmartPointer<vtkTable> output);

signals:
  void histogramDone(vtkSmartPointer<vtkImageData> image,
                     vtkSmartPointer<vtkTable> output);
};

void HistogramMaker::makeHistogram(vtkSmartPointer<vtkImageData> input,
                                   vtkSmartPointer<vtkTable> output)
{
  // make the histogram and notify observers (the main thread) that it
  // is done.
  if (input && output)
    {
    PopulateHistogram(input.Get(), output.Get());
    }
  emit histogramDone(input, output);
}

class CentralWidget::CWInternals
{
public:
  Ui::CentralWidget Ui;
};

class vtkHistogramMarker : public vtkPlot
{
public:
  static vtkHistogramMarker * New();
  double PositionX;

  bool Paint(vtkContext2D *painter)
  {
    vtkNew<vtkPen> pen;
    pen->SetColor(255, 0, 0, 255);
    pen->SetWidth(2.0);
    painter->ApplyPen(pen.Get());
    painter->DrawLine(PositionX, 0, PositionX, 1e9);
    return true;
  }
};
vtkStandardNewMacro(vtkHistogramMarker)

class vtkChartHistogram : public vtkChartXY
{
public:
  static vtkChartHistogram * New();

  bool MouseDoubleClickEvent(const vtkContextMouseEvent &mouse);

  vtkNew<vtkTransform2D> Transform;
  double PositionX;
  vtkNew<vtkHistogramMarker> Marker;
};

vtkStandardNewMacro(vtkChartHistogram)

bool vtkChartHistogram::MouseDoubleClickEvent(const vtkContextMouseEvent &m)
{
  // Determine the location of the click, and emit something we can listen to!
  vtkPlotBar *histo = 0;
  if (this->GetNumberOfPlots() > 0)
    {
    histo = vtkPlotBar::SafeDownCast(this->GetPlot(0));
    }
  if (!histo)
    {
    return false;
    }
  this->CalculateUnscaledPlotTransform(histo->GetXAxis(), histo->GetYAxis(),
                                       this->Transform.Get());
  vtkVector2f pos;
  this->Transform->InverseTransformPoints(m.GetScenePos().GetData(), pos.GetData(),
                                          1);
  this->PositionX = pos.GetX();
  this->Marker->PositionX = this->PositionX;
  this->Marker->Modified();
  this->Scene->SetDirty(true);
  if (this->GetNumberOfPlots() == 1)
    {
    // Work around a bug in the charts - ensure corner is invalid for the plot.
    this->Marker->SetXAxis(NULL);
    this->Marker->SetYAxis(NULL);
    this->AddPlot(this->Marker.Get());
    }
  this->InvokeEvent(vtkCommand::CursorChangedEvent);
  return true;
}

//-----------------------------------------------------------------------------
CentralWidget::CentralWidget(QWidget* parentObject, Qt::WindowFlags wflags)
  : Superclass(parentObject, wflags),
    Internals(new CentralWidget::CWInternals()),
    HistogramGen(new HistogramMaker),
    Worker(new QThread(this))
{
  this->Internals->Ui.setupUi(this);

  qRegisterMetaType<vtkSmartPointer<vtkImageData> >();
  qRegisterMetaType<vtkSmartPointer<vtkTable> >();

  QList<int> sizes;
  sizes << 200 << 200;
  this->Internals->Ui.splitter->setSizes(sizes);
  this->Internals->Ui.splitter->setStretchFactor(0, 0);
  this->Internals->Ui.splitter->setStretchFactor(1, 1);

  // Set up our little chart.
  this->Histogram
      ->SetInteractor(this->Internals->Ui.histogramWidget->GetInteractor());
  this->Internals->Ui.histogramWidget
      ->SetRenderWindow(this->Histogram->GetRenderWindow());
  vtkChartHistogram* chart = this->Chart.Get();
  this->Histogram->GetScene()->AddItem(chart);
  chart->SetBarWidthFraction(1.0);
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

  // start the worker thread and give it ownership of the HistogramMaker
  // object.  Also connect the HistogramMaker's signal to the histogramReady
  // slot on this object.  This slot will be called on the GUI thread when the
  // histogram has been finished on the background thread.
  this->Worker->start();
  this->HistogramGen->moveToThread(this->Worker);
  this->connect(this->HistogramGen,
     SIGNAL(histogramDone(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>)),
     SLOT(histogramReady(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>)));
}

//-----------------------------------------------------------------------------
CentralWidget::~CentralWidget()
{
  // disconnect all signals/slots
  QObject::disconnect(this->HistogramGen, NULL, NULL, NULL);
  // when the HistogramMaker is deleted, kill the background thread
  QObject::connect(this->HistogramGen, SIGNAL(destroyed()),
                   this->Worker, SLOT(quit()));
  // I can't remember if deleteLater must be called on the owning thread
  // play it safe and let the owning thread call it.
  QMetaObject::invokeMethod(this->HistogramGen, "deleteLater");
  // Wait for the background thread to clean up the object and quit
  while (this->Worker->isRunning())
    {
    QCoreApplication::processEvents();
    }
}

//-----------------------------------------------------------------------------
void CentralWidget::setDataSource(DataSource* source)
{
  if (this->ADataSource)
    {
    this->disconnect(this->ADataSource);
    }
  this->ADataSource = source;
  if (source)
    {
    this->connect(source, SIGNAL(dataChanged()), SLOT(refreshHistogram()));
    }

  // Whenever the data source changes clear the plot, and then populate when
  // ready (or use the cached histogram values.
  this->Chart->ClearPlots();

  if (!source)
    {
    return;
    }

  // Get the actual data source, build a histogram out of it.
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  vtkImageData *image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  // Check our cache, and use that if appopriate (or update it).
  if (this->HistogramCache.contains(image))
    {
    vtkTable *cachedTable = this->HistogramCache[image];
    if (cachedTable->GetMTime() > image->GetMTime())
      {
      this->setHistogramTable(cachedTable);
      return;
      }
    else
      {
      // Need to recalculate, clear the plots, and remove the cached data.
      this->Chart->ClearPlots();
      this->HistogramCache.remove(image);
      }
    }

  // Get the current color map
  vtkScalarsToColors *lut =
      vtkScalarsToColors::SafeDownCast(source->colorMap()->GetClientSideObject());
  if (lut)
    {
    this->LUT = lut;
    }
  else
    {
    this->LUT = NULL;
    }

  // Calculate a histogram.
  vtkSmartPointer<vtkTable> table =
    vtkSmartPointer<vtkTable>::New();
  this->HistogramCache[image] = table.Get();
  vtkSmartPointer<vtkImageData> const imageSP = image;

  // This fakes a Qt signal to the background thread (without exposing the
  // class internals as a signal).  The background thread will then call
  // makeHistogram on the HistogramMaker object with the parameters we
  // gave here.
  QMetaObject::invokeMethod(this->HistogramGen,
     "makeHistogram",
     Q_ARG(vtkSmartPointer<vtkImageData>, imageSP),
     Q_ARG(vtkSmartPointer<vtkTable>, table));
}

void CentralWidget::refreshHistogram()
{
  this->setDataSource(this->ADataSource);
}

void CentralWidget::histogramReady(vtkSmartPointer<vtkImageData> input,
                                   vtkSmartPointer<vtkTable> output)
{
  if (!input || !output)
    return;

  // If we no longer have an active datasource, ignore showing the histogram
  // since the data has been deleted
  if (!this->ADataSource)
    return;

  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    this->ADataSource->producer()->GetClientSideObject());
  vtkImageData *image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  // The current dataset has changed since the histogram was requested,
  // ignore this histogram and wait for the next one queued...
  if (image != input.Get())
    return;

  this->setHistogramTable(output.Get());
}

void CentralWidget::histogramClicked(vtkObject *)
{
  //qDebug() << "Histogram clicked at" << this->Chart->PositionX
  //         << "making this a great spot to ask for an isosurface at value"
  //         << this->Chart->PositionX;
  Q_ASSERT(this->ADataSource);

  vtkSMViewProxy* view = ActiveObjects::instance().activeView();
  if (!view)
    {
    return;
    }

  // Use active ModuleContour is possible. Otherwise, find the first existing
  // ModuleContour instance or just create a new one, if none exists.
#ifdef DAX_DEVICE_ADAPTER
  typedef ModuleStreamingContour ModuleContourType;
#else
  typedef ModuleContour ModuleContourType;
#endif

  ModuleContourType* contour = qobject_cast<ModuleContourType*>(
    ActiveObjects::instance().activeModule());
  if (!contour)
    {
    QList<ModuleContourType*> contours =
      ModuleManager::instance().findModules<ModuleContourType*>(this->ADataSource, view);
    if (contours.size() == 0)
      {
      contour = qobject_cast<ModuleContourType*>(ModuleManager::instance().createAndAddModule(
          "Contour", this->ADataSource, view));
      }
    else
      {
      contour = contours[0];
      }
    ActiveObjects::instance().setActiveModule(contour);
    }
  Q_ASSERT(contour);
  contour->setIsoValue(this->Chart->PositionX);
  tomviz::convert<pqView*>(view)->render();
}

void CentralWidget::setHistogramTable(vtkTable *table)
{
  vtkDataArray *arr =
      vtkDataArray::SafeDownCast(table->GetColumnByName("image_pops"));
  if (!arr)
    {
    return;
    }

  this->Chart->ClearPlots();

  vtkNew<vtkPlotBar> plot;

  this->Chart->AddPlot(plot.Get());
  plot->SetInputData(table, "image_extents", "image_pops");
  plot->SetColor(0, 0, 255, 255);
  plot->GetPen()->SetLineType(vtkPen::NO_PEN);

  double max = log10(arr->GetRange()[1]);
  vtkAxis *leftAxis = this->Chart->GetAxis(vtkAxis::LEFT);
  leftAxis->SetUnscaledMinimum(1.0);
  leftAxis->SetMaximumLimit(max + 2.0);
  leftAxis->SetMaximum(static_cast<int>(max) + 1.0);

  arr = vtkDataArray::SafeDownCast(table->GetColumnByName("image_extents"));
  if (arr && arr->GetNumberOfTuples() > 2)
    {
    double range[2];
    arr->GetRange(range);
    double halfInc = (arr->GetTuple1(1) - arr->GetTuple1(0)) / 2.0;
    vtkAxis *bottomAxis = this->Chart->GetAxis(vtkAxis::BOTTOM);
    bottomAxis->SetBehavior(vtkAxis::FIXED);
    bottomAxis->SetRange(range[0] - halfInc , range[1] + halfInc);
    }

  if (this->LUT)
    {
    plot->ScalarVisibilityOn();
    plot->SetLookupTable(this->LUT);
    plot->SelectColorArray("image_extents");
    }
}

} // end of namespace tomviz

#include "CentralWidget.moc"
