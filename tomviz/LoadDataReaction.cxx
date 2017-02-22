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
#include "LoadDataReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EmdFormat.h"
#include "ModuleManager.h"
#include "RecentFilesMenu.h"
#include "Utilities.h"

#include "pqActiveObjects.h"
#include "pqLoadDataReaction.h"
#include "pqPipelineSource.h"
#include "pqProxyWidgetDialog.h"
#include "pqRenderView.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSmartPointer.h"
#include "vtkTrivialProducer.h"

#include <QFileDialog>
#include <QFileInfo>

namespace tomviz {

LoadDataReaction::LoadDataReaction(QAction* parentObject)
  : Superclass(parentObject)
{
}

LoadDataReaction::~LoadDataReaction()
{
}

void LoadDataReaction::onTriggered()
{
  loadData();
}

QList<DataSource*> LoadDataReaction::loadData()
{
  vtkNew<vtkSMParaViewPipelineController> controller;

  QStringList filters;
  filters
    << "Common file types (*.emd *.jpg *.jpeg *.png *.tiff *.tif *.raw"
       " *.dat *.bin *.txt *.mhd *.mha *.mrc *.st *.rec *.ali *.xmf *.xdmf)"
    << "EMD (*.emd)"
    << "JPeg Image files (*.jpg *.jpeg)"
    << "PNG Image files (*.png)"
    << "TIFF Image files (*.tiff *.tif)"
    << "Raw data files (*.raw *.dat *.bin)"
    << "Meta Image files (*.mhd *.mha)"
    << "MRC files (*.mrc *.st *.rec *.ali)"
    << "XDMF files (*.xmf *.xdmf)"
    << "Text files (*.txt)"
    << "All files (*.*)";

  QFileDialog dialog(nullptr);
  dialog.setFileMode(QFileDialog::ExistingFiles);
  dialog.setNameFilters(filters);
  dialog.setObjectName("FileOpenDialog-tomviz"); // avoid name collision?

  QList<DataSource*> dataSources;
  if (dialog.exec()) {
    QStringList filenames = dialog.selectedFiles();
    dataSources << loadData(filenames);
  }

  return dataSources;
}

DataSource* LoadDataReaction::loadData(const QString& fileName)
{
  QStringList fileNames;
  fileNames << fileName;

  return loadData(fileNames);
}

DataSource* LoadDataReaction::loadData(const QStringList& fileNames)
{
  DataSource* dataSource(nullptr);
  QString fileName;
  if (fileNames.size() > 0) {
    fileName = fileNames[0];
  }
  QFileInfo info(fileName);
  if (info.suffix() == "emd") {
    // Load the file using our simple EMD class.
    EmdFormat emdFile;
    vtkNew<vtkImageData> imageData;
    emdFile.read(fileName.toLatin1().data(), imageData.Get());

    dataSource = createDataSource(imageData.Get());
    dataSource->originalDataSource()->SetAnnotation(Attributes::FILENAME,
                                                    fileName.toLatin1().data());
    LoadDataReaction::dataSourceAdded(dataSource);
  } else {
    // Use ParaView's file load infrastructure.
    pqPipelineSource* reader = pqLoadDataReaction::loadData(fileNames);

    if (!reader) {
      return nullptr;
    }

    dataSource = createDataSource(reader->getProxy());
    // The dataSource may be NULL if the user cancelled the action.
    if (dataSource) {
      RecentFilesMenu::pushDataReader(dataSource, reader->getProxy());
    }
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(reader->getProxy());
  }

  return dataSource;
}

DataSource* LoadDataReaction::createDataSource(vtkSMProxy* reader)
{
  // Prompt user for reader configuration.
  pqProxyWidgetDialog dialog(reader);
  dialog.setObjectName("ConfigureReaderDialog");
  dialog.setWindowTitle("Configure Reader Parameters");
  if (dialog.hasVisibleWidgets() == false ||
      dialog.exec() == QDialog::Accepted) {
    DataSource* previousActiveDataSource =
      ActiveObjects::instance().activeDataSource();

    DataSource* dataSource =
      new DataSource(vtkSMSourceProxy::SafeDownCast(reader));
    // do whatever we need to do with a new data source.
    LoadDataReaction::dataSourceAdded(dataSource);
    if (!previousActiveDataSource) {
      pqRenderView* renderView =
        qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
      if (renderView) {
        tomviz::createCameraOrbit(dataSource->producer(),
                                  renderView->getRenderViewProxy());
      }
    }
    return dataSource;
  }
  return nullptr;
}

DataSource* LoadDataReaction::createDataSource(vtkImageData* imageData)
{
  auto pxm = tomviz::ActiveObjects::instance().proxyManager();
  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  auto tp = vtkTrivialProducer::SafeDownCast(source->GetClientSideObject());
  tp->SetOutput(imageData);
  source->SetAnnotation("tomviz.Type", "DataSource");

  auto dataSource = new DataSource(vtkSMSourceProxy::SafeDownCast(source));
  return dataSource;
}

void LoadDataReaction::dataSourceAdded(DataSource* dataSource)
{
  bool oldMoveObjectsEnabled = ActiveObjects::instance().moveObjectsEnabled();
  ActiveObjects::instance().setMoveObjectsMode(false);
  ModuleManager::instance().addDataSource(dataSource);

  ActiveObjects::instance().createRenderViewIfNeeded();
  ActiveObjects::instance().setActiveViewToFirstRenderView();

  vtkSMViewProxy* view = ActiveObjects::instance().activeView();

  // Create an outline module for the source in the active view.
  if (Module* module = ModuleManager::instance().createAndAddModule(
        "Outline", dataSource, view)) {
    ActiveObjects::instance().setActiveModule(module);
  }
  if (Module* module = ModuleManager::instance().createAndAddModule(
        "Orthogonal Slice", dataSource, view)) {
    ActiveObjects::instance().setActiveModule(module);
  }
  ActiveObjects::instance().setMoveObjectsMode(oldMoveObjectsEnabled);
}

} // end of namespace tomviz
