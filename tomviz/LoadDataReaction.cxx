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
#include "Pipeline.h"
#include "PipelineManager.h"
#include "RAWFileReaderDialog.h"
#include "RecentFilesMenu.h"
#include "Utilities.h"

#include <pqActiveObjects.h>
#include <pqLoadDataReaction.h>
#include <pqPipelineSource.h>
#include <pqProxyWidgetDialog.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>
#include <pqView.h>
#include <vtkSMCoreUtilities.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMStringVectorProperty.h>
#include <vtkSMViewProxy.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>

#include <vtk_pugixml.h>

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>

#include <sstream>

namespace {
bool hasData(vtkSMProxy* reader)
{
  vtkSMSourceProxy* dataSource = vtkSMSourceProxy::SafeDownCast(reader);
  if (!dataSource) {
    return false;
  }

  dataSource->UpdatePipeline();
  vtkAlgorithm* vtkalgorithm =
    vtkAlgorithm::SafeDownCast(dataSource->GetClientSideObject());
  if (!vtkalgorithm) {
    return false;
  }

  // Create a clone and release the reader data.
  vtkImageData* data =
    vtkImageData::SafeDownCast(vtkalgorithm->GetOutputDataObject(0));
  if (!data) {
    return false;
  }

  int extent[6];
  data->GetExtent(extent);
  if (extent[0] > extent[1] || extent[2] > extent[3] || extent[4] > extent[5]) {
    return false;
  }

  vtkPointData* pd = data->GetPointData();
  if (!pd) {
    return false;
  }

  if (pd->GetNumberOfArrays() < 1) {
    return false;
  }
  return true;
}

QString pvXml(vtkSMProxy* readerProxy)
{
  pugi::xml_document doc;
  pugi::xml_node root = doc.root();
  pugi::xml_node node = root.prepend_child("DataReader");
  node.append_attribute("xmlgroup").set_value(readerProxy->GetXMLGroup());
  node.append_attribute("xmlname").set_value(readerProxy->GetXMLName());
  tomviz::serialize(readerProxy, node);
  std::ostringstream stream;
  doc.save(stream);

  return QString(stream.str().c_str());
}

}

namespace tomviz {

LoadDataReaction::LoadDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
}

LoadDataReaction::~LoadDataReaction() = default;

void LoadDataReaction::onTriggered()
{
  loadData();
}

