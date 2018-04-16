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
#include "Pipeline.h"
#include "PythonGeneratedDatasetReaction.h"
#include "Utilities.h"
#include "tomvizConfig.h"

#include <pqActiveObjects.h>
#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqDeleteReaction.h>
#include <pqPVApplicationCore.h>

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkPVRenderView.h>
#include <vtkPVXMLElement.h>
#include <vtkPVXMLParser.h>
#include <vtkRenderer.h>
#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMProxyLocator.h>
#include <vtkSMProxyManager.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMRepresentationProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTimeKeeper.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkStdString.h>

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QMap>
#include <QMessageBox>
#include <QMultiMap>
#include <QPointer>
#include <QSet>

#include <sstream>

namespace tomviz {

class ModuleManager::MMInternals
{
public:
  // TODO Should only hold top level roots of pipeline
  QList<QPointer<DataSource>> DataSources;
  QList<QPointer<DataSource>> ChildDataSources;
  QList<QPointer<Module>> Modules;
  QMap<vtkSMProxy*, vtkSmartPointer<vtkCamera>> RenderViewCameras;

  // Map from view proxies to modules. Used to keep track of how many modules
  // have been added to a view.
  QMultiMap<vtkSMProxy*, Module*> ViewModules;

  // Only used by onPVStateLoaded for the second half of deserialize
  QDir dir;
  QMap<vtkTypeUInt32, vtkSMViewProxy*> ViewIdMap;
  void relativeFilePaths(DataSource* ds, const QDir& stateDir,
                         QJsonObject& dataSourceState)
  {
    QJsonObject readerProps;
    if (dataSourceState.contains("reader") &&
        dataSourceState["reader"].isObject()) {
      readerProps = dataSourceState["reader"].toObject();
    }

    // Make any reader fileName properties relative to the state file being
    // written.
    if (readerProps.contains("fileName")) {
      // Exclude transient data sources.
      // ( ones without a file. i.e. output data sources )
      if (!ds->isTransient()) {
        auto fileName = readerProps["fileName"].toString();
        readerProps["fileName"] = stateDir.relativeFilePath(fileName);
      }
      dataSourceState["reader"] = readerProps;
    } else if (readerProps.contains("fileNames")) {
      // Exclude transient data sources.
      // ( ones without a file. i.e. output data sources )
      if (!ds->isTransient()) {
        auto fileNames = readerProps["fileNames"].toArray();
        QJsonArray relativeNames;
        foreach (QJsonValue name, fileNames) {
          relativeNames.append(stateDir.relativeFilePath(name.toString()));
        }
        readerProps["fileNames"] = relativeNames;
      }
      dataSourceState["reader"] = readerProps;
    }
  }

  void absoluteFilePaths(QJsonObject& dataSourceState)
  {

    std::function<QString(QString)> absolute = [this](QString path) {
      if (!path.isEmpty()) {
        path = this->dir.absoluteFilePath(path);
      }

      return path;
    };

    if (dataSourceState.contains("reader") &&
        dataSourceState["reader"].isObject()) {
      auto reader = dataSourceState["reader"].toObject();
      if (reader.contains("fileName") && reader["fileName"].isString()) {
        reader["fileName"] = absolute(reader["fileName"].toString());
      } else if (reader.contains("fileNames") &&
                 reader["fileNames"].isArray()) {
        auto fileNames = reader["fileNames"].toArray();
        QJsonArray absoluteFileNames;
        foreach (const QJsonValue& path, fileNames) {
          absoluteFileNames.append(absolute(path.toString()));
        }
        reader["fileNames"] = absoluteFileNames;
      }
      dataSourceState["reader"] = reader;
    }
  }
};

ModuleManager::ModuleManager(QObject* parentObject)
  : Superclass(parentObject), d(new ModuleManager::MMInternals())
{
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          SIGNAL(viewRemoved(pqView*)), SLOT(onViewRemoved(pqView*)));
}

ModuleManager::~ModuleManager() = default;

ModuleManager& ModuleManager::instance()
{
  static ModuleManager theInstance;
  return theInstance;
}

void ModuleManager::reset()
{
  removeAllModules();
  removeAllDataSources();
  pqDeleteReaction::deleteAll();
}

bool ModuleManager::hasRunningOperators()
{
  for (QPointer<DataSource> dsource : d->DataSources) {
    if (dsource->pipeline()->isRunning()) {
      return true;
    }
  }
  return false;
}

