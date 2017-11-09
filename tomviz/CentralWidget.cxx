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
#include <vtkPNGWriter.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkTable.h>
#include <vtkTransferFunctionBoxItem.h>
#include <vtkTrivialProducer.h>
#include <vtkUnsignedShortArray.h>
#include <vtkVector.h>

#include <vtkPVDiscretizableColorTransferFunction.h>

#include <QModelIndex>
#include <QThread>
#include <QTimer>

#include "AbstractDataModel.h"
#include "ComputeHistogram.h"
#include "DataSource.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Utilities.h"

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)
Q_DECLARE_METATYPE(vtkSmartPointer<vtkTable>)

namespace tomviz {

// This is just here for now - quick and dirty historgram calculations...
void PopulateHistogram(vtkImageData* input, vtkTable* output)
{
  // The output table will have the twice the number of columns, they will be
  // the x and y for input column. This is the bin centers, and the population.
  double minmax[2] = { 0.0, 0.0 };

  // This number of bins in the 2D histogram will also be used as the number of
  // bins in the 2D transfer function for X (scalar value) and Y (gradient mag.)
  const int numberOfBins = 256;

  // Keep the array we are working on around even if the user shallow copies
  // over the input image data by incrementing the reference count here.
  vtkSmartPointer<vtkDataArray> arrayPtr = input->GetPointData()->GetScalars();

  // The bin values are the centers, extending +/- half an inc either side
  arrayPtr->GetFiniteRange(minmax, -1);
  if (minmax[0] == minmax[1]) {
    minmax[1] = minmax[0] + 1.0;
  }

  double inc = (minmax[1] - minmax[0]) / numberOfBins;
  double halfInc = inc / 2.0;
  vtkSmartPointer<vtkFloatArray> extents = vtkFloatArray::SafeDownCast(
    output->GetColumnByName(vtkStdString("image_extents").c_str()));
  if (!extents) {
    extents = vtkSmartPointer<vtkFloatArray>::New();
    extents->SetName(vtkStdString("image_extents").c_str());
  }
  extents->SetNumberOfTuples(numberOfBins);
  double min = minmax[0] + halfInc;
  for (int j = 0; j < numberOfBins; ++j) {
    extents->SetValue(j, min + j * inc);
  }
  vtkSmartPointer<vtkIntArray> populations = vtkIntArray::SafeDownCast(
    output->GetColumnByName(vtkStdString("image_pops").c_str()));
  if (!populations) {
    populations = vtkSmartPointer<vtkIntArray>::New();
    populations->SetName(vtkStdString("image_pops").c_str());
  }
  populations->SetNumberOfTuples(numberOfBins);
  auto pops = static_cast<int*>(populations->GetVoidPointer(0));
  for (int k = 0; k < numberOfBins; ++k) {
    pops[k] = 0;
  }
  int invalid = 0;

  switch (arrayPtr->GetDataType()) {
    vtkTemplateMacro(tomviz::CalculateHistogram(
      reinterpret_cast<VTK_TT*>(arrayPtr->GetVoidPointer(0)),
      arrayPtr->GetNumberOfTuples(), arrayPtr->GetNumberOfComponents(),
      -1 /* Magnitude */, minmax[0], pops, inc, numberOfBins, invalid));
    default:
      cout << "UpdateFromFile: Unknown data type" << endl;
  }

#ifndef NDEBUG
  vtkIdType total = invalid;
  for (int i = 0; i < numberOfBins; ++i)
    total += pops[i];
  assert(total == arrayPtr->GetNumberOfTuples());
#endif
  if (invalid) {
    cout << "Warning: NaN or infinite value in dataset" << endl;
  }

  output->AddColumn(extents.Get());
  output->AddColumn(populations.Get());
}

void Populate2DHistogram(vtkImageData* input, vtkImageData* output)
{
  double minmax[2] = { 0.0, 0.0 };
  const int numberOfBins = 256;

  // Keep the array we are working on around even if the user shallow copies
  // over the input image data by incrementing the reference count here.
  vtkSmartPointer<vtkDataArray> arrayPtr = input->GetPointData()->GetScalars();

  // The bin values are the centers, extending +/- half an inc either side
  arrayPtr->GetFiniteRange(minmax, -1);
  if (minmax[0] == minmax[1]) {
    minmax[1] = minmax[0] + 1.0;
  }

  // vtkPlotHistogram2D expects the histogram array to be VTK_DOUBLE
  output->SetDimensions(numberOfBins, numberOfBins, 1);
  output->AllocateScalars(VTK_DOUBLE, 1);

  // Get input parameters
  int dim[3];
  input->GetDimensions(dim);
  int numComp = arrayPtr->GetNumberOfComponents();
  double spacing[3];
  input->GetSpacing(spacing);

  switch (arrayPtr->GetDataType()) {
    vtkTemplateMacro(tomviz::Calculate2DHistogram(
      reinterpret_cast<VTK_TT*>(arrayPtr->GetVoidPointer(0)), dim, numComp,
      minmax, output, spacing));
    default:
      cout << "UpdateFromFile: Unknown data type" << endl;
  }
}

// This is a QObject that will be owned by the background thread
// and use signals/slots to create histograms
class HistogramMaker : public QObject
{
  Q_OBJECT

