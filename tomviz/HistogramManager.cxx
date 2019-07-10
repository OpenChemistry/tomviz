/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "HistogramManager.h"

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkTable.h>
#include <vtkUnsignedLongLongArray.h>

#include "ComputeHistogram.h"

#include <QCoreApplication>
#include <QThread>

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)
Q_DECLARE_METATYPE(vtkSmartPointer<vtkTable>)

namespace {

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
  if (!arrayPtr) {
    return;
  }

  // The bin values are the centers, extending +/- half an inc either side
  arrayPtr->GetFiniteRange(minmax, -1);
  if (minmax[0] == minmax[1]) {
    minmax[1] = minmax[0] + 1.0;
  }

  double inc = (minmax[1] - minmax[0]) / (numberOfBins - 1);
  double halfInc = inc / 2.0;
  vtkSmartPointer<vtkFloatArray> extents =
    vtkFloatArray::SafeDownCast(output->GetColumnByName("image_extents"));
  if (!extents) {
    extents = vtkSmartPointer<vtkFloatArray>::New();
    extents->SetName("image_extents");
  }
  extents->SetNumberOfTuples(numberOfBins);
  double min = minmax[0] + halfInc;
  for (int j = 0; j < numberOfBins; ++j) {
    extents->SetValue(j, min + j * inc);
  }
  vtkSmartPointer<vtkUnsignedLongLongArray> populations =
    vtkUnsignedLongLongArray::SafeDownCast(
      output->GetColumnByName("image_pops"));
  if (!populations) {
    populations = vtkSmartPointer<vtkUnsignedLongLongArray>::New();
    populations->SetName("image_pops");
  }
  populations->SetNumberOfTuples(numberOfBins);
  auto pops = static_cast<uint64_t*>(populations->GetVoidPointer(0));
  for (int k = 0; k < numberOfBins; ++k) {
    pops[k] = 0;
  }
  int invalid = 0;

  switch (arrayPtr->GetDataType()) {
    vtkTemplateMacro(tomviz::CalculateHistogram(
      reinterpret_cast<VTK_TT*>(arrayPtr->GetVoidPointer(0)),
      arrayPtr->GetNumberOfTuples(), arrayPtr->GetNumberOfComponents(),
      minmax[0], minmax[1], pops, 1.0 / inc, invalid));
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

  output->AddColumn(extents);
  output->AddColumn(populations);
}

void Populate2DHistogram(vtkImageData* input, vtkImageData* output)
{
  double minmax[2] = { 0.0, 0.0 };
  const int numberOfBins = 256;

  // Keep the array we are working on around even if the user shallow copies
  // over the input image data by incrementing the reference count here.
  vtkSmartPointer<vtkDataArray> arrayPtr = input->GetPointData()->GetScalars();
  if (!arrayPtr) {
    return;
  }

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

} // namespace

namespace tomviz {

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
    PopulateHistogram(input, output);
  }
  emit histogramDone(input, output);
}

void HistogramMaker::makeHistogram2D(vtkSmartPointer<vtkImageData> input,
                                     vtkSmartPointer<vtkImageData> output)
{
  if (input && output) {
    Populate2DHistogram(input, output);
  }
  emit histogram2DDone(input, output);
}

HistogramManager::HistogramManager()
  : m_histogramGen(new HistogramMaker), m_worker(new QThread(this))
{
  qRegisterMetaType<vtkSmartPointer<vtkImageData>>();
  qRegisterMetaType<vtkSmartPointer<vtkTable>>();

  // Start the worker thread and give it ownership of the HistogramMaker
  // object. Also connect the HistogramMaker's signal to the histogramReady
  // slot on this object. This slot will be called on the GUI thread when the
  // histogram has been finished on the background thread.
  m_worker->start();
  m_histogramGen->moveToThread(m_worker);
  connect(m_histogramGen, SIGNAL(histogramDone(vtkSmartPointer<vtkImageData>,
                                               vtkSmartPointer<vtkTable>)),
          SLOT(histogramReadyInternal(vtkSmartPointer<vtkImageData>,
                                      vtkSmartPointer<vtkTable>)));
  connect(m_histogramGen,
          SIGNAL(histogram2DDone(vtkSmartPointer<vtkImageData>,
                                 vtkSmartPointer<vtkImageData>)),
          SLOT(histogram2DReadyInternal(vtkSmartPointer<vtkImageData>,
                                        vtkSmartPointer<vtkImageData>)));
}