void ModuleManager::addDataSource(DataSource* dataSource)
{
  if (dataSource && !d->DataSources.contains(dataSource)) {
    d->DataSources.push_back(dataSource);
    emit dataSourceAdded(dataSource);
  }
}

void ModuleManager::addChildDataSource(DataSource* dataSource)
{
  if (dataSource && !d->ChildDataSources.contains(dataSource)) {
    d->ChildDataSources.push_back(dataSource);
    emit childDataSourceAdded(dataSource);
  }
}

void ModuleManager::removeDataSource(DataSource* dataSource)
{
  if (d->DataSources.removeOne(dataSource) ||
      d->ChildDataSources.removeOne(dataSource)) {
    emit dataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
}

void ModuleManager::removeChildDataSource(DataSource* dataSource)
{
  if (d->ChildDataSources.removeOne(dataSource)) {
    emit childDataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
}

void ModuleManager::removeAllDataSources()
{
  foreach (DataSource* dataSource, d->DataSources) {
    emit dataSourceRemoved(dataSource);
    dataSource->deleteLater();
  }
  d->DataSources.clear();
}

bool ModuleManager::isChild(DataSource* source) const
{
  return (d->ChildDataSources.indexOf(source) >= 0);
}

void ModuleManager::addModule(Module* module)
{
  if (!d->Modules.contains(module)) {
    module->setParent(this);
    d->Modules.push_back(module);

    // Reset display if this is the first module in the view.
    if (d->ViewModules.count(module->view()) == 0) {
      auto pqview = tomviz::convert<pqView*>(module->view());
      pqview->resetDisplay();
      pqview->render();
    }
    d->ViewModules.insert(module->view(), module);

    emit moduleAdded(module);
    connect(module, &Module::renderNeeded, this, &ModuleManager::render);
  }
}

void ModuleManager::removeModule(Module* module)
{
  if (d->Modules.removeOne(module)) {
    d->ViewModules.remove(module->view(), module);
    emit moduleRemoved(module);
    module->deleteLater();
  }
}

void ModuleManager::removeAllModules()
{
  foreach (Module* module, d->Modules) {
    emit moduleRemoved(module);
    module->deleteLater();
  }
  d->Modules.clear();
}

void ModuleManager::removeAllModules(DataSource* source)
{
  Q_ASSERT(source);
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
    if (module->dataSource() == source) {
      modules.push_back(module);
    }
  }
  foreach (Module* module, modules) {
    removeModule(module);
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
  auto module = ModuleFactory::createModule(type, dataSource, view);
  if (module) {
    addModule(module);
  }
  return module;
}

QList<Module*> ModuleManager::findModulesGeneric(const DataSource* dataSource,
                                                 const vtkSMViewProxy* view)
{
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
    if (module && module->dataSource() == dataSource &&
        (view == nullptr || view == module->view())) {
      modules.push_back(module);
    }
  }
  return modules;
}

QJsonArray jsonArrayFromXml(pugi::xml_node node)
{
  // Simple function, just iterates through the elements and fills the array.
  // This assumes the ParaView Element nodes, values stored in value attributes.
  QJsonArray array;
  for (auto element = node.child("Element"); element;
       element = element.next_sibling("Element")) {
    array.append(element.attribute("value").as_int(-1));
  }
  return array;
}

QJsonArray jsonArrayFromXmlDouble(pugi::xml_node node)
{
  // Simple function, just iterates through the elements and fills the array.
  // This assumes the ParaView Element nodes, values stored in value attributes.
  QJsonArray array;
  for (auto element = node.child("Element"); element;
       element = element.next_sibling("Element")) {
    array.append(element.attribute("value").as_double(-1));
  }
  return array;
}

bool ModuleManager::serialize(QJsonObject& doc, const QDir& stateDir,
                              bool interactive) const
{
  QJsonObject tvObj;
  tvObj["version"] = QString(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0) {
    tvObj["versionExtra"] = QString(TOMVIZ_VERSION_EXTRA);
  }

  auto pvVer = QString("%1.%2.%3")
                 .arg(vtkSMProxyManager::GetVersionMajor())
                 .arg(vtkSMProxyManager::GetVersionMinor())
                 .arg(vtkSMProxyManager::GetVersionPatch());
  tvObj["paraViewVersion"] = pvVer;

  doc["tomviz"] = tvObj;

  if (interactive) {
    // Iterate over all data sources and check is there are any that are not
    // currently saved.
    int modified = 0;
    foreach (const QPointer<DataSource>& ds, d->DataSources) {
      if (ds != nullptr &&
          ds->persistenceState() == DataSource::PersistenceState::Modified) {
        ++modified;
      }
    }
    foreach (const QPointer<DataSource>& ds, d->ChildDataSources) {
      if (ds != nullptr &&
          ds->persistenceState() == DataSource::PersistenceState::Modified) {
        ++modified;
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

  QJsonArray jDataSources;
  foreach (DataSource* ds, d->DataSources) {
    auto jDataSource = ds->serialize();
    if (ds == ActiveObjects::instance().activeDataSource()) {
      jDataSource["active"] = true;
    }
    QJsonArray jModules;
    // Now serialize each of the modules.
    for (int i = 0; i < d->Modules.size(); ++i) {
      const QPointer<Module>& mdl = d->Modules[i];
      if (mdl && mdl->dataSource() == ds) {
        QJsonObject jModule = mdl->serialize();
        jModule["type"] = ModuleFactory::moduleType(mdl);
        jModule["viewId"] = static_cast<int>(mdl->view()->GetGlobalID());
        jModule["id"] = i;
        if (mdl == ActiveObjects::instance().activeModule()) {
          jModule["active"] = true;
        }

        jModules.append(jModule);
      }
    }
    if (!jModules.isEmpty()) {
      jDataSource["modules"] = jModules;
    }

    d->relativeFilePaths(ds, stateDir, jDataSource);

    jDataSources.append(jDataSource);
  }
  doc["dataSources"] = jDataSources;

  // Now serialize the views and layouts.
  vtkNew<vtkSMProxyIterator> iter;
  iter->SetSessionProxyManager(ActiveObjects::instance().proxyManager());
  iter->SetModeToOneGroup();
  QJsonArray jLayouts;
  for (iter->Begin("layouts"); !iter->IsAtEnd(); iter->Next()) {
    if (vtkSMProxy* layout = iter->GetProxy()) {
      QJsonObject jLayout;
      jLayout["id"] = static_cast<int>(layout->GetGlobalID());
      jLayout["xmlGroup"] = layout->GetXMLGroup();
      jLayout["xmlName"] = layout->GetXMLName();

      // I suspect this is a huge amount of overkill to get the servers...
      pugi::xml_document document;
      auto proxyNode = document.append_child("ParaViewXML");
      tomviz::serialize(layout, proxyNode);
      auto layoutProxy = document.child("ParaViewXML").child("Proxy");
      jLayout["servers"] = layoutProxy.attribute("servers").as_int(0);
      // Iterate through the layout nodes.
      QJsonArray layoutArray;
      for (auto node = layoutProxy.child("Layout"); node;
           node = node.next_sibling("Layout")) {
        QJsonArray itemArray;
        for (auto itemNode = node.child("Item"); itemNode;
             itemNode = itemNode.next_sibling("Item")) {
          QJsonObject itemObj;
          itemObj["direction"] = itemNode.attribute("direction").as_int(0);
          itemObj["fraction"] = itemNode.attribute("fraction").as_double(0);
          itemObj["viewId"] = itemNode.attribute("view").as_int(0);
          itemArray.append(itemObj);
        }
        layoutArray.append(itemArray);
      }
      jLayout["items"] = layoutArray;
      jLayouts.append(jLayout);
    }
  }
  if (!jLayouts.isEmpty()) {
    doc["layouts"] = jLayouts;
  }
  QJsonArray jViews;
  for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
    if (vtkSMProxy* view = iter->GetProxy()) {
      QJsonObject jView;
      jView["id"] = static_cast<int>(view->GetGlobalID());
      jView["xmlGroup"] = view->GetXMLGroup();
      jView["xmlName"] = view->GetXMLName();
      if (view == ActiveObjects::instance().activeView()) {
        jView["active"] = true;
      }

      // Now to get some more specific information about the view!
      pugi::xml_document document;
      pugi::xml_node proxyNode = document.append_child("ParaViewXML");
      tomviz::serialize(view, proxyNode);

      QJsonObject camera;

      // Curate the pieces we want from the XML produced.
      auto viewProxy = document.child("ParaViewXML").child("Proxy");
      jView["servers"] = viewProxy.attribute("servers").as_int(0);
      QJsonArray backgroundColor;
      // Iterate through the properties...
      for (pugi::xml_node node = viewProxy.child("Property"); node;
           node = node.next_sibling("Property")) {
        std::string name = node.attribute("name").as_string("");
        if (name == "ViewSize") {
          jView["viewSize"] = jsonArrayFromXml(node);
        } else if (name == "CameraFocalPoint") {
          camera["focalPoint"] = jsonArrayFromXmlDouble(node);
        } else if (name == "CameraPosition") {
          camera["position"] = jsonArrayFromXmlDouble(node);
        } else if (name == "CameraViewUp") {
          camera["viewUp"] = jsonArrayFromXmlDouble(node);
        } else if (name == "CameraViewAngle") {
          camera["viewAngle"] = jsonArrayFromXmlDouble(node)[0];
        } else if (name == "EyeAngle") {
          camera["eyeAngle"] = jsonArrayFromXmlDouble(node)[0];
        } else if (name == "CenterOfRotation") {
          jView["centerOfRotation"] = jsonArrayFromXmlDouble(node);
        } else if (name == "Background") {
          backgroundColor.append(jsonArrayFromXmlDouble(node));
        } else if (name == "Background2") {
          vtkSMPropertyHelper helper(view, "UseGradientBackground");
          if (helper.GetAsInt()) {
            backgroundColor.append(jsonArrayFromXmlDouble(node));
          }
        }
      }
      jView["camera"] = camera;
      jView["backgroundColor"] = backgroundColor;

      jViews.append(jView);
      /*
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
      */
    }
  }
  if (!jViews.isEmpty()) {
    doc["views"] = jViews;
  }

  return true;
}

void createXmlProperty(pugi::xml_node& n, const char* name, int id)
{
  n.set_name("Property");
  n.append_attribute("name").set_value(name);
  QString idStr = QString::number(id) + "." + name;
  n.append_attribute("id").set_value(idStr.toStdString().c_str());
}

template <typename T>
void createXmlProperty(pugi::xml_node& n, const char* name, int id, T value)
{
  createXmlProperty(n, name, id);
  n.append_attribute("number_of_elements").set_value(1);
  auto element = n.append_child("Element");
  element.append_attribute("index").set_value(0);
  element.append_attribute("value").set_value(value);
}

void createXmlProperty(pugi::xml_node& n, const char* name, int id,
                       QJsonArray arr)
{
  createXmlProperty(n, name, id);
  n.append_attribute("number_of_elements").set_value(arr.size());
  for (int i = 0; i < arr.size(); ++i) {
    auto element = n.append_child("Element");
    element.append_attribute("index").set_value(i);
    element.append_attribute("value").set_value(arr[i].toDouble(-1));
  }
}

void createXmlLayout(pugi::xml_node& n, QJsonArray arr)
{
  n.set_name("Layout");
  n.append_attribute("number_of_elements").set_value(arr.size());
  for (int i = 0; i < arr.size(); ++i) {
    auto obj = arr[i].toObject();
    auto item = n.append_child("Item");
    item.append_attribute("direction").set_value(obj["direction"].toInt());
    item.append_attribute("fraction").set_value(obj["fraction"].toDouble());
    item.append_attribute("view").set_value(obj["viewId"].toInt());
  }
}

bool ModuleManager::deserialize(const QJsonObject& doc, const QDir& stateDir)
{
  // Get back to a known state.
  reset();

  // High level game plan - construct some XML for ParaView, restore the
  // layouts, the views, links, etc. Once they are ready then restore the data
  // pipeline, using the nested layout to assure the order is correct.
  QJsonArray views = doc["views"].toArray();
  QJsonArray layouts = doc["layouts"].toArray();
  QJsonArray links = doc["links"].toArray();

  // ParaView must load all views and layouts first.
  pugi::xml_document document;
  auto pvxml = document.append_child("ParaView");
  auto pvState = pvxml.append_child("ServerManagerState");
  pvState.append_attribute("version").set_value(
    QString("%1.%2.%3")
      .arg(vtkSMProxyManager::GetVersionMajor())
      .arg(vtkSMProxyManager::GetVersionMinor())
      .arg(vtkSMProxyManager::GetVersionPatch())
      .toLatin1()
      .data());
  auto pvViews = pvState.append_child("ProxyCollection");
  pvViews.append_attribute("name").set_value("views");
  auto pvLayouts = pvState.append_child("ProxyCollection");
  pvLayouts.append_attribute("name").set_value("layouts");
  int numViews = 0, numLayouts = 0;

  // First see if we have views, and unpack them.
  for (int i = 0; i < views.size(); ++i) {
    QJsonObject view = views[i].toObject();
    auto viewId = view["id"].toInt();
    auto proxyNode = pvState.append_child("Proxy");
    proxyNode.append_attribute("group").set_value("views");
    proxyNode.append_attribute("type").set_value("RenderView");
    proxyNode.append_attribute("id").set_value(viewId);
    proxyNode.append_attribute("servers").set_value(view["servers"].toInt());

    auto propNode = proxyNode.append_child("Property");
    createXmlProperty(propNode, "CenterOfRotation", viewId,
                      view["centerOfRotation"].toArray());
    // Let's do the camera now...
    QJsonObject camera = view["camera"].toObject();
    propNode = proxyNode.append_child("Property");
    createXmlProperty(propNode, "CameraFocalPoint", viewId,
                      camera["focalPoint"].toArray());

    if (view.contains("backgroundColor")) {
      auto backgroundColor = view["backgroundColor"].toArray();
      // Restore the background color
      propNode = proxyNode.append_child("Property");
      createXmlProperty(propNode, "Background", viewId,
                        backgroundColor.at(0).toArray());

      // If we have more than one element, we have a gradient so also restore
      // Background2 and set UseGradientBackground.
      if (backgroundColor.size() > 1) {
        propNode = proxyNode.append_child("Property");
        createXmlProperty(propNode, "Background2", viewId,
                          backgroundColor.at(1).toArray());
        propNode = proxyNode.append_child("Property");
        createXmlProperty(propNode, "UseGradientBackground", viewId, 1);
      }
    }

    // Create an entry in the views thing...
    pugi::xml_node viewSummary = pvViews.append_child("Item");
    viewSummary.append_attribute("id").set_value(viewId);
    viewSummary.append_attribute("name").set_value(
      QString("View%1").arg(++numViews).toStdString().c_str());
  }
  // Now the layouts - should only ever be one, but go through the motions...
  for (int i = 0; i < layouts.size(); ++i) {
    QJsonObject layout = layouts[i].toObject();
    auto layoutId = layout["id"].toInt();
    auto proxyNode = pvState.append_child("Proxy");
    proxyNode.append_attribute("group").set_value("misc");
    proxyNode.append_attribute("type").set_value("ViewLayout");
    proxyNode.append_attribute("id").set_value(layoutId);
    proxyNode.append_attribute("servers").set_value(layout["servers"].toInt());

    QJsonArray items = layout["items"].toArray();
    for (int j = 0; j < items.size(); ++j) {
      auto layoutNode = proxyNode.append_child("Layout");
      createXmlLayout(layoutNode, items[j].toArray());
    }

    // Create an entry in the layouts thing...
    auto layoutSummary = pvLayouts.append_child("Item");
    layoutSummary.append_attribute("id").set_value(layoutId);
    layoutSummary.append_attribute("name").set_value(
      QString("Layout%1").arg(++numLayouts).toStdString().c_str());
  }

  d->dir = stateDir;
  m_stateObject = doc;
  connect(pqApplicationCore::instance(),
          SIGNAL(stateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)),
          SLOT(onPVStateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)));
  // Set up call to ParaView to load state
  std::ostringstream stream;
  document.first_child().print(stream);

  // qDebug() << "\nPV XML:" << stream.str().c_str() << "\n";
  vtkNew<vtkPVXMLParser> parser;
  if (!parser->Parse(stream.str().c_str())) {
    return false;
  }
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  pqServer* server = activeObjects->activeServer();

  pqApplicationCore::instance()->loadState(parser->GetRootElement(), server);
  // Clean up the state -- since the Qt slot call should be synchronous
  // it should be done before the code returns to here.
  disconnect(pqApplicationCore::instance(),
             SIGNAL(stateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)), this,
             SLOT(onPVStateLoaded(vtkPVXMLElement*, vtkSMProxyLocator*)));

  d->dir = QDir();
  m_stateObject = QJsonObject();

  // Now to restore all of our cameras...
  for (int i = 0; i < views.size(); ++i) {
    auto view = views[i].toObject();
    auto viewProxy =
      vtkSMRenderViewProxy::SafeDownCast(lookupView(view["id"].toInt()));
    if (!viewProxy) {
      continue;
    }
    double tmp[3];
    auto camera = view["camera"].toObject();
    auto jsonArray = camera["position"].toArray();
    for (int j = 0; j < jsonArray.size(); ++j) {
      tmp[j] = jsonArray[j].toDouble();
    }
    vtkSMPropertyHelper(viewProxy, "CameraPosition").Set(tmp, 3);
    jsonArray = camera["focalPoint"].toArray();
    for (int j = 0; j < jsonArray.size(); ++j) {
      tmp[j] = jsonArray[j].toDouble();
    }
    vtkSMPropertyHelper(viewProxy, "CameraFocalPoint").Set(tmp, 3);
    jsonArray = camera["viewUp"].toArray();
    for (int j = 0; j < jsonArray.size(); ++j) {
      tmp[j] = jsonArray[j].toDouble();
    }
    vtkSMPropertyHelper(viewProxy, "CameraViewUp").Set(tmp, 3);
    vtkSMPropertyHelper(viewProxy, "CameraViewAngle")
      .Set(camera["viewAngle"].toDouble());
    vtkSMPropertyHelper(viewProxy, "EyeAngle")
      .Set(camera["EyeAngle"].toDouble());
    viewProxy->UpdateVTKObjects();
  }

  return true;
}

