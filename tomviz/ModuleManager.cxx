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
#include "ModuleManager.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"
#include "Module.h"
#include "ModuleFactory.h"
#include "PythonGeneratedDatasetReaction.h"
#include "Utilities.h"
#include "tomvizConfig.h"

#include "pqActiveObjects.h"
#include "pqAnimationCue.h"
#include "pqAnimationManager.h"
#include "pqAnimationScene.h"
#include "pqApplicationCore.h"
#include "pqDeleteReaction.h"
#include "pqPVApplicationCore.h"

#include "vtkCamera.h"
#include "vtkNew.h"
#include "vtkPVRenderView.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkRenderer.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyIterator.h"
#include "vtkSMProxyLocator.h"
#include "vtkSMProxyManager.h"
#include "vtkSMRenderViewProxy.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMTimeKeeper.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"
#include "vtkStdString.h"

#include <QDir>
#include <QMap>
#include <QMessageBox>
#include <QMultiMap>
#include <QPointer>
#include <QSet>
#include <QtDebug>

#include <sstream>

namespace tomviz {

class ModuleManager::MMInternals
{
public:
  QList<QPointer<DataSource>> DataSources;
  QList<QPointer<DataSource>> ChildDataSources;
  QList<QPointer<Module>> Modules;
  QMap<vtkSMProxy*, vtkSmartPointer<vtkCamera>> RenderViewCameras;

  // Map from view proxies to modules. Used to keep track of how many modules
  // have been added to a view.
  QMultiMap<vtkSMProxy*, Module*> ViewModules;

  // only used by onPVStateLoaded for the second half of deserialize
  pugi::xml_node node;
  QDir dir;

  QMap<vtkTypeUInt32, DataSource*> DataSourceIdMap;
};

ModuleManager::ModuleManager(QObject* parentObject)
  : Superclass(parentObject), Internals(new ModuleManager::MMInternals())
{
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          SIGNAL(viewRemoved(pqView*)), SLOT(onViewRemoved(pqView*)));
}

ModuleManager::~ModuleManager()
{
  // Internals is a QScopedPointer.
}

ModuleManager& ModuleManager::instance()
{
  static ModuleManager theInstance;
  return theInstance;
}

void ModuleManager::reset()
{
  this->removeAllModules();
  this->removeAllDataSources();
  pqDeleteReaction::deleteAll();
}

bool ModuleManager::hasRunningOperators()
{
  for (QPointer<DataSource> dsource : this->Internals->DataSources) {
    if (dsource->isRunningAnOperator()) {
      return true;
    }
  }
  return false;
}

void ModuleManager::addDataSource(DataSource* dataSource)
{
  if (dataSource && !this->Internals->DataSources.contains(dataSource)) {
    dataSource->setParent(this);
    this->Internals->DataSources.push_back(dataSource);
    emit this->dataSourceAdded(dataSource);
  }
}

void ModuleManager::addChildDataSource(DataSource* dataSource)
{
  if (dataSource && !this->Internals->ChildDataSources.contains(dataSource)) {
    dataSource->setParent(this);
    this->Internals->ChildDataSources.push_back(dataSource);
    emit this->childDataSourceAdded(dataSource);
  }
}