  void run();

public:
  HistogramMaker(QObject* p = nullptr) : QObject(p) {}

public slots:
  void makeHistogram(vtkSmartPointer<vtkImageData> input,
                     vtkSmartPointer<vtkTable> output);

  void makeHistogram2D(vtkSmartPointer<vtkImageData> input,
                       vtkSmartPointer<vtkImageData> output);

signals:
  void histogramDone(vtkSmartPointer<vtkImageData> image,
                     vtkSmartPointer<vtkTable> output);

  void histogram2DDone(vtkSmartPointer<vtkImageData> image,
                       vtkSmartPointer<vtkImageData> output);
};

void HistogramMaker::makeHistogram(vtkSmartPointer<vtkImageData> input,
                                   vtkSmartPointer<vtkTable> output)
{
  // make the histogram and notify observers (the main thread) that it
  // is done.
  if (input && output) {
    PopulateHistogram(input.Get(), output.Get());
  }
  emit histogramDone(input, output);
}

void HistogramMaker::makeHistogram2D(vtkSmartPointer<vtkImageData> input,
                                     vtkSmartPointer<vtkImageData> output)
{
  if (input && output) {
    Populate2DHistogram(input.Get(), output.Get());
  }
  emit histogram2DDone(input, output);
}

//////////////////////////////////////////////////////////////////////////////////
/**
 * \brief Data model holding a set of vtkTransferFunctionBoxItem instances used
 * to
 * edit a 2D transfer function.
 * \note Does not currently support insertion and removal of items.
 */
class Transfer2DModel : public AbstractDataModel
{
public:
  using ItemBoxPtr = vtkSmartPointer<vtkTransferFunctionBoxItem>;
  using DataItemBox = DataItem<ItemBoxPtr>;

  Transfer2DModel(QObject* parent = nullptr) : AbstractDataModel(parent)
  {
    initializeRootItem();
    populate();
  };

  ~Transfer2DModel() = default;

  void initializeRootItem()
  {
    m_rootItem = new DataItemBox;
    m_rootItem->setData(0, Qt::DisplayRole, "Id");
    m_rootItem->setData(0, Qt::DisplayRole, "Name");
  };

  /**
   * Initializes with a default TFBoxItem, which will be used to hold the
   * default Module/DataSource transfer functions.
   */
  void populate()
  {
    auto item = new DataItemBox(m_rootItem);
    item->setData(0, Qt::DisplayRole, m_rootItem->childCount() + 1);
    auto itemBox = ItemBoxPtr::New();
    item->setReferencedData(itemBox);
  };

  const ItemBoxPtr& get(const QModelIndex& index)
  {
    const auto itemBox = static_cast<const DataItemBox*>(getItem(index));
    return itemBox->getReferencedDataConst();
  };