void ModuleManager::onPVStateLoaded(vtkPVXMLElement*,
                                    vtkSMProxyLocator* locator)
{
  auto pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  // Populate the view id map, needed to create modules with the restored views.
  d->ViewIdMap.clear();
  if (m_stateObject["views"].isArray()) {
    auto viewArray = m_stateObject["views"].toArray();
    for (int i = 0; i < viewArray.size(); ++i) {
      auto view = viewArray[i].toObject();
      auto viewId = view["id"].toInt();
      d->ViewIdMap[viewId] =
        vtkSMViewProxy::SafeDownCast(locator->LocateProxy(viewId));
    }
  }

  // Load up all of the data sources.
  if (m_stateObject["dataSources"].isArray()) {
    auto dataSources = m_stateObject["dataSources"].toArray();
    foreach (auto ds, dataSources) {
      auto dsObject = ds.toObject();
      QJsonObject options;
      options["defaultModules"] = false;
      options["addToRecent"] = false;
      options["child"] = false;
      d->absoluteFilePaths(dsObject);

      QStringList fileNames;
      if (dsObject.contains("reader")) {
        auto reader = dsObject["reader"].toObject();
        options["reader"] = reader;

        if (reader.contains("fileName")) {
          auto fileName = reader["fileName"].toString();
          if (!fileName.isEmpty()) {
            fileNames << fileName;
          }
        } else if (reader.contains("fileNames")) {
          foreach (const QJsonValue& value, reader["fileNames"].toArray()) {
            auto fileName = value.toString();
            if (fileName.isEmpty()) {
              fileNames << fileName;
            }
          }
        } else {
          qCritical() << "Unable to locate file name.";
        }
      }

      DataSource* dataSource;
      if (dsObject.find("sourceInformation") != dsObject.end()) {
        dataSource = PythonGeneratedDatasetReaction::createDataSource(
          dsObject["sourceInformation"].toObject());
        LoadDataReaction::dataSourceAdded(dataSource, false, false);
      } else {
        dataSource = LoadDataReaction::loadData(fileNames, options);
      }

      dataSource->deserialize(dsObject);
      if (fileNames.isEmpty()) {
        dataSource->setPersistenceState(
          DataSource::PersistenceState::Transient);
      }
      // FIXME: I think we need to collect the active objects and set them at
      // the end, as the act of adding generally implies setting to active.
      if (dsObject["active"].toBool()) {
        ActiveObjects::instance().setActiveDataSource(dataSource);
      }
    }
  }
}

void ModuleManager::onViewRemoved(pqView* view)
{
  Q_ASSERT(view);
  auto viewProxy = view->getViewProxy();
  QList<Module*> modules;
  foreach (Module* module, d->Modules) {
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
  auto view = tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

vtkSMViewProxy* ModuleManager::lookupView(int id)
{
  return d->ViewIdMap.value(id);
}

} // end of namesapce tomviz