QList<DataSource*> LoadDataReaction::loadData()
{
  QStringList filters;
  filters
    << "Common file types (*.emd *.jpg *.jpeg *.png *.tiff *.tif *.raw"
       " *.dat *.bin *.txt *.mhd *.mha *.vti *.mrc *.st *.rec *.ali *.xmf *.xdmf)"
    << "EMD (*.emd)"
    << "JPeg Image files (*.jpg *.jpeg)"
    << "PNG Image files (*.png)"
    << "TIFF Image files (*.tiff *.tif)"
    << "OME-TIFF Image files (*.ome.tif)"
    << "Raw data files (*.raw *.dat *.bin)"
    << "Meta Image files (*.mhd *.mha)"
    << "VTK ImageData Files (*.vti)"
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

DataSource* LoadDataReaction::loadData(const QString& fileName,
                                       bool defaultModules, bool addToRecent,
                                       bool child, const QJsonObject& options)
{
  QJsonObject opts = options;
  opts["defaultModules"] = defaultModules;
  opts["addToRecent"] = addToRecent;
  opts["child"] = child;
  return LoadDataReaction::loadData(fileName, opts);
}

DataSource* LoadDataReaction::loadData(const QString& fileName,
                                       const QJsonObject& val)
{
  QStringList fileNames;
  fileNames << fileName;

  return loadData(fileNames, val);
}

DataSource* LoadDataReaction::loadData(const QStringList& fileNames,
                                       const QJsonObject& options)
{
  bool defaultModules = options["defaultModules"].toBool(true);
  bool addToRecent = options["addToRecent"].toBool(true);
  bool child = options["child"].toBool(false);

  DataSource* dataSource(nullptr);
  QString xml;
  QString fileName;
  if (fileNames.size() > 0) {
    fileName = fileNames[0];
  }
  QFileInfo info(fileName);
  if (info.suffix().toLower() == "emd") {
    // Load the file using our simple EMD class.
    EmdFormat emdFile;
    vtkNew<vtkImageData> imageData;
    if (emdFile.read(fileName.toLatin1().data(), imageData)) {
      dataSource = new DataSource(imageData);
      LoadDataReaction::dataSourceAdded(dataSource, defaultModules, child);
    }
  } else if (options.contains("pvXml") && options["pvXml"].isString()) {
    // Create the ParaView reader from the XML supplied.
    pugi::xml_document doc;
    xml = options["pvXml"].toString();
    if (doc.load(xml.toUtf8().data())) {
      pugi::xml_node root = doc.root();
      pugi::xml_node node = root.child("DataReader");
      auto pxm = ActiveObjects::instance().proxyManager();
      vtkSmartPointer<vtkSMProxy> reader;
      reader.TakeReference(
        pxm->NewProxy(node.attribute("xmlgroup").as_string(),
                      node.attribute("xmlname").as_string()));
      if (tomviz::deserialize(reader, node)) {
        // Use fileNames provided rather than relying on the XML. When loading
        // a state file the file names provide are resolved relative to the
        // state files so will survive a directory move.
        // The are the possible ParaView properties
        const char* propNames[] = { "FileNames", "FileName", "FilePrefix" };
        for (int i = 0; i < 3; i++) {
          auto propName = propNames[i];
          auto prop = reader->GetProperty(propName);
          if (prop != nullptr) {
            vtkSMPropertyHelper helper(prop);
            helper.Set(fileName.toLatin1().data());
            break;
          }
        }

        reader->UpdateVTKObjects();
        vtkSMSourceProxy::SafeDownCast(reader)->UpdatePipelineInformation();
        dataSource =
          LoadDataReaction::createDataSource(reader, defaultModules, child);
      }
    }
  } else if (info.completeSuffix().endsWith("ome.tif")) {
    auto pxm = tomviz::ActiveObjects::instance().proxyManager();
    vtkSmartPointer<vtkSMProxy> source;
    source.TakeReference(pxm->NewProxy("sources", "OMETIFFReader"));
    QString pname = vtkSMCoreUtilities::GetFileNameProperty(source);
    vtkSMStringVectorProperty* prop = vtkSMStringVectorProperty::SafeDownCast(
      source->GetProperty(pname.toUtf8().data()));
    pqSMAdaptor::setElementProperty(prop, fileName);
    source->UpdateVTKObjects();

    dataSource = createDataSource(source, defaultModules, child);
    xml = pvXml(source);
  } else {
    // Use ParaView's file load infrastructure.
    pqPipelineSource* reader = pqLoadDataReaction::loadData(fileNames);
    if (!reader) {
      return nullptr;
    }

    dataSource = createDataSource(reader->getProxy(), defaultModules, child);
    xml = pvXml(reader->getProxy());
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(reader->getProxy());
  }

  // Now for house keeping, registering elements, etc.
  dataSource->setFileName(fileName);
  if (fileNames.size() > 1) {
    dataSource->setFileNames(fileNames);
  }
  if (!xml.isEmpty()) {
    dataSource->setPvReaderXml(xml);
  }
  if (addToRecent && dataSource) {
    RecentFilesMenu::pushDataReader(dataSource);
  }
  return dataSource;
}

DataSource* LoadDataReaction::createDataSource(vtkSMProxy* reader,
                                               bool defaultModules, bool child)
{
  // Prompt user for reader configuration, unless it is TIFF.
  QScopedPointer<QDialog> dialog(new pqProxyWidgetDialog(reader));
  bool hasVisibleWidgets =
    qobject_cast<pqProxyWidgetDialog*>(dialog.data())->hasVisibleWidgets();
  if (QString(reader->GetXMLName()) == "TVRawImageReader") {
    dialog.reset(new RAWFileReaderDialog(reader));
    hasVisibleWidgets = true;
    // This will show only a partial filename when reading multiple files as a
    // raw image
    vtkSMPropertyHelper fname(reader, "FilePrefix");
    QFileInfo info(fname.GetAsString());
    dialog->setWindowTitle(QString("Opening %1").arg(info.fileName()));
  } else {
    dialog->setWindowTitle("Configure Reader Parameters");
  }
  dialog->setObjectName("ConfigureReaderDialog");
  if (QString(reader->GetXMLName()) == "TIFFSeriesReader" ||
      hasVisibleWidgets == false || dialog->exec() == QDialog::Accepted) {

    if (!hasData(reader)) {
      qCritical() << "Error: failed to load file!";
      return nullptr;
    }

    auto source = vtkSMSourceProxy::SafeDownCast(reader);
    source->UpdatePipeline();
    auto algo = vtkAlgorithm::SafeDownCast(source->GetClientSideObject());
    auto data = algo->GetOutputDataObject(0);
    auto image = vtkImageData::SafeDownCast(data);

    DataSource* dataSource = new DataSource(image);
    // Do whatever we need to do with a new data source.
    LoadDataReaction::dataSourceAdded(dataSource, defaultModules, child);
    return dataSource;
  }
  return nullptr;
}

void LoadDataReaction::dataSourceAdded(DataSource* dataSource,
                                       bool defaultModules, bool child)
{
  DataSource* previousActiveDataSource =
    ActiveObjects::instance().activeDataSource();
  bool oldMoveObjectsEnabled = ActiveObjects::instance().moveObjectsEnabled();
  ActiveObjects::instance().setMoveObjectsMode(false);
  if (child) {
    ModuleManager::instance().addChildDataSource(dataSource);
  } else {
    auto pipeline = new Pipeline(dataSource);
    PipelineManager::instance().addPipeline(pipeline);
    // TODO Eventually we shouldn't need to keep track of the data sources,
    // the pipeline should do that for us.
    ModuleManager::instance().addDataSource(dataSource);
    if (defaultModules) {
      pipeline->addDefaultModules(dataSource);
    }
  }

  // Work through pathological cases as necessary, prefer active view.
  ActiveObjects::instance().createRenderViewIfNeeded();
  auto view = ActiveObjects::instance().activeView();

  if (!view || QString(view->GetXMLName()) != "RenderView") {
    ActiveObjects::instance().setActiveViewToFirstRenderView();
    view = ActiveObjects::instance().activeView();
  }

  ActiveObjects::instance().setMoveObjectsMode(oldMoveObjectsEnabled);
  if (!previousActiveDataSource) {
    pqRenderView* renderView =
      qobject_cast<pqRenderView*>(pqActiveObjects::instance().activeView());
    if (renderView) {
      tomviz::createCameraOrbit(dataSource->proxy(),
                                renderView->getRenderViewProxy());
    }
  }
}

} // end of namespace tomviz