void ModuleManager::removeDataSource(DataSource* dataSource)
{
  if (this->Internals->DataSources.removeOne(dataSource)) {
    emit this->dataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
}

void ModuleManager::removeChildDataSource(DataSource* dataSource)
{
  if (this->Internals->ChildDataSources.removeOne(dataSource)) {
    emit this->childDataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
}

void ModuleManager::removeAllDataSources()
{
  foreach (DataSource* dataSource, this->Internals->DataSources) {
    emit this->dataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
  this->Internals->DataSources.clear();
}

bool ModuleManager::isChild(DataSource* source) const
{
  return (this->Internals->ChildDataSources.indexOf(source) >= 0);
}

void ModuleManager::addModule(Module* module)
{
  if (!this->Internals->Modules.contains(module)) {
    module->setParent(this);
    this->Internals->Modules.push_back(module);

    // Reset display if this is the first module in the view.
    if (this->Internals->ViewModules.count(module->view()) == 0) {
      auto pqview = tomviz::convert<pqView*>(module->view());
      pqview->resetDisplay();
      pqview->render();
    }
    this->Internals->ViewModules.insert(module->view(), module);

    emit this->moduleAdded(module);
    connect(module, &Module::renderNeeded, this, &ModuleManager::render);
  }
}

void ModuleManager::removeModule(Module* module)
{
  if (this->Internals->Modules.removeOne(module)) {
    this->Internals->ViewModules.remove(module->view(), module);
    emit this->moduleRemoved(module);
    module->deleteLater();
  }
}

void ModuleManager::removeAllModules()
{
  foreach (Module* module, this->Internals->Modules) {
    emit this->moduleRemoved(module);
    module->deleteLater();
  }
  this->Internals->Modules.clear();
}

void ModuleManager::removeAllModules(DataSource* source)
{
  Q_ASSERT(source);
  QList<Module*> modules;
  foreach (Module* module, this->Internals->Modules) {
    if (module->dataSource() == source) {
      modules.push_back(module);
    }
  }
  foreach (Module* module, modules) {
    this->removeModule(module);
  }
}

Module* ModuleManager::createAndAddModule(const QString& type,
                                          DataSource* dataSource,
                                          vtkSMViewProxy* view)
{
  if (!view || !dataSource) {
    return nullptr;
  }

  // Create an outline module for the source in the active view.
  Module* module = ModuleFactory::createModule(type, dataSource, view);
  if (module) {
    this->addModule(module);
  }
  return module;
}

QList<Module*> ModuleManager::findModulesGeneric(DataSource* dataSource,
                                                 vtkSMViewProxy* view)
{
  QList<Module*> modules;
  foreach (Module* module, this->Internals->Modules) {
    if (module && module->dataSource() == dataSource &&
        (view == nullptr || view == module->view())) {
      modules.push_back(module);
    }
  }
  return modules;
}

bool ModuleManager::serialize(pugi::xml_node& ns, const QDir& saveDir,
                              bool interactive) const
{
  QSet<vtkSMSourceProxy*> uniqueOriginalSources;

  // Populate some high level version information.
  pugi::xml_node versionInfo = ns.append_child("version");
  versionInfo.append_attribute("full").set_value(
    std::string(TOMVIZ_VERSION).c_str());
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0) {
    versionInfo.append_attribute("extra").set_value(
      std::string(TOMVIZ_VERSION_EXTRA).c_str());
  }

  if (interactive) {
    // Iterate over all data sources and check is there are any that are not
    // currently saved.
    int modified = 0;
    foreach (const QPointer<DataSource>& ds, this->Internals->DataSources) {
      if (ds != nullptr &&
          ds->persistenceState() == DataSource::PersistenceState::Modified) {
        modified++;
      }
    }

    foreach (const QPointer<DataSource>& ds,
             this->Internals->ChildDataSources) {
      if (ds != nullptr &&
          ds->persistenceState() == DataSource::PersistenceState::Modified) {
        modified++;
      }
    }

    if (modified > 0) {

      QMessageBox modifiedMessageBox;
      modifiedMessageBox.setIcon(QMessageBox::Warning);
      QString text = QString("Warning: unsaved data - %1 data source%2")
                       .arg(modified)
                       .arg(modified > 1 ? "s" : "");
      QString infoText =
        "Unsaved data is marked in the pipeline italic text "
        "with an asterisk. You may continue to save the state, "
        "and any unsaved data (along with operators/modules) "
        "will be skipped.";
      modifiedMessageBox.setText(text);
      modifiedMessageBox.setInformativeText(infoText);
      modifiedMessageBox.setStandardButtons(QMessageBox::Save |
                                            QMessageBox::Cancel);
      modifiedMessageBox.setDefaultButton(QMessageBox::Save);

      if (modifiedMessageBox.exec() == QMessageBox::Cancel) {
        return false;
      }
    }
  }

  // Build a list of unique original data sources. These are the data readers.
  foreach (const QPointer<DataSource>& ds,
           this->Internals->ChildDataSources + this->Internals->DataSources) {
    if (ds == nullptr ||
        uniqueOriginalSources.contains(ds->originalDataSource()) ||
        ds->persistenceState() == DataSource::PersistenceState::Modified) {
      continue;
    }
    vtkSMSourceProxy* reader = ds->originalDataSource();
    Q_ASSERT(reader != nullptr);
    pugi::xml_node odsnode = ns.append_child("OriginalDataSource");
    odsnode.append_attribute("id").set_value(reader->GetGlobalIDAsString());
    odsnode.append_attribute("xmlgroup").set_value(reader->GetXMLGroup());
    odsnode.append_attribute("xmlname").set_value(reader->GetXMLName());
    if (tomviz::serialize(reader, odsnode, QStringList(), &saveDir) == false) {
      qWarning() << "Failed to serialize data reader: "
                 << reader->GetGlobalIDAsString();
      ns.remove_child(odsnode);
      continue;
    }
    uniqueOriginalSources.insert(reader);
  }

  // Now serialize each of the data-sources. The data sources don't serialize
  // the original data source since that can be shared among sources.
  QList<DataSource*> serializedDataSources;
  foreach (const QPointer<DataSource>& ds,
           this->Internals->ChildDataSources + this->Internals->DataSources) {
    if (ds && uniqueOriginalSources.contains(ds->originalDataSource()) &&
        ds->persistenceState() == DataSource::PersistenceState::Saved) {
      pugi::xml_node dsnode = ns.append_child("DataSource");
      dsnode.append_attribute("id").set_value(
        ds->producer()->GetGlobalIDAsString());
      dsnode.append_attribute("original_data_source")
        .set_value(ds->originalDataSource()->GetGlobalIDAsString());
      if (ds == ActiveObjects::instance().activeDataSource()) {
        dsnode.append_attribute("active").set_value(1);
      }
      if (isChild(ds)) {
        dsnode.append_attribute("child").set_value(true);
      }
      if (!ds->serialize(dsnode)) {
        qWarning("Failed to serialize DataSource.");
        ns.remove_child(dsnode);
        continue;
      }
      Q_ASSERT(serializedDataSources.contains(ds) == false);
      serializedDataSources.append(ds);
    }
  }

  // Now serialize each of the modules.
  for (int i = 0; i < this->Internals->Modules.size(); ++i) {
    const QPointer<Module>& mdl = this->Internals->Modules[i];
    if (mdl && serializedDataSources.contains(mdl->dataSource())) {
      pugi::xml_node mdlnode = ns.append_child("Module");
      mdlnode.append_attribute("type").set_value(
        ModuleFactory::moduleType(mdl));
      mdlnode.append_attribute("data_source")
        .set_value(mdl->dataSource()->producer()->GetGlobalIDAsString());
      mdlnode.append_attribute("view").set_value(
        mdl->view()->GetGlobalIDAsString());
      mdlnode.append_attribute("module_id").set_value(i);
      if (mdl == ActiveObjects::instance().activeModule()) {
        mdlnode.append_attribute("active").set_value(1);
      }
      if (!mdl->serialize(mdlnode)) {
        qWarning() << "Failed to serialize Module.";
        ns.remove_child(mdlnode);
        continue;
      }
    }
  }

  // save the animations
  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();
  pugi::xml_node sceneNode = ns.append_child("AnimationScene");
  vtkSMPropertyHelper numFrames(scene->getProxy(), "NumberOfFrames");
  vtkSMPropertyHelper duration(scene->getProxy(), "Duration");
  sceneNode.append_attribute("number_of_frames")
    .set_value(numFrames.GetAsInt());
  sceneNode.append_attribute("duration").set_value(duration.GetAsDouble());

  QSet<pqAnimationCue*> cues = scene->getCues();
  foreach (pqAnimationCue* cue, cues) {
    pugi::xml_node cueNode = ns.append_child("Cue");
    vtkSMProxy* animatedProxy = cue->getAnimatedProxy();
    QString helperKey;
    pqProxy* animatedSrcProxy =
      pqProxy::findProxyWithHelper(animatedProxy, helperKey);
    for (int i = 0; i < this->Internals->Modules.size(); ++i) {
      const QPointer<Module>& mdl = this->Internals->Modules[i];
      if (mdl && mdl->isProxyPartOfModule(animatedProxy)) {
        cueNode.append_attribute("module_id").set_value(i);
        Module::serializeAnimationCue(cue, mdl, cueNode);
        break;
      } else if (mdl && animatedSrcProxy &&
                 mdl->isProxyPartOfModule(animatedSrcProxy->getProxy())) {
        cueNode.append_attribute("module_id").set_value(i);
        Module::serializeAnimationCue(cue, mdl, cueNode,
                                      helperKey.toStdString().c_str(),
                                      animatedSrcProxy->getProxy());
        break;
      }
    }
    if (!cueNode.attribute("module_id")) {
      cueNode.append_attribute("module_id").set_value(-1);
      if (cue->getAnimatedProxy() == ActiveObjects::instance().activeView()) {
        cueNode.append_attribute("view_animation").set_value(true);
        Module::serializeAnimationCue(cue, "View", cueNode);
      } else if (vtkSMTimeKeeper::SafeDownCast(
                   cue->getAnimatedProxy()->GetClientSideObject())) {
        cueNode.append_attribute("timekeeper").set_value(true);
        tomviz::serialize(cue->getProxy(), cueNode);
      }
    }
  }

  // Now serialize the views and layouts.
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(ActiveObjects::instance().proxyManager());
  iter->SetModeToOneGroup();
  for (iter->Begin("layouts"); !iter->IsAtEnd(); iter->Next()) {
    if (vtkSMProxy* layout = iter->GetProxy()) {
      pugi::xml_node lnode = ns.append_child("Layout");
      lnode.append_attribute("id").set_value(layout->GetGlobalIDAsString());
      lnode.append_attribute("xmlgroup").set_value(layout->GetXMLGroup());
      lnode.append_attribute("xmlname").set_value(layout->GetXMLName());
      if (!tomviz::serialize(layout, lnode)) {
        qWarning("Failed to serialize layout.");
        ns.remove_child(lnode);
      }
    }
  }
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    if (vtkSMProxy* view = iter->GetProxy()) {
      pugi::xml_node vnode = ns.append_child("View");
      vnode.append_attribute("id").set_value(view->GetGlobalIDAsString());
      vnode.append_attribute("xmlgroup").set_value(view->GetXMLGroup());
      vnode.append_attribute("xmlname").set_value(view->GetXMLName());
      if (view == ActiveObjects::instance().activeView()) {
        vnode.append_attribute("active").set_value(1);
      }
      if (!tomviz::serialize(view, vnode)) {
        qWarning("Failed to serialize view.");
        ns.remove_child(vnode);
      }

      if (view->GetProperty("AxesGrid")) {
        vtkSMProxy* axesGrid =
          vtkSMPropertyHelper(view, "AxesGrid").GetAsProxy();
        pugi::xml_node axesGridNode = vnode.append_child("AxesGrid");
        axesGridNode.append_attribute("id").set_value(
          axesGrid->GetGlobalIDAsString());
        axesGridNode.append_attribute("xmlgroup")
          .set_value(axesGrid->GetXMLGroup());
        axesGridNode.append_attribute("xmlname").set_value(
          axesGrid->GetXMLName());
        if (!tomviz::serialize(axesGrid, axesGridNode)) {
          qWarning("Failed to serialize axes grid");
          ns.remove_child(axesGridNode);
        }
      }
    }
  }

  // Save camera links
  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  vtkNew<vtkPVXMLElement> linksElement;
  linksElement->SetName("Links");
  pxm->SaveRegisteredLinks(linksElement.Get());

  std::ostringstream stream;
  linksElement->PrintXML(stream, vtkIndent());

  std::string linksStr = stream.str();
  ns.append_buffer(linksStr.c_str(), linksStr.size());

  return true;
}

bool ModuleManager::deserialize(const pugi::xml_node& ns, const QDir& stateDir)
{
  this->reset();

  // let ParaView load all views and layouts first.
  pugi::xml_document document;
  pugi::xml_node pvxml = document.append_child("ParaView");
  pugi::xml_node pvState = pvxml.append_child("ServerManagerState");
  pvState.append_attribute("version").set_value(
    QString("%1.%2.%3")
      .arg(vtkSMProxyManager::GetVersionMajor())
      .arg(vtkSMProxyManager::GetVersionMinor())
      .arg(vtkSMProxyManager::GetVersionPatch())
      .toLatin1()
      .data());
  pugi::xml_node pvViews = pvState.append_child("ProxyCollection");
  pvViews.append_attribute("name").set_value("views");
  pugi::xml_node pvLayouts = pvState.append_child("ProxyCollection");
  pvLayouts.append_attribute("name").set_value("layouts");
  int numViews = 0, numLayouts = 0;
  for (pugi::xml_node node = ns.child("View"); node;
       node = node.next_sibling("View")) {
    pugi::xml_node axesGridNode = node.child("AxesGrid");
    pugi::xml_node axesGridProxyNode = axesGridNode.child("Proxy");
    vtkTypeUInt32 axesGridId = axesGridProxyNode.attribute("id").as_uint(0);
    vtkTypeUInt32 id = node.attribute("id").as_uint(0);
    pugi::xml_node proxyNode = node.child("Proxy");
    if (proxyNode) {
      pvState.append_copy(proxyNode);
      pugi::xml_node viewSummary = pvViews.append_child("Item");
      viewSummary.append_attribute("id").set_value(id);
      viewSummary.append_attribute("name").set_value(
        QString("View%1").arg(++numViews).toStdString().c_str());
    }
    if (axesGridProxyNode) {
      pvState.append_copy(axesGridProxyNode);
      pugi::xml_node collection = pvState.append_child("ProxyCollection");
      QString name = QString("pq_helper_proxies.%1").arg(id);
      collection.append_attribute("name").set_value(name.toStdString().c_str());
      pugi::xml_node item = collection.append_child("Item");
      item.append_attribute("id").set_value(axesGridId);
      item.append_attribute("name").set_value("AxesGrid");
    }
  }
  for (pugi::xml_node node = ns.child("Layout"); node;
       node = node.next_sibling("Layout")) {
    vtkTypeUInt32 id = node.attribute("id").as_uint(0);
    pugi::xml_node proxyNode = node.child("Proxy");
    if (proxyNode) {
      pvState.append_copy(proxyNode);
      pugi::xml_node layoutSummary = pvLayouts.append_child("Item");
      layoutSummary.append_attribute("id").set_value(id);
      layoutSummary.append_attribute("name").set_value(
        QString("Layout%1").arg(++numLayouts).toStdString().c_str());
    }
  }
  pugi::xml_node node = ns.child("Links");
  if (node) {
    pvState.append_copy(node);
  }
  // This state and connection is cleaned up by onPVStateLoaded.
  this->Internals->node = ns;
  this->Internals->dir = stateDir;
  this->connect(pqApplicationCore::instance(),
                SIGNAL(stateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)),
                SLOT(onPVStateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)));
  // Set up call to ParaView to load state
  std::ostringstream stream;
  document.first_child().print(stream);
  vtkNew<vtkPVXMLParser> parser;
  if (!parser->Parse(stream.str().c_str())) {
    return false;
  }
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  pqServer* server = activeObjects->activeServer();

  pqApplicationCore::instance()->loadState(parser->GetRootElement(), server);
  // clean up the state -- since the Qt slot call should be synchronous
  // it should be done before the code returns to here.
  this->disconnect(pqApplicationCore::instance(),
                   SIGNAL(stateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)),
                   this,
                   SLOT(onPVStateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)));
  this->Internals->node = pugi::xml_node();
  this->Internals->dir = QDir();

  // Restore cameras for each view to settings stored in state file
  for (QMap<vtkSMProxy*, vtkSmartPointer<vtkCamera>>::const_iterator iter =
         this->Internals->RenderViewCameras.begin();
       iter != this->Internals->RenderViewCameras.end(); ++iter) {
    vtkSMRenderViewProxy* viewProxy =
      vtkSMRenderViewProxy::SafeDownCast(iter.key());
    vtkSmartPointer<vtkCamera> camera = iter.value();

    double doubles[3];
    camera->GetPosition(doubles);
    vtkSMPropertyHelper(viewProxy, "CameraPosition").Set(doubles, 3);
    camera->GetFocalPoint(doubles);
    vtkSMPropertyHelper(viewProxy, "CameraFocalPoint").Set(doubles, 3);
    camera->GetViewUp(doubles);
    vtkSMPropertyHelper(viewProxy, "CameraViewUp").Set(doubles, 3);
    vtkSMPropertyHelper(viewProxy, "CameraViewAngle")
      .Set(camera->GetViewAngle());
    vtkSMPropertyHelper(viewProxy, "EyeAngle").Set(camera->GetEyeAngle());
    viewProxy->UpdateVTKObjects();
  }

  return true;
}

