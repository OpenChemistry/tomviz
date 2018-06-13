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

#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPNGWriter.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkTable.h>
#include <vtkTransferFunctionBoxItem.h>
#include <vtkTrivialProducer.h>
#include <vtkUnsignedShortArray.h>
#include <vtkVector.h>

#include <pqSettings.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMViewProxy.h>

#include <QModelIndex>
#include <QThread>
#include <QTimer>

#include "AbstractDataModel.h"
#include "ActiveObjects.h"
#include "DataSource.h"
#include "HistogramManager.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Utilities.h"

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
  }

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
    m_timer(new QTimer(this)), m_transfer2DModel(new Transfer2DModel(this))
{
  m_ui->setupUi(this);

  // Setup the view for the list of 2D transfer functions. This functionality
  // is work-in-progress and currenlty hidden.
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
      // Allocate maximum to the 3D widget, addded a fallback so both are shown.
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

  connect(m_ui->histogramWidget, SIGNAL(colorMapUpdated()),
          SLOT(onColorMapUpdated()));
  connect(m_ui->histogramWidget, SIGNAL(colorLegendToggled(bool)),
          SLOT(onColorLegendToggled(bool)));
  connect(m_ui->gradientOpacityWidget, SIGNAL(mapUpdated()),
          SLOT(onColorMapUpdated()));
  m_ui->gradientOpacityWidget->hide();
  connect(m_ui->histogramWidget, SIGNAL(opacityChanged()),
          m_ui->histogram2DWidget, SLOT(updateTransfer2D()));

  auto& histogramMgr = HistogramManager::instance();
  connect(&histogramMgr, &HistogramManager::histogramReady, this,
          &CentralWidget::histogramReady);
  connect(&histogramMgr, &HistogramManager::histogram2DReady, this,
          &CentralWidget::histogram2DReady);

  m_timer->setInterval(200);
  m_timer->setSingleShot(true);
  connect(m_timer.data(), SIGNAL(timeout()), SLOT(refreshHistogram()));
  layout()->setMargin(0);
  layout()->setSpacing(0);
}

CentralWidget::~CentralWidget()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("Tomviz.centralSplitSizes", m_ui->splitter->saveState());
  // Shut down the background thread used to create histograms.
  HistogramManager::instance().finalize();
}

void CentralWidget::setActiveColorMapDataSource(DataSource* source)
{
  auto selected = ActiveObjects::instance().selectedDataSource();

  // If we have a data source selected directly ( i.e. the user has clicked on
  // it ) use that, otherwise use the active transformed data source passed in.
  if (selected != nullptr) {
    source = selected;
    // set the active module to null so we use the color map for the data
    // source.
    m_activeModule = nullptr;
  }

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

void CentralWidget::setActiveOperator(Operator* op)
{
  if (op != nullptr) {
    m_activeModule = nullptr;
    setColorMapDataSource(op->dataSource());
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
    m_ui->histogram2DWidget->setTransfer2D(nullptr, nullptr);
    return;
  }

  // Get the actual data source, build a histogram out of it.
  auto image = vtkImageData::SafeDownCast(source->dataObject());

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
        m_activeModule->transferFunction2D(),
        m_activeModule->transferFunction2DBox());
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
    m_ui->histogram2DWidget->setTransfer2D(source->transferFunction2D(),
                                           source->transferFunction2DBox());
  }
  m_ui->histogram2DWidget->updateTransfer2D();

  vtkSmartPointer<vtkImageData> const imageSP = image;
  auto histogram = HistogramManager::instance().getHistogram(imageSP);
  auto histogram2D = HistogramManager::instance().getHistogram2D(imageSP);

  if (histogram) {
    setHistogramTable(histogram);
  }
  if (histogram2D) {
    m_ui->histogram2DWidget->setHistogram(histogram2D);
  }
}

void CentralWidget::onColorMapUpdated()
{
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
  refreshHistogram();
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

  auto image =
    vtkImageData::SafeDownCast(m_activeColorMapDataSource->dataObject());

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
