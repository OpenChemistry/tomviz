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
#include "FileFormatManager.h"
#include "ImageStackDialog.h"
#include "ImageStackModel.h"
#include "LoadStackReaction.h"
#include "ModuleManager.h"
#include "MoleculeSource.h"
#include "Pipeline.h"
#include "PipelineManager.h"
#include "PythonReader.h"
#include "PythonUtilities.h"
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
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTIFFReader.h>
#include <vtkTrivialProducer.h>
#include <vtkXYZMolReader2.h>

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>

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
} // namespace

namespace tomviz {

LoadDataReaction::LoadDataReaction(QAction* parentObject)
  : pqReaction(parentObject)
{}

LoadDataReaction::~LoadDataReaction() = default;

void LoadDataReaction::onTriggered()
{
  loadData();
}

QList<DataSource*> LoadDataReaction::loadData()
{
  QStringList filters;
  filters << "Common file types (*.emd *.jpg *.jpeg *.png *.tiff *.tif *.raw"
             " *.dat *.bin *.txt *.mhd *.mha *.vti *.mrc *.st *.rec *.ali "
             "*.xmf *.xdmf)"
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
          << "Molecule files (*.xyz)"
          << "Text files (*.txt)";

  foreach (auto reader, FileFormatManager::instance().pythonReaderFactories()) {
    filters << reader->getFileDialogFilter();
  }

  filters << "All files (*.*)";

  QFileDialog dialog(nullptr);
  dialog.setFileMode(QFileDialog::ExistingFiles);
  dialog.setNameFilters(filters);
  dialog.setObjectName("FileOpenDialog-tomviz"); // avoid name collision?

  QList<DataSource*> dataSources;
  if (dialog.exec()) {
    QStringList filenames = dialog.selectedFiles();
    QString fileName = filenames.size() > 0 ? filenames[0] : "";
    QFileInfo info(fileName);
    auto suffix = info.suffix().toLower();
    QStringList tiffExt = { "tif", "tiff" };
    QStringList moleculeExt = { "xyz" };
    if (filenames.size() > 1 && tiffExt.contains(suffix)) {
      dataSources << LoadStackReaction::loadData(filenames);
    } else if (moleculeExt.contains(suffix)) {
      loadMolecule(filenames);
    } else {
      dataSources << loadData(filenames);
    }
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
  bool loadWithParaview = true;
  bool loadWithPython = false;

  DataSource* dataSource(nullptr);
  QString fileName;
  if (fileNames.size() > 0) {
    fileName = fileNames[0];
  }
  QFileInfo info(fileName);
  if (info.suffix().toLower() == "emd") {
    // Load the file using our simple EMD class.
    loadWithParaview = false;
    EmdFormat emdFile;
    vtkNew<vtkImageData> imageData;
    if (emdFile.read(fileName.toLatin1().data(), imageData)) {
      dataSource = new DataSource(imageData);
      LoadDataReaction::dataSourceAdded(dataSource, defaultModules, child);
    }
  } else if (info.completeSuffix().endsWith("ome.tif")) {
    loadWithParaview = false;
    auto pxm = tomviz::ActiveObjects::instance().proxyManager();
    const char* name = "OMETIFFReader";
    vtkSmartPointer<vtkSMProxy> source;
    source.TakeReference(pxm->NewProxy("sources", name));
    QString pname = vtkSMCoreUtilities::GetFileNameProperty(source);
    vtkSMStringVectorProperty* prop = vtkSMStringVectorProperty::SafeDownCast(
      source->GetProperty(pname.toUtf8().data()));
    pqSMAdaptor::setElementProperty(prop, fileName);
    source->UpdateVTKObjects();

    dataSource = createDataSource(source, defaultModules, child);
    QJsonObject readerProperties;
    readerProperties["name"] = name;
    dataSource->setReaderProperties(readerProperties.toVariantMap());
  } else if (options.contains("reader")) {
    loadWithParaview = false;
    // Create the ParaView reader and set its properties using the JSON
    // configuration.
    auto props = options["reader"].toObject();
    auto name = props["name"].toString();

    auto pxm = ActiveObjects::instance().proxyManager();
    vtkSmartPointer<vtkSMProxy> reader;
    reader.TakeReference(pxm->NewProxy("sources", name.toLatin1().data()));

    setProperties(props, reader);
    setFileNameProperties(props, reader);
    reader->UpdateVTKObjects();
    vtkSMSourceProxy::SafeDownCast(reader)->UpdatePipelineInformation();
    dataSource =
      LoadDataReaction::createDataSource(reader, defaultModules, child);
    if (dataSource == nullptr) {
      return nullptr;
    }

    dataSource->setReaderProperties(props.toVariantMap());

  } else if (FileFormatManager::instance().pythonReaderFactory(
               info.suffix().toLower()) != nullptr) {
    loadWithParaview = false;
    loadWithPython = true;
  }

  if (loadWithParaview) {
    // Use ParaView's file load infrastructure.
    pqPipelineSource* reader = pqLoadDataReaction::loadData(fileNames);
    if (!reader) {
      return nullptr;
    }

    dataSource = createDataSource(reader->getProxy(), defaultModules, child);
    if (dataSource == nullptr) {
      return nullptr;
    }

    QJsonObject props = readerProperties(reader->getProxy());
    props["name"] = reader->getProxy()->GetXMLName();

    dataSource->setReaderProperties(props.toVariantMap());

    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(reader->getProxy());
  } else if (loadWithPython) {
    QString ext = info.suffix().toLower();
    auto factory = FileFormatManager::instance().pythonReaderFactory(ext);
    Q_ASSERT(factory != nullptr);
    auto reader = factory->createReader();
    auto imageData = reader.read(fileNames[0]);
    if (imageData == nullptr) {
      return nullptr;
    }
    dataSource = new DataSource(imageData);
    LoadDataReaction::dataSourceAdded(dataSource, defaultModules, child);
  }

  // It is possible that the dataSource will be null if, for example, loading
  // a VTI is cancelled in the array selection dialog. Guard against this.
  if (!dataSource) {
    return nullptr;
  }

  // Now for house keeping, registering elements, etc.
  // Always save it as a list, even if there is only one file.
  dataSource->setFileNames(fileNames);

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
  if (!dataSource) {
    return;
  }
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

QJsonObject LoadDataReaction::readerProperties(vtkSMProxy* reader)
{
  QStringList propNames({ "DataScalarType", "DataByteOrder",
                          "NumberOfScalarComponents", "DataExtent" });

  QJsonObject props;
  foreach (QString propName, propNames) {
    auto prop = reader->GetProperty(propName.toLatin1().data());
    if (prop != nullptr) {
      props[propName] = toJson(prop);
    }
  }

  // Special case file name related properties
  auto prop = reader->GetProperty("FileName");
  if (prop != nullptr) {
    props["fileName"] = toJson(prop);
  }
  prop = reader->GetProperty("FileNames");
  if (prop != nullptr) {
    auto fileNames = toJson(prop).toArray();
    if (fileNames.size() > 1) {
      props["fileNames"] = fileNames;
    }
    // Normalize to fileNames for single value.
    else {
      props["fileName"] = fileNames[0];
    }
  }
  prop = reader->GetProperty("FilePrefix");
  if (prop != nullptr) {
    props["fileName"] = toJson(prop);
  }

  return props;
}

void LoadDataReaction::setFileNameProperties(const QJsonObject& props,
                                             vtkSMProxy* reader)
{
  auto prop = reader->GetProperty("FileName");
  if (prop != nullptr) {
    if (!props.contains("fileName")) {
      qCritical() << "Reader doesn't have 'fileName' property.";
      return;
    }
    tomviz::setProperty(props["fileName"], prop);
  }
  prop = reader->GetProperty("FileNames");
  if (prop != nullptr) {
    if (!props.contains("fileNames") && !props.contains("fileName")) {
      qCritical() << "Reader doesn't have 'fileName' or 'fileNames' property.";
      return;
    }

    if (props.contains("fileNames")) {
      tomviz::setProperty(props["fileNames"], prop);
    } else {
      QJsonArray fileNames;
      fileNames.append(props["fileName"]);
      tomviz::setProperty(fileNames, prop);
    }
  }
  prop = reader->GetProperty("FilePrefix");
  if (prop != nullptr) {
    if (!props.contains("fileName")) {
      qCritical() << "Reader doesn't have 'fileName' property.";
      return;
    }
    tomviz::setProperty(props["fileName"], prop);
  }
}

MoleculeSource* LoadDataReaction::loadMolecule(QStringList fileNames,
                                               const QJsonObject& options)
{
  bool addToRecent = options["addToRecent"].toBool(true);
  bool defaultModules = options["defaultModules"].toBool(true);
  vtkMolecule* molecule = vtkMolecule::New();
  foreach (auto fileName, fileNames) {
    vtkNew<vtkXYZMolReader2> reader;
    vtkNew<vtkMolecule> tmpMolecule;
    reader->SetFileName(fileName.toLatin1().data());
    reader->SetOutput(tmpMolecule);
    reader->Update();
    for (int i = 0; i < tmpMolecule->GetNumberOfAtoms(); ++i) {
      vtkAtom atom = tmpMolecule->GetAtom(i);
      molecule->AppendAtom(atom.GetAtomicNumber(), atom.GetPosition());
    }
  }
  auto moleculeSource = new MoleculeSource(molecule);
  moleculeSource->setFileNames(fileNames);
  ModuleManager::instance().addMoleculeSource(moleculeSource);
  if (moleculeSource && defaultModules) {
    auto view = ActiveObjects::instance().activeView();
    ModuleManager::instance().createAndAddModule("Molecule", moleculeSource,
                                                 view);
  }
  if (moleculeSource && addToRecent) {
    RecentFilesMenu::pushMoleculeReader(moleculeSource);
  }
  return moleculeSource;
}

} // end of namespace tomviz
