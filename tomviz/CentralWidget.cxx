/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CentralWidget.h"
#include "ui_CentralWidget.h"

#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkTable.h>
#include <vtkTransferFunctionBoxItem.h>
#include <vtkVector.h>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <vtkDataArray.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMViewProxy.h>

#include <QThread>
#include <QTimer>

#include "AbstractDataModel.h"
#include "ActiveObjects.h"
#include "GradientOpacityWidget.h"
#include "Histogram2DWidget.h"
#include "HistogramWidget.h"
#include "HistogramManager.h"

#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/sinks/VolumeSink.h"
#include "pipeline/data/VolumeData.h"

namespace tomviz {

//////////////////////////////////////////////////////////////////////////////////
/**
 * \brief Data model holding a set of vtkTransferFunctionBoxItem instances used
 * to edit a 2D transfer function.
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
  }

  ~Transfer2DModel() = default;

  void initializeRootItem()
  {
    m_rootItem = new DataItemBox;
    m_rootItem->setData(0, Qt::DisplayRole, "Id");
    m_rootItem->setData(0, Qt::DisplayRole, "Name");
  }

  /**
   * Initializes with a default TFBoxItem, which will be used to hold the
   * default transfer functions.
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
  }

  /**
   * Returns the first element of the list which refers to the default
   * transfer function box.
   */
  const ItemBoxPtr& getDefault() { return get(index(0, 0, QModelIndex())); }

private:
  Transfer2DModel(const Transfer2DModel&) = delete;
  void operator=(const Transfer2DModel&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
CentralWidget::CentralWidget(QWidget* parentObject, Qt::WindowFlags wflags)
  : QWidget(parentObject, wflags), m_ui(new Ui::CentralWidget),
    m_timer(new QTimer(this)), m_transfer2DModel(new Transfer2DModel(this))
{
  m_ui->setupUi(this);

  // Setup the view for the list of 2D transfer functions. This functionality
  // is work-in-progress and currently hidden.
  m_ui->tvTransfer2DSelection->setModel(m_transfer2DModel);
  m_ui->wTransfer2DSelection->hide();

  // Hide the layout tabs
  m_ui->tabbedMultiViewWidget->setTabVisibility(false);

  // Setting the initial split size is trickier than you might expect, set the
  // stretch to favor the 3D widget, and then wait for layout to complete to
  // reallocate size if this is the first time this window is shown.
  m_ui->splitter->setStretchFactor(0, 0);
  m_ui->splitter->setStretchFactor(1, 1);
  QTimer::singleShot(0, [this]() {
    auto settings = pqApplicationCore::instance()->settings();
    bool resize = settings->value("Tomviz.firstCentralWidget", true).toBool();
    settings->setValue("Tomviz.firstCentralWidget", false);
    if (resize) {
      // Allocate maximum to the 3D widget, added a fallback so both are shown.
      int mainWidgetSize = m_ui->splitter->size().height() - 150;
      if (mainWidgetSize < 650) {
        mainWidgetSize = 650;
      }
      m_ui->splitter->setSizes({ 150, mainWidgetSize });
    } else {
      auto sizing = settings->value("Tomviz.centralSplitSizes").toByteArray();
      m_ui->splitter->restoreState(sizing);
    }
  });

  connect(m_ui->histogramWidget, &HistogramWidget::colorMapUpdated, this,
          &CentralWidget::onColorMapUpdated);
  connect(m_ui->histogramWidget, &HistogramWidget::colorLegendToggled, this,
          &CentralWidget::onColorLegendToggled);
  connect(m_ui->gradientOpacityWidget, &GradientOpacityWidget::mapUpdated, this,
          &CentralWidget::onColorMapUpdated);
  m_ui->gradientOpacityWidget->hide();
  connect(m_ui->histogramWidget, &HistogramWidget::opacityChanged,
          m_ui->histogram2DWidget, &Histogram2DWidget::updateTransfer2D);
  // An opacity morph animation installs an override curve on the sink
  // (see ScalarOpacityAnimation); scrubbing can leave it installed with
  // no playback-ended event to clear it. Editing the histogram is the
  // user taking the controls back, so drop the override or the edit
  // renders no change.
  connect(m_ui->histogramWidget, &HistogramWidget::opacityChanged, this,
          [this]() {
            if (auto* volumeSink =
                  qobject_cast<pipeline::VolumeSink*>(m_activeSink.data())) {
              volumeSink->setAnimatedScalarOpacity(nullptr);
            }
          });

  auto& histogramMgr = HistogramManager::instance();
  connect(&histogramMgr, &HistogramManager::histogramReady, this,
          &CentralWidget::histogramReady);
  connect(&histogramMgr, &HistogramManager::histogram2DReady, this,
          &CentralWidget::histogram2DReady);

  m_timer->setInterval(200);
  m_timer->setSingleShot(true);
  connect(m_timer.data(), &QTimer::timeout, this,
          &CentralWidget::refreshHistogram);
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->setSpacing(0);
}

CentralWidget::~CentralWidget()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("Tomviz.centralSplitSizes", m_ui->splitter->saveState());
  // Shut down the background thread used to create histograms.
  HistogramManager::instance().finalize();
}

void CentralWidget::onColorMapUpdated()
{
  // HistogramWidget/GradientOpacityWidget already triggered a coalesced
  // render via pqView::render(). Refresh the histogram to stay in sync.
  onColorMapDataSourceChanged();
}

void CentralWidget::onColorLegendToggled(bool visibility)
{
  auto view = ActiveObjects::instance().activeView();
  auto sbProxy = m_ui->histogramWidget->getScalarBarRepresentation(view);
  if (view && sbProxy) {
    vtkSMPropertyHelper(sbProxy, "Visibility").Set(visibility ? 1 : 0);
    vtkSMPropertyHelper(sbProxy, "Enabled").Set(visibility ? 1 : 0);
    sbProxy->UpdateVTKObjects();
    ActiveObjects::instance().renderAllViews();
  }
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
  if (m_activeSink) {
    setActiveSinkNode(m_activeSink);
    return;
  }
  if (m_activeVolumeData) {
    setActiveVolumeData(m_activeVolumeData);
    return;
  }
}

void CentralWidget::histogramReady(vtkSmartPointer<vtkImageData> input,
                                   vtkSmartPointer<vtkTable> output)
{
  vtkImageData* inputIm = getInputImage(input);
  if (!inputIm) {
    // The image pointer doesn't match the current active data.
    // The pipeline may have replaced the image. Trigger a refresh
    // so we request a histogram for the current image.
    if (m_activeVolumeData && m_activeVolumeData->isValid() && input) {
      refreshHistogram();
    }
    return;
  }
  if (!output) {
    return;
  }

  setHistogramTable(output);
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
  refreshHistogram();
}

vtkImageData* CentralWidget::getInputImage(vtkSmartPointer<vtkImageData> input)
{
  if (!input) {
    return nullptr;
  }

  if (m_activeVolumeData && m_activeVolumeData->isValid()) {
    if (m_activeVolumeData->imageData() == input.Get()) {
      return input;
    }
  }

  return nullptr;
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

void CentralWidget::setActiveSinkNode(pipeline::LegacyModuleSink* sink)
{
  // Disconnect previous sink
  if (m_activeSink) {
    disconnect(m_activeSink, &pipeline::LegacyModuleSink::colorMapChanged,
               this, nullptr);
  }

  m_activeSink = sink;

  if (!sink) {
    m_activeVolumeData.reset();
    m_ui->histogramWidget->setVolumeData(nullptr);
    m_ui->histogramWidget->setLUTProxy(nullptr);
    m_ui->histogramWidget->setInputData(nullptr, "", "");
    m_ui->gradientOpacityWidget->setInputData(nullptr, "", "");
    return;
  }

  // Cache the VolumeData for histogram computation
  auto vol = sink->volumeData();
  m_activeVolumeData = vol;
  m_ui->histogramWidget->setVolumeData(vol);

  // Set (or clear) the color map proxy on the histogram widget; this
  // also refreshes the edit buttons' enabled state.
  m_ui->histogramWidget->setLUTProxy(sink->colorMap());
  auto* gradOp = sink->gradientOpacity();
  if (gradOp) {
    m_ui->gradientOpacityWidget->setLUT(gradOp);
  }

  // Request histogram from the VolumeData's image
  if (vol && vol->isValid()) {
    vtkSmartPointer<vtkImageData> image = vol->imageData();
    auto histogram = HistogramManager::instance().getHistogram(image);
    if (histogram) {
      setHistogramTable(histogram);
    }
    // If not cached, HistogramManager will emit histogramReady later
  }

  // Re-run on color map changes
  connect(sink, &pipeline::LegacyModuleSink::colorMapChanged, this,
          [this, sink]() { setActiveSinkNode(sink); });
}

void CentralWidget::setActiveVolumeData(pipeline::VolumeDataPtr volumeData)
{
  // Disconnect previous sink
  if (m_activeSink) {
    disconnect(m_activeSink, &pipeline::LegacyModuleSink::colorMapChanged,
               this, nullptr);
    m_activeSink = nullptr;
  }

  m_activeVolumeData = volumeData;
  m_ui->histogramWidget->setVolumeData(volumeData);

  if (!volumeData || !volumeData->isValid()) {
    m_ui->histogramWidget->setLUTProxy(nullptr);
    m_ui->histogramWidget->setInputData(nullptr, "", "");
    m_ui->gradientOpacityWidget->setInputData(nullptr, "", "");
    return;
  }

  // Only set up color map editing if the VolumeData already has a color map
  // (e.g., from a sink). Don't lazily create one for a bare output port —
  // the SM proxy created by initColorMap() may not be fully initialized.
  if (volumeData->hasColorMap()) {
    m_ui->histogramWidget->setLUTProxy(volumeData->colorMap());
    m_ui->gradientOpacityWidget->setLUT(volumeData->gradientOpacity());
  } else {
    m_ui->histogramWidget->setLUTProxy(nullptr);
  }

  // Request histogram
  vtkSmartPointer<vtkImageData> image = volumeData->imageData();
  auto histogram = HistogramManager::instance().getHistogram(image);
  if (histogram) {
    setHistogramTable(histogram);
  }
  // If not cached, HistogramManager will emit histogramReady later
}

} // end of namespace tomviz