HistogramManager::~HistogramManager()
{
  if (m_worker) {
    finalize();
  }
}

void HistogramManager::finalize()
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
  m_worker = nullptr;
  m_histogramCache.clear();
  m_histogram2DCache.clear();
}

HistogramManager& HistogramManager::instance()
{
  static HistogramManager theInstance;
  return theInstance;
}

vtkSmartPointer<vtkTable> HistogramManager::getHistogram(
  vtkSmartPointer<vtkImageData> image)
{
  if (m_histogramCache.contains(image)) {
    auto cachedTable = m_histogramCache[image];
    if (cachedTable->GetMTime() > image->GetMTime()) {
      return cachedTable;
    } else {
      // Need to recalculate, clear the plots, and remove the cached data.
      m_histogramCache.remove(image);
    }
  }
  if (m_histogramsInProgress.contains(image)) {
    // it is in progress, don't start a new one
    return nullptr;
  }
  auto table = vtkSmartPointer<vtkTable>::New();
  m_histogramsInProgress.append(image);
  vtkSmartPointer<vtkImageData> const imageSP = image;

  // This fakes a Qt signal to the background thread (without exposing the
  // class internals as a signal).  The background thread will then call
  // makeHistogram on the HistogramMaker object with the parameters we
  // gave here.
  QMetaObject::invokeMethod(m_histogramGen, "makeHistogram",
                            Q_ARG(vtkSmartPointer<vtkImageData>, imageSP),
                            Q_ARG(vtkSmartPointer<vtkTable>, table));

  // The histogram cannot be returned for use while the background thread is
  // populating it.
  return nullptr;
}

vtkSmartPointer<vtkImageData> HistogramManager::getHistogram2D(
  vtkSmartPointer<vtkImageData> image)
{
  if (m_histogram2DCache.contains(image)) {
    auto cachedHistogram = m_histogram2DCache[image];
    if (cachedHistogram->GetMTime() > image->GetMTime()) {
      return cachedHistogram;
    } else {
      // Need to recalculate, clear the plots, and remove the cached data.
      m_histogram2DCache.remove(image);
    }
  }
  if (m_histogram2DsInProgress.contains(image)) {
    // it is in progress, don't start a new one
    return nullptr;
  }
  auto histogram = vtkSmartPointer<vtkImageData>::New();
  m_histogram2DsInProgress.append(image);
  vtkSmartPointer<vtkImageData> const imageSP = image;

  // This fakes a Qt signal to the background thread (without exposing the
  // class internals as a signal).  The background thread will then call
  // makeHistogram on the HistogramMaker object with the parameters we
  // gave here.
  QMetaObject::invokeMethod(m_histogramGen, "makeHistogram2D",
                            Q_ARG(vtkSmartPointer<vtkImageData>, imageSP),
                            Q_ARG(vtkSmartPointer<vtkImageData>, histogram));
  // The histogram cannot be returned for use while the background thread is
  // populating it.
  return nullptr;
}

void HistogramManager::histogramReadyInternal(
  vtkSmartPointer<vtkImageData> image, vtkSmartPointer<vtkTable> histogram)
{
  m_histogramCache[image] = histogram;
  m_histogramsInProgress.removeAll(image);
  emit this->histogramReady(image, histogram);
}

void HistogramManager::histogram2DReadyInternal(
  vtkSmartPointer<vtkImageData> image, vtkSmartPointer<vtkImageData> histogram)
{
  m_histogram2DCache[image] = histogram;
  m_histogram2DsInProgress.removeAll(image);
  emit this->histogram2DReady(image, histogram);
}

} // namespace tomviz

#include "HistogramManager.moc"