void ModuleManager::onPVStateLoaded(vtkPVXMLElement* vtkNotUsed(xml),
                                    vtkSMProxyLocator* locator)
{
  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  pugi::xml_node& ns = this->Internals->node;

  // process all original data sources i.e. readers and create them.
  QMap<vtkTypeUInt32, vtkSmartPointer<vtkSMSourceProxy>> originalDataSources;
  for (pugi::xml_node odsnode = ns.child("OriginalDataSource"); odsnode;
       odsnode = odsnode.next_sibling("OriginalDataSource")) {
    vtkTypeUInt32 id = odsnode.attribute("id").as_uint(0);
    const char* group = odsnode.attribute("xmlgroup").value();
    const char* type = odsnode.attribute("xmlname").value();
    if (group == nullptr || type == nullptr) {
      qWarning() << "Invalid xml for OriginalDataSource with id " << id;
      continue;
    }

    vtkSmartPointer<vtkSMProxy> proxy;
    proxy.TakeReference(pxm->NewProxy(group, type));
    if (!tomviz::deserialize(proxy, odsnode, &this->Internals->dir)) {
      qWarning() << "Failed to create proxy of type: " << group << ", " << type;
      continue;
    }
    proxy->UpdateVTKObjects();
    if (proxy->GetAnnotation(Attributes::DATASOURCE_FILENAME) ==
        QString("Python Generated Data")) {
      QString label = proxy->GetAnnotation(Attributes::LABEL);
      QString script = proxy->GetAnnotation("tomviz.Python_Source.Script");
      int shape[3];
      shape[0] = std::atoi(proxy->GetAnnotation("tomviz.Python_Source.X"));
      shape[1] = std::atoi(proxy->GetAnnotation("tomviz.Python_Source.Y"));
      shape[2] = std::atoi(proxy->GetAnnotation("tomviz.Python_Source.Z"));
      proxy =
        PythonGeneratedDatasetReaction::getSourceProxy(label, script, shape);
    }
    originalDataSources[id] = vtkSMSourceProxy::SafeDownCast(proxy);
  }

  this->Internals->DataSourceIdMap.clear();
  // now, deserialize all data sources.
  for (pugi::xml_node dsnode = ns.child("DataSource"); dsnode;
       dsnode = dsnode.next_sibling("DataSource")) {
    vtkTypeUInt32 id = dsnode.attribute("id").as_uint(0);
    vtkTypeUInt32 odsid = dsnode.attribute("original_data_source").as_uint(0);
    bool child = dsnode.attribute("child").as_bool(false);
    if (id == 0 || odsid == 0) {
      qWarning() << "Invalid xml for DataSource with id " << id;
      continue;
    }
    if (!originalDataSources.contains(odsid)) {
      qWarning() << "Skipping DataSource with id " << id
                 << " since required OriginalDataSource is missing.";
      continue;
    }

    // create the data source.
    DataSource* dataSource = nullptr;
    vtkSMSourceProxy* srcProxy = originalDataSources[odsid];
    if (srcProxy->GetAnnotation(Attributes::FILENAME)) {
      dataSource = LoadDataReaction::loadData(
        srcProxy->GetAnnotation(Attributes::FILENAME), false, false, child);
    } else {
      dataSource = new DataSource(srcProxy);
    }
    if (!child) {
      this->addDataSource(dataSource);
    } else {
      this->addChildDataSource(dataSource);
    }
    if (!dataSource->deserialize(dsnode)) {
      qWarning() << "Failed to deserialze DataSource with id " << id
                 << ". Skipping it";
      continue;
    }

    this->Internals->DataSourceIdMap[id] = dataSource;

    if (dsnode.attribute("active").as_int(0) == 1) {
      ActiveObjects::instance().setActiveDataSource(dataSource);
    }
  }

  QMap<int, Module*> modulesById;

  // now, deserialize all the modules.
  for (pugi::xml_node mdlnode = ns.child("Module"); mdlnode;
       mdlnode = mdlnode.next_sibling("Module")) {
    const char* type = mdlnode.attribute("type").value();
    vtkTypeUInt32 dsid = mdlnode.attribute("data_source").as_uint(0);
    vtkTypeUInt32 viewid = mdlnode.attribute("view").as_uint(0);
    int moduleId = mdlnode.attribute("module_id").as_int();
    if (this->Internals->DataSourceIdMap[dsid] == nullptr ||
        vtkSMViewProxy::SafeDownCast(locator->LocateProxy(viewid)) == nullptr) {
      qWarning() << "Failed to create module: " << type;
      continue;
    }

    // Create module.
    Module* module = ModuleFactory::createModule(
      type, this->Internals->DataSourceIdMap[dsid],
      vtkSMViewProxy::SafeDownCast(locator->LocateProxy(viewid)));
    if (!module || !module->deserialize(mdlnode)) {
      qWarning() << "Failed to create module: " << type;
      delete module;
      continue;
    }
    this->addModule(module);
    modulesById.insert(moduleId, module);
    if (mdlnode.attribute("active").as_int(0) == 1) {
      ActiveObjects::instance().setActiveModule(module);
    }
  }

  // Save camera settings for each view
  this->Internals->RenderViewCameras.clear();
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(pxm);
  iter->SetModeToOneGroup();
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    vtkSMRenderViewProxy* viewProxy =
      vtkSMRenderViewProxy::SafeDownCast(iter->GetProxy());
    if (viewProxy) {
      vtkSmartPointer<vtkCamera> camera = vtkSmartPointer<vtkCamera>::New();

      std::vector<double> doubles;
      doubles =
        vtkSMPropertyHelper(viewProxy, "CameraPosition").GetDoubleArray();
      camera->SetPosition(&doubles[0]);
      doubles =
        vtkSMPropertyHelper(viewProxy, "CameraFocalPoint").GetDoubleArray();
      camera->SetFocalPoint(&doubles[0]);
      doubles = vtkSMPropertyHelper(viewProxy, "CameraViewUp").GetDoubleArray();
      camera->SetViewUp(&doubles[0]);
      doubles =
        vtkSMPropertyHelper(viewProxy, "CameraViewAngle").GetDoubleArray();
      camera->SetViewAngle(doubles[0]);
      doubles = vtkSMPropertyHelper(viewProxy, "EyeAngle").GetDoubleArray();
      camera->SetEyeAngle(doubles[0]);
      this->Internals->RenderViewCameras.insert(viewProxy, camera);
    }
  }

  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();
  const pugi::xml_node& sceneNode = ns.child("AnimationScene");
  vtkSMPropertyHelper numFrames(scene->getProxy(), "NumberOfFrames");
  vtkSMPropertyHelper duration(scene->getProxy(), "Duration");
  numFrames.Set(sceneNode.attribute("number_of_frames").as_int());
  duration.Set(sceneNode.attribute("duration").as_double());

  for (pugi::xml_node cueNode = ns.child("Cue"); cueNode;
       cueNode = cueNode.next_sibling("Cue")) {
    int idx = cueNode.attribute("module_id").as_int();
    Module* module = modulesById.value(idx, nullptr);
    if (module) {
      Module::deserializeAnimationCue(module, cueNode);
    } else {
      if (cueNode.attribute("view_animation").as_bool()) {
        Module::deserializeAnimationCue(ActiveObjects::instance().activeView(),
                                        cueNode);
      } else if (cueNode.attribute("timekeeper").as_bool()) {
        QSet<pqAnimationCue*> cues = scene->getCues();
        vtkSMProxy* timeKeeperCue = nullptr;
        foreach (pqAnimationCue* cue, cues) {
          if (vtkSMTimeKeeper::SafeDownCast(
                cue->getAnimatedProxy()->GetClientSideObject())) {
            timeKeeperCue = cue->getProxy();
          }
        }
        if (timeKeeperCue) {
          tomviz::deserialize(timeKeeperCue, cueNode);
        }
      }
    }
  }
}

void ModuleManager::onViewRemoved(pqView* view)
{
  Q_ASSERT(view);
  auto viewProxy = view->getViewProxy();
  QList<Module*> modules;
  foreach (Module* module, this->Internals->Modules) {
    if (module->view() == viewProxy) {
      modules.push_back(module);
    }
  }
  foreach (Module* module, modules) {
    this->removeModule(module);
  }
}

void ModuleManager::render()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

DataSource* ModuleManager::lookupDataSource(int id)
{
  return this->Internals->DataSourceIdMap.value(id);
}

} // end of namesapce tomviz