  /**
   * Returns the first element of the list which refers to the default
   * Module/DataSource transfer function box.
   */
  const ItemBoxPtr& getDefault() { return get(index(0, 0, QModelIndex())); }

private:
  Transfer2DModel(const Transfer2DModel&) = delete;
  void operator=(const Transfer2DModel&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
CentralWidget::CentralWidget(QWidget* parentObject, Qt::WindowFlags wflags)
  : QWidget(parentObject, wflags), m_ui(new Ui::CentralWidget),
    m_timer(new QTimer(this)), m_histogramGen(new HistogramMaker),
    m_worker(new QThread(this)), m_transfer2DModel(new Transfer2DModel(this))
{
  m_ui->setupUi(this);

  // Setup the view for the list of 2D transfer functions. This functionality
  // is work-in-progress and currenlty hidden.
  m_ui->tvTransfer2DSelection->setModel(m_transfer2DModel);
  m_ui->wTransfer2DSelection->hide();

  // Hide the layout tabs
  m_ui->tabbedMultiViewWidget->setTabVisibility(false);

  qRegisterMetaType<vtkSmartPointer<vtkImageData>>();
  qRegisterMetaType<vtkSmartPointer<vtkTable>>();

  QList<int> sizes;
  sizes << 200 << 200;
  m_ui->splitter->setSizes(sizes);
  m_ui->splitter->setStretchFactor(0, 0);
  m_ui->splitter->setStretchFactor(1, 1);

  connect(m_ui->histogramWidget, SIGNAL(colorMapUpdated()),
          SLOT(onColorMapUpdated()));
  connect(m_ui->gradientOpacityWidget, SIGNAL(mapUpdated()),
          SLOT(onColorMapUpdated()));
  m_ui->gradientOpacityWidget->hide();
  connect(m_ui->histogramWidget, SIGNAL(opacityChanged()),
          m_ui->histogram2DWidget, SLOT(updateTransfer2D()));

  // Start the worker thread and give it ownership of the HistogramMaker
  // object. Also connect the HistogramMaker's signal to the histogramReady
  // slot on this object. This slot will be called on the GUI thread when the
  // histogram has been finished on the background thread.
  m_worker->start();
  m_histogramGen->moveToThread(m_worker);
  connect(m_histogramGen, SIGNAL(histogramDone(vtkSmartPointer<vtkImageData>,
                                               vtkSmartPointer<vtkTable>)),
          SLOT(histogramReady(vtkSmartPointer<vtkImageData>,
                              vtkSmartPointer<vtkTable>)));
  connect(m_histogramGen,
          SIGNAL(histogram2DDone(vtkSmartPointer<vtkImageData>,
                                 vtkSmartPointer<vtkImageData>)),
          SLOT(histogram2DReady(vtkSmartPointer<vtkImageData>,
                                vtkSmartPointer<vtkImageData>)));
  m_timer->setInterval(200);
  m_timer->setSingleShot(true);
  connect(m_timer.data(), SIGNAL(timeout()), SLOT(refreshHistogram()));
  layout()->setMargin(0);
  layout()->setSpacing(0);
}

CentralWidget::~CentralWidget()
{
  // disconnect all signals/slots
  disconnect(m_histogramGen, nullptr, nullptr, nullptr);
  // when the HistogramMaker is deleted, kill the background thread
  connect(m_histogramGen, SIGNAL(destroyed()), m_worker, SLOT(quit()));
  // I can't remember if deleteLater must be called on the owning thread
  // play it safe and let the owning thread call it.
  QMetaObject::invokeMethod(m_histogramGen, "deleteLater");
  // Wait for the background thread to clean up the object and quit
  while (m_worker->isRunning()) {
    QCoreApplication::processEvents();
  }
}

void CentralWidget::setActiveColorMapDataSource(DataSource* source)
{
  setColorMapDataSource(source);
}

void CentralWidget::setActiveModule(Module* module)
{
  if (m_activeModule) {
    m_activeModule->disconnect(this);
  }
  m_activeModule = module;
  if (m_activeModule) {
    connect(m_activeModule, SIGNAL(colorMapChanged()),
            SLOT(onColorMapDataSourceChanged()));
    setColorMapDataSource(module->colorMapDataSource());
    connect(m_activeModule, SIGNAL(transferModeChanged(const int)), this,
            SLOT(onTransferModeChanged(const int)));
    onTransferModeChanged(static_cast<int>(m_activeModule->getTransferMode()));

  } else {
    setColorMapDataSource(nullptr);
  }
}

void CentralWidget::setColorMapDataSource(DataSource* source)
{
  if (m_activeColorMapDataSource) {
    m_activeColorMapDataSource->disconnect(this);
    m_ui->histogramWidget->disconnect(m_activeColorMapDataSource);
  }
  m_activeColorMapDataSource = source;

  if (source) {
    connect(source, SIGNAL(dataChanged()), SLOT(onColorMapDataSourceChanged()));
  }

  if (!source) {
    m_ui->histogramWidget->setInputData(nullptr, "", "");
    m_ui->gradientOpacityWidget->setInputData(nullptr, "", "");
    return;
  }

  // Get the actual data source, build a histogram out of it.
  auto t =
    vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
  auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  if (image->GetPointData()->GetScalars() == nullptr) {
    return;
  }

  // Get the current color map
  if (m_activeModule) {
    m_ui->histogramWidget->setLUTProxy(m_activeModule->colorMap());
    if (m_activeModule->supportsGradientOpacity()) {
      m_ui->gradientOpacityWidget->setLUT(m_activeModule->gradientOpacityMap());
      m_transfer2DModel->getDefault()->SetColorFunction(
        vtkColorTransferFunction::SafeDownCast(
          m_activeModule->colorMap()->GetClientSideObject()));
      m_transfer2DModel->getDefault()->SetOpacityFunction(
        vtkPiecewiseFunction::SafeDownCast(
          m_activeModule->opacityMap()->GetClientSideObject()));
      m_ui->histogram2DWidget->setTransfer2D(
        m_activeModule->transferFunction2D());
    }
  } else {
    m_ui->histogramWidget->setLUTProxy(source->colorMap());
    m_ui->gradientOpacityWidget->setLUT(source->gradientOpacityMap());

    m_transfer2DModel->getDefault()->SetColorFunction(
      vtkColorTransferFunction::SafeDownCast(
        source->colorMap()->GetClientSideObject()));
    m_transfer2DModel->getDefault()->SetOpacityFunction(
      vtkPiecewiseFunction::SafeDownCast(
        source->opacityMap()->GetClientSideObject()));
    m_ui->histogram2DWidget->setTransfer2D(source->transferFunction2D());
  }
  m_ui->histogram2DWidget->updateTransfer2D();

  // Check our cache, and use that if appopriate (or update it).
  if (m_histogramCache.contains(image)) {
    auto cachedTable = m_histogramCache[image];
    if (cachedTable->GetMTime() > image->GetMTime()) {
      setHistogramTable(cachedTable);
      return;
    } else {
      // Need to recalculate, clear the plots, and remove the cached data.
      m_histogramCache.remove(image);
    }
  }

  // Calculate a histogram.
  auto table = vtkSmartPointer<vtkTable>::New();
  m_histogramCache[image] = table.Get();
  vtkSmartPointer<vtkImageData> const imageSP = image;

  // This fakes a Qt signal to the background thread (without exposing the
  // class internals as a signal).  The background thread will then call
  // makeHistogram on the HistogramMaker object with the parameters we
  // gave here.
  QMetaObject::invokeMethod(m_histogramGen, "makeHistogram",
                            Q_ARG(vtkSmartPointer<vtkImageData>, imageSP),
                            Q_ARG(vtkSmartPointer<vtkTable>, table));

  auto histogram = vtkSmartPointer<vtkImageData>::New();
  QMetaObject::invokeMethod(m_histogramGen, "makeHistogram2D",
                            Q_ARG(vtkSmartPointer<vtkImageData>, imageSP),
                            Q_ARG(vtkSmartPointer<vtkImageData>, histogram));
}

void CentralWidget::onColorMapUpdated()
{
  onColorMapDataSourceChanged();
}

void CentralWidget::onColorMapDataSourceChanged()
{
  // This starts/restarts the internal timer so that several events occurring
  // within a few milliseconds of each other only result in one call to
  // refreshHistogram()
  m_timer->start();
}

void CentralWidget::refreshHistogram()
{
  setColorMapDataSource(m_activeColorMapDataSource);
}

void CentralWidget::histogramReady(vtkSmartPointer<vtkImageData> input,
                                   vtkSmartPointer<vtkTable> output)
{
  vtkImageData* inputIm = getInputImage(input);
  if (!inputIm || !output) {
    return;
  }

  setHistogramTable(output.Get());
}

void CentralWidget::histogram2DReady(vtkSmartPointer<vtkImageData> input,
                                     vtkSmartPointer<vtkImageData> output)
{
  vtkImageData* inputIm = getInputImage(input);
  if (!inputIm || !output) {
    return;
  }

  m_ui->histogram2DWidget->setHistogram(output);
  m_ui->histogram2DWidget->addFunctionItem(m_transfer2DModel->getDefault());
}

vtkImageData* CentralWidget::getInputImage(vtkSmartPointer<vtkImageData> input)
{
  if (!input) {
    return nullptr;
  }

  // If we no longer have an active datasource, ignore showing the histogram
  // since the data has been deleted
  if (!m_activeColorMapDataSource) {
    return nullptr;
  }

  auto t = vtkTrivialProducer::SafeDownCast(
    m_activeColorMapDataSource->producer()->GetClientSideObject());
  auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  // The current dataset has changed since the histogram was requested,
  // ignore this histogram and wait for the next one queued...
  if (image != input.Get()) {
    return nullptr;
  }

  return image;
}

void CentralWidget::setHistogramTable(vtkTable* table)
{
  auto arr = vtkDataArray::SafeDownCast(table->GetColumnByName("image_pops"));
  if (!arr) {
    return;
  }

  m_ui->histogramWidget->setInputData(table, "image_extents", "image_pops");
  m_ui->gradientOpacityWidget->setInputData(table, "image_extents",
                                            "image_pops");
}

void CentralWidget::onTransferModeChanged(const int mode)
{
  if (!m_activeModule) {
    return;
  }

  int index = 0;
  switch (static_cast<Module::TransferMode>(mode)) {
    case Module::SCALAR:
      m_ui->gradientOpacityWidget->hide();
      break;
    case Module::GRADIENT_1D:
      m_ui->gradientOpacityWidget->show();
      break;
    case Module::GRADIENT_2D:
      index = 1;
      break;
  }

  m_ui->swTransferMode->setCurrentIndex(index);
}

} // end of namespace tomviz

#include "CentralWidget.moc"
