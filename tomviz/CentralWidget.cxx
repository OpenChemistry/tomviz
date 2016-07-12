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

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkTable.h>
#include <vtkTrivialProducer.h>
#include <vtkVector.h>

#include <QThread>
#include <QTimer>

#include "ComputeHistogram.h"
#include "DataSource.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Utilities.h"

#ifdef DAX_DEVICE_ADAPTER
# include "dax/ModuleStreamingContour.h"
#endif

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)
Q_DECLARE_METATYPE(vtkSmartPointer<vtkTable>)

namespace tomviz
{

// This is just here for now - quick and dirty historgram calculations...
void PopulateHistogram(vtkImageData *input, vtkTable *output)
{
  // The output table will have the twice the number of columns, they will be
  // the x and y for input column. This is the bin centers, and the population.
  double minmax[2] = { 0.0, 0.0 };
  const int numberOfBins = 256;

  // Keep the array we are working on around even if the user shallow copies
  // over the input image data by incrementing the reference count here.
  vtkSmartPointer<vtkDataArray> arrayPtr = input->GetPointData()->GetScalars();


  // The bin values are the centers, extending +/- half an inc either side
  switch (arrayPtr->GetDataType())
  {
    vtkTemplateMacro(
          tomviz::GetScalarRange(reinterpret_cast<VTK_TT *>(arrayPtr->GetVoidPointer(0)),
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
  int invalid = 0;

  switch (arrayPtr->GetDataType())
  {
    vtkTemplateMacro(
          tomviz::CalculateHistogram(reinterpret_cast<VTK_TT *>(arrayPtr->GetVoidPointer(0)),
                             arrayPtr->GetNumberOfTuples(),
                             minmax[0], pops, inc, numberOfBins, invalid));
    default:
      cout << "UpdateFromFile: Unknown data type" << endl;
  }

#ifndef NDEBUG
  vtkIdType total = invalid;
  for (int i = 0; i < numberOfBins; ++i)
    total += pops[i];
  assert(total == arrayPtr->GetNumberOfTuples());
#endif
  if (invalid)
  {
    cout << "Warning: NaN or infinite value in dataset" << endl;
  }

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
  HistogramMaker(QObject *p = nullptr) : QObject(p) {}

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
  QTimer Timer;
};

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

  this->connect(this->Internals->Ui.histogramWidget, SIGNAL(colorMapUpdated()),
                SLOT(onColorMapUpdated()));

  // Start the worker thread and give it ownership of the HistogramMaker
  // object. Also connect the HistogramMaker's signal to the histogramReady
  // slot on this object. This slot will be called on the GUI thread when the
  // histogram has been finished on the background thread.
  this->Worker->start();
  this->HistogramGen->moveToThread(this->Worker);
  this->connect(this->HistogramGen,
     SIGNAL(histogramDone(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>)),
     SLOT(histogramReady(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>)));
  this->Internals->Timer.setInterval(200);
  this->Internals->Timer.setSingleShot(true);
  this->connect(&this->Internals->Timer, SIGNAL(timeout()), SLOT(refreshHistogram()));
  this->layout()->setMargin(0);
  this->layout()->setSpacing(0);

  this->LUT = nullptr;
}

CentralWidget::~CentralWidget()
{
  // disconnect all signals/slots
  QObject::disconnect(this->HistogramGen, nullptr, nullptr, nullptr);
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

void CentralWidget::setActiveDataSource(DataSource* source)
{
  if (this->AModule)
  {
    this->AModule->disconnect(this);
    this->AModule = nullptr;
  }
  this->setDataSource(source);
}

void CentralWidget::setActiveModule(Module* module)
{
  if (this->AModule)
  {
    this->AModule->disconnect(this);
  }
  this->AModule = module;
  if (this->AModule)
  {
    this->connect(this->AModule, SIGNAL(colorMapChanged()),
                  SLOT(onDataSourceChanged()));
    this->setDataSource(module->dataSource());
  }
  else
  {
    this->setDataSource(nullptr);
  }
}

void CentralWidget::setDataSource(DataSource* source)
{
  if (this->ADataSource)
  {
    this->ADataSource->disconnect(this);
  }
  this->ADataSource = source;
  if (source)
  {
    this->connect(source, SIGNAL(dataChanged()), SLOT(onDataSourceChanged()));
  }

  if (!source)
  {
    return;
  }

  // Get the actual data source, build a histogram out of it.
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(
    source->producer()->GetClientSideObject());
  vtkImageData *image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  if (image->GetPointData()->GetScalars() == nullptr)
  {
    return;
  }

  // Get the current color map
  vtkPVDiscretizableColorTransferFunction *lut;
  if (this->AModule)
  {
    this->Internals->Ui.histogramWidget->setLUTProxy(this->AModule->colorMap());
    vtkObjectBase* colorMapObject = this->AModule->colorMap()->GetClientSideObject();
    lut = vtkPVDiscretizableColorTransferFunction::SafeDownCast(colorMapObject);
  }
  else
  {
    this->Internals->Ui.histogramWidget->setLUTProxy(source->colorMap());
    vtkObjectBase* colorMapObject = source->colorMap()->GetClientSideObject();
    lut = vtkPVDiscretizableColorTransferFunction::SafeDownCast(colorMapObject);
  }
  if (lut)
  {
    this->LUT = lut;
  }
  else
  {
    this->LUT = nullptr;
  }

  if (this->LUT)
  {
    this->Internals->Ui.histogramWidget->setLUT(this->LUT);
  }

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
      this->HistogramCache.remove(image);
    }
  }

  // Calculate a histogram.
  vtkSmartPointer<vtkTable> table = vtkSmartPointer<vtkTable>::New();
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

void CentralWidget::onColorMapUpdated()
{
  this->onDataSourceChanged();
}

void CentralWidget::onDataSourceChanged()
{
  // This starts/restarts the internal timer so that several events occurring
  // within a few milliseconds of each other only result in one call to
  // refreshHistogram()
  this->Internals->Timer.start();
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

void CentralWidget::setHistogramTable(vtkTable *table)
{
  vtkDataArray *arr =
      vtkDataArray::SafeDownCast(table->GetColumnByName("image_pops"));
  if (!arr)
  {
    return;
  }

  this->Internals->Ui.histogramWidget->setInputData(table,
                                                    "image_extents",
                                                    "image_pops");
}

} // end of namespace tomviz

#include "CentralWidget.moc"
