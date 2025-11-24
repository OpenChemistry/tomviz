/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Utilities.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "tomvizConfig.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqCoreUtilities.h>
#include <pqPVApplicationCore.h>
#include <pqSMAdaptor.h>
#include <pqTimeKeeper.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkPVXMLElement.h>
#include <vtkPVXMLParser.h>
#include <vtkSMNamedPropertyIterator.h>
#include <vtkSMPluginManager.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxyManager.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMUtilities.h>

#include <vtkBoundingBox.h>
#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkCubeAxesActor.h>
#include <vtkImageData.h>
#include <vtkImageSliceMapper.h>
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPeriodicTable.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStringList.h>
#include <vtkTable.h>
#include <vtkTextProperty.h>
#include <vtkTrivialProducer.h>
#include <vtkTuple.h>
#include <vtkVariantArray.h>

#include <array>
#include <sstream>
#include <vector>

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QJsonArray>
#include <QLayout>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

namespace tomviz {

using std::string;

const char* Attributes::TYPE = "tomviz.Type";
const char* Attributes::DATASOURCE_FILENAME = "tomviz.DataSource.FileName";
const char* Attributes::LABEL = "tomviz.Label";
const char* Attributes::FILENAME = "tomviz.filename";
} // namespace tomviz

namespace {

// This pugi::xml_tree_walker converts filenames in the xml to and
// from relative paths.
class XMLFileNameConverter : public pugi::xml_tree_walker
{
  void convertFileName(pugi::xml_attribute& fname)
  {
    if (fname) {
      QString path(fname.value());
      QString newPath;
      if (this->toRelative) {
        newPath = this->rootDir.relativeFilePath(path);
      } else {
        newPath = this->rootDir.absoluteFilePath(path);
      }
      fname.set_value(newPath.toStdString().c_str());
    }
  }

public:
  XMLFileNameConverter(const QDir& dir, bool rel)
    : rootDir(dir), toRelative(rel)
  {}
  bool for_each(pugi::xml_node& node) override
  {
    if (strcmp(node.name(), "Property") == 0) {
      pugi::xml_attribute propName = node.attribute("name");
      if ((strcmp(propName.value(), "FileNames") != 0) &&
          (strcmp(propName.value(), "FileName") != 0) &&
          (strcmp(propName.value(), "FilePrefix") != 0)) {
        return true;
      }
      pugi::xml_node child = node.first_child();
      while (child) {
        if (strcmp(child.name(), "Element") == 0) {
          pugi::xml_attribute xml_fname = child.attribute("value");
          this->convertFileName(xml_fname);
        }
        child = child.next_sibling();
      }
    } else if (strcmp(node.name(), "Annotation") == 0) {
      pugi::xml_attribute key = node.attribute("key");
      if (strcmp(key.value(), tomviz::Attributes::FILENAME) == 0 ||
          strcmp(key.value(), tomviz::Attributes::DATASOURCE_FILENAME) == 0) {
        pugi::xml_attribute xml_fname = node.attribute("value");
        this->convertFileName(xml_fname);
      }
    }
    return true;
  }
  QDir rootDir;
  bool toRelative;
};
} // namespace

namespace tomviz {

namespace {

void createXmlProperty(pugi::xml_node& n, const char* name, int id,
                       QJsonArray arr)
{
  n.set_name("Property");
  n.append_attribute("name").set_value(name);
  QString idStr = QString::number(id) + "." + name;
  n.append_attribute("id").set_value(idStr.toStdString().c_str());
  n.append_attribute("number_of_elements").set_value(arr.size());
  for (int i = 0; i < arr.size(); ++i) {
    auto element = n.append_child("Element");
    element.append_attribute("index").set_value(i);
    element.append_attribute("value").set_value(arr[i].toDouble(-1));
  }
}
} // namespace

QJsonObject serialize(vtkSMProxy* proxy)
{
  // Probe for some known types that can be serialized directly.
  if (auto func = vtkPVDiscretizableColorTransferFunction::SafeDownCast(
        proxy->GetClientSideObject())) {
    return serialize(func);
  }

  return QJsonObject();
}

bool deserialize(vtkSMProxy* proxy, const QJsonObject& json)
{
  if (!proxy) {
    return false;
  }

  if (json.empty()) {
    // Empty state loaded.
    return true;
  }

  if (json.contains("colors")) {
    pugi::xml_document document;
    auto proxyNode = document.append_child("Proxy");

    proxyNode.append_attribute("group").set_value("lookup_tables");
    proxyNode.append_attribute("type").set_value("PVLookupTable");
    auto propNode = proxyNode.append_child("Property");
    createXmlProperty(propNode, "RGBPoints", json["id"].toInt(),
                      json["colors"].toArray());

    if (json.contains("colorSpace")) {
      QMap<QString, int> colorSpaceToIntMap = {
        { "RGB", 0 },       { "HSV", 1 },  { "CIELAB", 2 },
        { "CIEDE2000", 4 }, { "Step", 5 },
      };
      if (colorSpaceToIntMap.contains(json["colorSpace"].toString())) {
        auto propNode2 = proxyNode.append_child("Property");
        auto id = colorSpaceToIntMap[json["colorSpace"].toString()];
        createXmlProperty(propNode2, "ColorSpace", json["id"].toInt(), { id });
      }
    }

    proxyNode.append_attribute("id").set_value(json["id"].toInt());
    proxyNode.append_attribute("servers").set_value(json["servers"].toInt());

    std::ostringstream stream;
    document.first_child().print(stream);
    vtkNew<vtkPVXMLParser> parser;
    if (!parser->Parse(stream.str().c_str())) {
      return false;
    }
    if (proxy->LoadXMLState(parser->GetRootElement(), nullptr) != 0) {
      proxy->UpdateVTKObjects();
    }
  }
  if (json.contains("points")) {
    auto p = vtkSMPropertyHelper(proxy, "ScalarOpacityFunction").GetAsProxy();
    if (!p) {
      return false;
    }

    pugi::xml_document document;
    auto proxyNode = document.append_child("Proxy");

    proxyNode.append_attribute("group").set_value("piecewise_functions");
    proxyNode.append_attribute("type").set_value("PiecewiseFunction");
    auto propNode = proxyNode.append_child("Property");
    createXmlProperty(propNode, "Points", json["id"].toInt(),
                      json["points"].toArray());

    proxyNode.append_attribute("id").set_value(json["id"].toInt());
    proxyNode.append_attribute("servers").set_value(json["servers"].toInt());
    std::ostringstream stream;
    document.first_child().print(stream);
    vtkNew<vtkPVXMLParser> parser;
    if (!parser->Parse(stream.str().c_str())) {
      return false;
    }
    if (p->LoadXMLState(parser->GetRootElement(), nullptr) != 0) {
      p->UpdateVTKObjects();
    }
  }

  return true;
}

QJsonObject serialize(vtkDiscretizableColorTransferFunction* func)
{
  QJsonObject json;
  QJsonArray colorTable;

  if (!func) {
    return json;
  }

  auto opacityFunc = func->GetScalarOpacityFunction();
  json["points"] = serialize(opacityFunc)["points"];

  // The data is of the form x, r, g, b for each point. Iterate through it.
  const int numPoints = func->GetSize() * 4;
  auto ptr = func->GetDataPointer();
  for (int i = 0; i < numPoints; ++i) {
    colorTable.append(ptr[i]);
  }
  json["colors"] = colorTable;

  switch(func->GetColorSpace()) {
    case 0:
      json["colorSpace"] = "RGB";
      break;
    case 1:
      json["colorSpace"] = "HSV";
      break;
    case 2:
      json["colorSpace"] = "CIELAB";
      break;
    case 4:
      json["colorSpace"] = "CIEDE2000";
      break;
    case 5:
      json["colorSpace"] = "Step";
      break;
    default:
      json["colorSpace"] = "Diverging";
  }

  return json;
}

bool deserialize(vtkDiscretizableColorTransferFunction* func,
                 const QJsonObject& json)
{
  if (!func || json.empty()) {
    // Empty state loaded.
    return true;
  }

  if (json.contains("points") && json.contains("colors")) {
    func->RemoveAllPoints();
    auto opacityFunc = func->GetScalarOpacityFunction();
    deserialize(opacityFunc, json);
    auto colors = json["colors"].toArray();
    double* values = new double[colors.size()];
    for (int i = 0; i < colors.size(); ++i) {
      values[i] = colors[i].toDouble();
    }
    func->FillFromDataPointer(colors.size(), values);

    if (json.contains("colorSpace")) {
      QMap<QString, int> colorSpaceToIntMap = {
        { "RGB", 0 },       { "HSV", 1 },  { "CIELAB", 2 },
        { "CIEDE2000", 4 }, { "Step", 5 },
      };
      if (colorSpaceToIntMap.contains(json["colorSpace"].toString())) {
        func->SetColorSpace(colorSpaceToIntMap[json["colorSpace"].toString()]);
      }
    }
    return true;
  }

  return false;
}

QJsonObject serialize(vtkPiecewiseFunction* func)
{
  QJsonObject json;
  QJsonArray pointsTable;

  const int numPoints = func->GetSize();
  for (int pointIdx = 0; pointIdx < numPoints; pointIdx++) {
    double values[4] = { 0.0 };
    func->GetNodeValue(pointIdx, values);
    for (int i = 0; i < 4; i++) {
      pointsTable.append(values[i]);
    }
  }
  json["points"] = pointsTable;

  return json;
}

bool deserialize(vtkPiecewiseFunction* func, const QJsonObject& json)
{
  if (json.empty()) {
    // Empty state loaded.
    return true;
  }

  if (json.contains("points")) {
    auto points = json["points"].toArray();
    func->RemoveAllPoints();
    double values[4];
    for (int pointIdx = 0; pointIdx < points.size(); pointIdx += 4) {
      for (int i = 0; i < 4; i++) {
        values[i] = points.at(pointIdx + i).toDouble();
      }
      func->AddPoint(values[0], values[1], values[2], values[3]);
    }
    return true;
  }

  return false;
}

bool serialize(vtkSMProxy* proxy, pugi::xml_node& out,
               const QStringList& properties, const QDir* relDir)
{
  if (!proxy) {
    return false;
  }

  vtkSmartPointer<vtkSMNamedPropertyIterator> iter;
  if (properties.size() > 0) {
    vtkNew<vtkStringList> pnames;
    foreach (const QString& str, properties) {
      pnames->AddString(str.toLatin1().data());
    }
    iter = vtkSmartPointer<vtkSMNamedPropertyIterator>::New();
    iter->SetPropertyNames(pnames.GetPointer());
    iter->SetProxy(proxy);
  }

  // Save options state -- that's all we need.
  vtkSmartPointer<vtkPVXMLElement> elem;
  elem.TakeReference(proxy->SaveXMLState(nullptr, iter.GetPointer()));

  std::ostringstream stream;
  elem->PrintXML(stream, vtkIndent());

  pugi::xml_document document;
  if (!document.load_string(stream.str().c_str())) {
    qCritical("Failed to convert from vtkPVXMLElement to pugi::xml_document");
    return false;
  }
  if (relDir) {
    QString canonicalPath = relDir->canonicalPath();
    XMLFileNameConverter converter(QDir(canonicalPath), true);
    pugi::xml_node root = document.first_child();
    root.traverse(converter);
  }
  out.append_copy(document.first_child());
  return true;
}

bool deserialize(vtkSMProxy* proxy, const pugi::xml_node& in,
                 const QDir* relDir, vtkSMProxyLocator* locator)
{
  if (!proxy) {
    return false;
  }

  if (!in || !in.first_child()) {
    // empty state loaded.
    return true;
  }

  if (relDir) {
    QString canonicalPath = relDir->canonicalPath();
    XMLFileNameConverter converter(QDir(canonicalPath), false);
    in.first_child().traverse(converter);
  }

  std::ostringstream stream;
  in.first_child().print(stream);
  vtkNew<vtkPVXMLParser> parser;
  if (!parser->Parse(stream.str().c_str())) {
    return false;
  }
  if (proxy->LoadXMLState(parser->GetRootElement(), locator) != 0) {
    proxy->UpdateVTKObjects();
    return true;
  }
  return false;
}

bool serialize(const QVariant& value, pugi::xml_node& out)
{
  switch (value.typeId()) {
    case QMetaType::Int:
      out.append_attribute("type").set_value("int");
      out.append_attribute("value").set_value(value.toInt());
      return true;
    case QMetaType::Double:
      out.append_attribute("type").set_value("double");
      out.append_attribute("value").set_value(value.toDouble());
      return true;
    case QMetaType::Bool:
      out.append_attribute("type").set_value("bool");
      out.append_attribute("value").set_value(value.toBool());
      return true;
    case QMetaType::QString: {
      out.append_attribute("type").set_value("string");
      out.append_attribute("value").set_value(
        value.toString().toLatin1().data());
      return true;
    }
    case QMetaType::QVariantList: {
      out.append_attribute("type").set_value("list");
      QVariantList list = value.toList();
      for (auto itr = list.begin(); itr != list.end(); ++itr) {
        pugi::xml_node child = out.append_child("variant");
        serialize(*itr, child);
      }
      return true;
    }
    default:
      qCritical() << "Unsupported type";
      return false;
  }
}

bool serialize(const QVariantMap& map, pugi::xml_node& out)
{
  bool result = true;
  for (auto itr = map.begin(); itr != map.end(); ++itr) {
    pugi::xml_node child = out.append_child("variant");
    child.append_attribute("name").set_value(itr.key().toLatin1().data());
    result &= serialize(itr.value(), child);
  }
  return result;
}

bool deserialize(QVariant& variant, const pugi::xml_node& in)
{
  QString type = in.attribute("type").as_string();
  if (type == "int") {
    variant = QVariant(in.attribute("value").as_int());
  } else if (type == "double") {
    variant = QVariant(in.attribute("value").as_double());
  } else if (type == "bool") {
    variant = QVariant(in.attribute("value").as_bool());
  } else if (type == "string") {
    variant = QVariant(in.attribute("value").as_string());
  } else if (type == "list") {
    QVariantList list;
    bool result = true;
    for (pugi::xml_node child = in.child("variant"); child;
         child = child.next_sibling("variant")) {
      QVariant tmp;
      result &= deserialize(tmp, child);
      list.push_back(tmp);
    }
    variant = QVariant(list);
    return result;
  } else {
    return false;
  }
  return true;
}

bool deserialize(QVariantMap& map, const pugi::xml_node& in)
{
  bool result = true;
  for (pugi::xml_node child = in.child("variant"); child;
       child = child.next_sibling("variant")) {
    QString key = child.attribute("name").as_string();
    QVariant value;
    result &= deserialize(value, child);
    map.insert(key, value);
  }
  return result;
}

bool serialize(vtkPiecewiseFunction* func, pugi::xml_node& out)
{
  if (!func) {
    return false;
  }

  const int numPoints = func->GetSize();
  pugi::xml_node pointsNode = out.append_child("Points");
  pointsNode.append_attribute("number_of_elements") = numPoints;

  for (int pointIdx = 0; pointIdx < numPoints; pointIdx++) {
    double values[4] = { 0.0 };
    func->GetNodeValue(pointIdx, values);

    for (int valueIdx = 0; valueIdx < 4; valueIdx++) {
      pugi::xml_node elemNode = pointsNode.append_child("Element");
      elemNode.append_attribute("index") = pointIdx * 4 + valueIdx;
      elemNode.append_attribute("value") = values[valueIdx];
    }
  }

  return true;
}

bool deserialize(vtkPiecewiseFunction* func, const pugi::xml_node& in)
{
  if (!func) {
    return false;
  }

  pugi::xml_node pointsNode = in.child("Points");
  if (!pointsNode) {
    return false;
  }

  double values[4];
  int numValues = 0;
  for (pugi::xml_node child = pointsNode.child("Element"); child;
       child = child.next_sibling("Element")) {
    values[numValues] = child.attribute("value").as_double();
    numValues++;

    if (numValues == 4) {
      func->AddPoint(values[0], values[1], values[2], values[3]);
      numValues = 0;
    }
  }

  return true;
}

vtkPVArrayInformation* scalarArrayInformation(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* dinfo = proxy->GetDataInformation();
  return dinfo ? dinfo->GetPointDataInformation()->GetAttributeInformation(
                   vtkDataSetAttributes::SCALARS)
               : nullptr;
}

bool rescaleColorMap(vtkSMProxy* colorMap, DataSource* dataSource)
{
  // rescale the color/opacity maps for the data source.
  vtkSMProxy* cmap = colorMap;
  vtkSMProxy* omap =
    vtkSMPropertyHelper(cmap, "ScalarOpacityFunction").GetAsProxy();
  vtkPVArrayInformation* ainfo =
    tomviz::scalarArrayInformation(dataSource->proxy());
  if (ainfo != nullptr &&
      vtkSMPropertyHelper(cmap, "AutomaticRescaleRangeMode").GetAsInt() !=
        vtkSMTransferFunctionManager::NEVER) {
    vtkSMTransferFunctionProxy::RescaleTransferFunction(
      cmap, ainfo->GetComponentRange(-1));
    vtkSMTransferFunctionProxy::RescaleTransferFunction(
      omap, ainfo->GetComponentRange(-1));
    return true;
  }
  return false;
}

QString readInTextFile(const QString& fileName, const QString& extension)
{
  QString path =
    QApplication::applicationDirPath() + "/../share/tomviz/scripts/";
  path += fileName + extension;
  QFile file(path);
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    return QString(array);
  }
// On OSX the above doesn't work in a build tree.  It is fine
// for superbuilds, but the following is needed in the build tree
// since the executable is three levels down in bin/tomviz.app/Contents/MacOS/
#ifdef __APPLE__
  else {
    path =
      QApplication::applicationDirPath() + "/../../../../share/tomviz/scripts/";
    path += fileName + extension;
    QFile file2(path);
    if (file2.open(QIODevice::ReadOnly)) {
      QByteArray array = file2.readAll();
      return QString(array);
    }
  }
#endif
  qCritical() << "Error: Could not find script file: " << fileName + extension;
  return "raise IOError(\"Couldn't read script file\")\n";
}

QString readInPythonScript(const QString& scriptName)
{
  return readInTextFile(scriptName, ".py");
}

QString readInJSONDescription(const QString& fileName)
{
  return readInTextFile(fileName, ".json");
}

void clearCameraCues(vtkSMRenderViewProxy* renderView)
{
  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();

  for (auto* cue : scene->getCues()) {
    if (!cue->getSMName().startsWith("CameraAnimationCue")) {
      continue;
    }

    vtkSMProxy* animatedProxy = pqSMAdaptor::getProxyProperty(
      cue->getProxy()->GetProperty("AnimatedProxy"));
    if (renderView && animatedProxy != renderView) {
      continue;
    }

    // If we made it this far, we should remove this cue
    scene->removeCue(cue);
  }
}

void createCameraOrbit(vtkSMSourceProxy* data, vtkSMRenderViewProxy* renderView)
{
  // Get camera position at start
  double* normal = renderView->GetActiveCamera()->GetViewUp();
  double* origin = renderView->GetActiveCamera()->GetPosition();

  // Get center of data
  double center[3];
  vtkTrivialProducer* t =
    vtkTrivialProducer::SafeDownCast(data->GetClientSideObject());
  if (!t) {
    return;
  }
  auto imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  double data_bounds[6];
  imageData->GetBounds(data_bounds);
  vtkBoundingBox box;
  box.SetBounds(data_bounds);
  box.GetCenter(center);
  QList<QVariant> centerList;
  centerList << center[0] << center[1] << center[2];

  // Generate camera orbit
  vtkSmartPointer<vtkPoints> pts;
  pts.TakeReference(vtkSMUtilities::CreateOrbit(center, normal, 7, origin));
  QList<QVariant> points;
  for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i) {
    double coords[3];
    pts->GetPoint(i, coords);
    points << coords[0] << coords[1] << coords[2];
  }

  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();

  pqAnimationCue* cue =
    scene->createCue(renderView, "Camera", 0, "CameraAnimationCue");
  pqSMAdaptor::setElementProperty(cue->getProxy()->GetProperty("Mode"), 1);
  cue->getProxy()->UpdateVTKObjects();
  vtkSMProxy* kf = cue->getKeyFrame(0);
  pqSMAdaptor::setMultipleElementProperty(kf->GetProperty("PositionPathPoints"),
                                          points);
  pqSMAdaptor::setMultipleElementProperty(kf->GetProperty("FocalPathPoints"),
                                          centerList);
  pqSMAdaptor::setElementProperty(kf->GetProperty("ClosedPositionPath"), 1);
  kf->UpdateVTKObjects();
}

void createCameraOrbit(vtkSMRenderViewProxy* renderView)
{
  // Get camera position at start
  double* normal = renderView->GetActiveCamera()->GetViewUp();
  double* origin = renderView->GetActiveCamera()->GetPosition();
  double* center = renderView->GetActiveCamera()->GetFocalPoint();

  QList<QVariant> centerList;
  centerList << center[0] << center[1] << center[2];

  // Generate camera orbit
  vtkSmartPointer<vtkPoints> pts;
  pts.TakeReference(vtkSMUtilities::CreateOrbit(center, normal, 7, origin));
  QList<QVariant> points;
  for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i) {
    auto* coords = pts->GetPoint(i);
    points << coords[0] << coords[1] << coords[2];
  }

  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();

  pqAnimationCue* cue =
    scene->createCue(renderView, "Camera", 0, "CameraAnimationCue");
  pqSMAdaptor::setElementProperty(cue->getProxy()->GetProperty("Mode"), 1);
  cue->getProxy()->UpdateVTKObjects();
  vtkSMProxy* kf = cue->getKeyFrame(0);
  pqSMAdaptor::setMultipleElementProperty(kf->GetProperty("PositionPathPoints"),
                                          points);
  pqSMAdaptor::setMultipleElementProperty(kf->GetProperty("FocalPathPoints"),
                                          centerList);
  pqSMAdaptor::setElementProperty(kf->GetProperty("ClosedPositionPath"), 1);
  kf->UpdateVTKObjects();
}

void setAnimationNumberOfFrames(int numFrames)
{
  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();
  pqSMAdaptor::setElementProperty(
    scene->getProxy()->GetProperty("NumberOfFrames"), numFrames);
}

void snapAnimationToTimeSteps(const std::vector<double>& timeSteps)
{
  std::vector<double> timeRange = { timeSteps.front(), timeSteps.back() };

  pqAnimationScene* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();
  pqSMAdaptor::setEnumerationProperty(
    scene->getProxy()->GetProperty("PlayMode"), "Snap To TimeSteps");

  auto* timeKeeper = ActiveObjects::instance().activeTimeKeeper();
  auto* proxy = timeKeeper->getProxy();
  vtkSMPropertyHelper(proxy, "TimestepValues")
    .Set(&timeSteps[0], timeSteps.size());
  vtkSMPropertyHelper(proxy, "TimeRange").Set(&timeRange[0], 2);
}

void setupRenderer(vtkRenderer* renderer, vtkImageSliceMapper* mapper,
                   vtkCubeAxesActor* axesActor)
{
  int axis = mapper->GetOrientation();
  int horizontal;
  int vertical;
  if (axis == 2) {
    horizontal = 0;
    vertical = 1;
  } else if (axis == 0) {
    horizontal = 1;
    vertical = 2;
  } else {
    horizontal = 0;
    vertical = 2;
  }
  renderer->SetBackground(1, 1, 1);
  vtkCamera* camera = renderer->GetActiveCamera();
  renderer->SetViewport(0.0, 0.0, 1.0, 1.0);

  double* bounds = mapper->GetBounds();
  double parallelScale;
  if (bounds[horizontal * 2 + 1] - bounds[horizontal * 2] <
      bounds[vertical * 2 + 1] - bounds[vertical * 2]) {
    parallelScale = 0.5 * (bounds[vertical * 2 + 1] - bounds[vertical * 2] + 1);
  } else {
    parallelScale =
      0.5 * (bounds[horizontal * 2 + 1] - bounds[horizontal * 2] + 1);
  }

  // If we have axes to plot, leave a little extra space for them
  double parallelScaleFactor = 1.0;

  if (axesActor) {
    axesActor->SetCamera(camera);
    double axisColor[3] = { 0.75, 0.75, 0.75 };
    double labelColor[3] = { 0.125, 0.125, 0.125 };
    double axesBounds[6] = { bounds[0], bounds[1], bounds[2],
                             bounds[3], bounds[4], bounds[5] };
    axesBounds[2 * axis] = bounds[2 * axis + 1];
    axesBounds[2 * axis + 1] = bounds[2 * axis + 1];
    axesActor->SetBounds(axesBounds);
    axesActor->SetScreenSize(20);
    axesActor->SetXTitle("");
    axesActor->SetYTitle("");
    axesActor->SetZTitle("");

    axesActor->GetXAxesLinesProperty()->SetColor(axisColor);
    axesActor->GetTitleTextProperty(0)->SetColor(axisColor);
    axesActor->GetLabelTextProperty(0)->SetColor(labelColor);

    axesActor->GetYAxesLinesProperty()->SetColor(axisColor);
    axesActor->GetTitleTextProperty(1)->SetColor(axisColor);
    axesActor->GetLabelTextProperty(1)->SetColor(labelColor);

    axesActor->GetZAxesLinesProperty()->SetColor(axisColor);
    axesActor->GetTitleTextProperty(2)->SetColor(axisColor);
    axesActor->GetLabelTextProperty(2)->SetColor(labelColor);

    renderer->AddActor(axesActor);
    parallelScaleFactor = 1.1;
  }

  vtkVector3d point;
  point[0] = 0.5 * (bounds[0] + bounds[1]);
  point[1] = 0.5 * (bounds[2] + bounds[3]);
  point[2] = 0.5 * (bounds[4] + bounds[5]);
  point[axis] = bounds[axis * 2 + 1];
  point[horizontal] -= (parallelScaleFactor - 1.0) * parallelScale / 2;
  point[vertical] -= (parallelScaleFactor - 1.0) * parallelScale / 2;
  camera->SetFocalPoint(point.GetData());
  point[axis] += parallelScale;
  camera->SetPosition(point.GetData());
  double viewUp[3] = { 0.0, 0.0, 0.0 };
  viewUp[vertical] = 1.0;
  camera->SetViewUp(viewUp);
  camera->ParallelProjectionOn();
  camera->SetParallelScale(parallelScale * parallelScaleFactor);
}

void deleteLayoutContents(QLayout* layout)
{
  while (layout && layout->count() > 0) {
    QLayoutItem* item = layout->itemAt(0);
    layout->removeItem(item);
    if (item) {
      if (item->widget()) {
        //-----------------------------------------------------------------------------
        delete item->widget();
        delete item;
      } else if (item->layout()) {
        deleteLayoutContents(item->layout());
        delete item->layout();
      }
    }
  }
}

Variant toVariant(const QVariant& value)
{
  switch (value.typeId()) {
    case QMetaType::Int:
      return Variant(value.toInt());
    case QMetaType::Double:
      return Variant(value.toDouble());
    case QMetaType::Bool:
      return Variant(value.toBool());
    case QMetaType::QString: {
      QString str = value.toString();
      return Variant(str.toStdString());
    }
    case QMetaType::QVariantList: {
      QVariantList list = value.toList();
      return toVariant(list);
    }
    case QMetaType::QVariantMap: {
      QVariantMap map = value.toMap();
      return toVariant(map);
    }
    default:
      qCritical() << "Unsupported type";
      return Variant();
  }
}

Variant toVariant(const QVariantList& list)
{
  std::vector<Variant> variantList;

  foreach (QVariant value, list) {
    variantList.push_back(toVariant(value));
  }

  return Variant(variantList);
}

Variant toVariant(const QVariantMap& map)
{
  std::map<std::string, Variant> variantMap;

  foreach (auto key, map.keys()) {
    variantMap[key.toStdString()] = toVariant(map[key]);
  }

  return Variant(variantMap);
}

QVariant toQVariant(const Variant& value)
{
  switch (value.type()) {
    case Variant::INTEGER:
      return QVariant(value.toInteger());
    case Variant::LONG:
      return QVariant((qlonglong)value.toLong());
    case Variant::DOUBLE:
      return QVariant(value.toDouble());
    case Variant::BOOL:
      return QVariant(value.toBool());
    case Variant::STRING: {
      return QVariant(QString::fromStdString(value.toString()));
    }
    case Variant::LIST: {
      std::vector<Variant> list = value.toList();
      QVariantList variantList;
      for (auto& v : list) {
        variantList.append(toQVariant(v));
      }

      return variantList;
    }
    case Variant::MAP: {
      std::map<std::string, Variant> map = value.toMap();
      QVariantMap variantMap;

      for (const auto& kv : map) {
        variantMap[QString::fromStdString(kv.first)] = toQVariant(kv.second);
      }

      return variantMap;
    }
    default: {
      qDebug() << value.type();
      qCritical() << "Unsupported type";
      return QVariant();
    }
  }
}

QString findPrefix(const QStringList& fileNames)
{

  QString prefix = fileNames[0];

  // Derived from ParaView pqObjectBuilder::createReader(...)
  // Find the largest prefix that matches all filenames.
  for (int i = 1; i < fileNames.size(); i++) {
    QString nextFile = fileNames[i];
    if (nextFile.startsWith(prefix))
      continue;
    QString commonPrefix = prefix;
    do {
      commonPrefix.chop(1);
    } while (!nextFile.startsWith(commonPrefix) && !commonPrefix.isEmpty());
    if (commonPrefix.isEmpty())
      break;
    prefix = commonPrefix;
  }

  return prefix;
}

QWidget* mainWidget()
{
  return pqCoreUtilities::mainWidget();
}

QJsonValue toJson(vtkVariant variant)
{
  auto type = variant.GetType();
  switch (type) {
    case VTK_STRING:
      return QJsonValue(variant.ToString().c_str());
    case VTK_CHAR:
      return QJsonValue(QString(QChar(variant.ToChar())));
    case VTK_SIGNED_CHAR:
    case VTK_UNSIGNED_CHAR:
    case VTK_SHORT:
    case VTK_UNSIGNED_SHORT:
    case VTK_INT:
    case VTK_UNSIGNED_INT:
      return QJsonValue(variant.ToInt());
    case VTK_LONG:
    case VTK_UNSIGNED_LONG:
    case VTK_LONG_LONG:
      return QJsonValue(variant.ToLongLong());
    case VTK_FLOAT:
    case VTK_DOUBLE:
      return QJsonValue(variant.ToDouble());
    default:
      qCritical() << QString("Unsupported vtkVariant type %1").arg(type);
      return QJsonValue();
  }
}

QJsonValue toJson(vtkSMProperty* property)
{
  vtkSMPropertyHelper helper(property);

  auto size = helper.GetNumberOfElements();

  if (size == 1) {
    return toJson(helper.GetAsVariant(0));
  } else {
    QJsonArray values;
    for (unsigned int i = 0; i < size; i++) {
      auto value = toJson(helper.GetAsVariant(i));
      values.append(value);
    }

    return values;
  }
}

bool setProperty(const QJsonArray& array, vtkSMProperty* prop)
{
  for (int i = 0; i < array.size(); i++) {
    if (!setProperty(array[i], prop, i)) {
      return false;
    }
  }

  return true;
}

bool setProperty(const QJsonValue& value, vtkSMProperty* prop, int index)
{
  vtkSMPropertyHelper helper(prop);

  if (value.isArray()) {
    return setProperty(value.toArray(), prop);
  } else if (value.isDouble()) {
    if (prop->IsA("vtkSMIntVectorProperty")) {
      helper.Set(index, value.toInt());
    } else if (prop->IsA("vtkSMDoubleVectorProperty")) {
      helper.Set(index, value.toDouble());
    } else {
      qCritical() << QString("Unexpected property type.");
      return false;
    }
  } else if (value.isString()) {
    helper.Set(index, value.toString().toLatin1().data());
  } else {
    qCritical() << QString("Unexpected JSON type.");
    return false;
  }

  return true;
}

bool setProperties(const QJsonObject& props, vtkSMProxy* proxy)
{
  if (proxy == nullptr) {
    return false;
  }

  foreach (const QString& name, props.keys()) {
    QJsonValue value = props.value(name);

    auto prop = proxy->GetProperty(name.toLatin1().data());
    if (prop != nullptr) {
      if (!setProperty(value, prop)) {
        return false;
      }
    }
  }

  return true;
}

QString dialogToFileName(QFileDialog* dialog)
{
  auto res = dialog->exec();
  if (res != QDialog::Accepted) {
    return QString();
  }
  QStringList fileNames = dialog->selectedFiles();
  if (fileNames.size() < 1) {
    return QString();
  }
  QString fileName = fileNames[0];
  return fileName;
}

QJsonDocument tableToJson(vtkTable* table)
{
  QJsonArray rows;
  for (vtkIdType i = 0; i < table->GetNumberOfRows(); ++i) {
    auto row = table->GetRow(i);
    QJsonArray item;
    for (vtkIdType j = 0; j < row->GetSize(); ++j) {
      auto value = row->GetValue(j);
      if (value.IsNumeric()) {
        if (value.IsFloat()) {
          item << value.ToFloat();
        } else if (value.IsDouble()) {
          item << value.ToDouble();
        } else if (value.IsInt()) {
          item << value.ToInt();
        } else {
          // Fall back to double, of include all the other types if needed
          item << value.ToDouble();
        }
      }
    }
    rows << item;
  }
  return QJsonDocument(rows);
}

QJsonDocument vectorToJson(const QVector<vtkVector2i> vector)
{
  QJsonArray rows;
  foreach (auto row, vector) {
    QJsonArray item;
    item << row[0] << row[1];
    rows << item;
  }
  return QJsonDocument(rows);
}

bool jsonToFile(const QJsonDocument& document)
{
  QStringList filters;
  filters << "JSON Files (*.json)";
  QFileDialog dialog;
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setNameFilters(filters);
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  QString fileName = dialogToFileName(&dialog);
  if (fileName.isEmpty()) {
    return false;
  }
  if (!fileName.endsWith(".json")) {
    fileName = QString("%1.json").arg(fileName);
  }

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    qCritical() << QString("Error opening file for writing: %1").arg(fileName);
    return false;
  }
  file.write(document.toJson());
  file.close();
  return true;
}

bool moleculeToFile(vtkMolecule* molecule)
{
  if (molecule == nullptr) {
    return false;
  }

  QStringList filters;
  filters << "XYZ Files (*.xyz)";
  QFileDialog dialog;
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setNameFilters(filters);
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  QString fileName = dialogToFileName(&dialog);
  if (fileName.isEmpty()) {
    return false;
  }
  if (!fileName.endsWith(".xyz")) {
    fileName = QString("%1.xyz").arg(fileName);
  }

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    qCritical() << QString("Error opening file for writing: %1").arg(fileName);
    return false;
  }
  QTextStream out(&file);
  out << molecule->GetNumberOfAtoms() << "\n";
  out << QString("Generated with TomViz ( http://tomviz.org )\n");
  vtkNew<vtkPeriodicTable> periodicTable;
  for (int i = 0; i < molecule->GetNumberOfAtoms(); ++i) {
    vtkAtom atom = molecule->GetAtom(i);
    auto symbol = QString(periodicTable->GetSymbol(atom.GetAtomicNumber()));
    auto position = atom.GetPosition();
    out << symbol << "   " << QString::number(position[0]) << "   "
        << QString::number(position[1]) << "   " << QString::number(position[2])
        << "\n";
  }
  file.close();
  return true;
}

void openUrl(const QString& link)
{
  openUrl(QUrl(link));
}

void openUrl(const QUrl& url)
{
  QDesktopServices::openUrl(url);
}

void openHelpUrl(const QString& path)
{
  QString basePath = "https://tomviz.readthedocs.io/en/latest/";

  QString fullUrl;
  if (!path.startsWith("http")) {
    // Assume this is a relative path that needs toe base added.
    fullUrl = basePath;
  }
  fullUrl += path;

  // For now, no local paths have been added. Just use the web path.
  openUrl(fullUrl);
}

double offWhite[3] = { 204.0 / 255, 204.0 / 255, 204.0 / 255 };

class StrictWeakOrdering
{
  // This was obtained from StrictWeakOrdering in ParaView's
  // vtkSMTransferFunctionProxy.cxx

public:
  bool operator()(const vtkTuple<double, 4>& x,
                  const vtkTuple<double, 4>& y) const
  {
    return (x.GetData()[0] < y.GetData()[0]);
  }
};

bool vtkNormalize(std::vector<vtkTuple<double, 4>>& cntrlPoints)
{
  // This was adapted from vtkNormalize() in
  // ParaView's vtkSMTransferFunctionProxy.cxx
  // The only difference is that we have removed the "log_space" option
  // and the "originalRange" parameter

  if (cntrlPoints.size() == 0) {
    // nothing to do, but not an error, so return true.
    return true;
  }
  if (cntrlPoints.size() == 1) {
    // Only 1 control point in the property. We'll add 2 points, however.
    cntrlPoints.resize(2);
    cntrlPoints[1] = cntrlPoints[0];
    cntrlPoints[0][0] = 0.0;
    cntrlPoints[1][0] = 1.0;
    return true;
  }

  // sort the points by x, just in case user didn't add them correctly.
  std::sort(cntrlPoints.begin(), cntrlPoints.end(), StrictWeakOrdering());

  // now simply normalize the cntrlPoints.
  const double range[2] = { cntrlPoints.front()[0], cntrlPoints.back()[0] };
  if (range[0] == 0.0 && range[1] == 1.0) {
    // nothing to do.
    return true;
  }
  const double denominator = (range[1] - range[0]);
  assert(denominator > 0);
  for (size_t cc = 0; cc < cntrlPoints.size(); ++cc) {
    cntrlPoints[cc][0] = (cntrlPoints[cc][0] - range[0]) / denominator;
  }
  return true;
}

bool vtkRescaleControlPoints(std::vector<vtkTuple<double, 4>>& cntrlPoints,
                             double rangeMin, double rangeMax)
{
  // This was adapted from vtkRescaleNormalizedControlPoints() in
  // ParaView's vtkSMTransferFunctionProxy.cxx
  // The only difference is that we have removed the log scale option
  // Also, we call vtkNormalize ourselves

  assert(cntrlPoints.size() >= 2);
  assert(rangeMin < rangeMax);

  vtkNormalize(cntrlPoints);

  double scale = (rangeMax - rangeMin);
  assert(scale > 0);
  for (size_t cc = 0; cc < cntrlPoints.size(); ++cc) {
    double& x = cntrlPoints[cc][0];
    x = x * scale + rangeMin;
  }
  return true;
}

void rescaleLut(vtkColorTransferFunction* lut, double rangeMin, double rangeMax)
{
  // Create the input for vtkRescaleControlPoints
  // Points are XRGB
  std::vector<vtkTuple<double, 4>> points;
  for (int i = 0; i < lut->GetSize(); ++i) {
    points.push_back(vtkTuple<double, 4>());

    // Values are XRGB, followed by sharpness and mid point
    double values[6];
    lut->GetNodeValue(i, values);
    // Copy over the first four values (XRGB) into the point data
    std::copy(values, values + 4, points.back().GetData());
  }

  // Rescale the points
  vtkRescaleControlPoints(points, rangeMin, rangeMax);

  // Now set the results back on the LUT
  for (int i = 0; i < lut->GetSize(); ++i) {
    // Values are XRGB, followed by sharpness and mid point
    double values[6];
    lut->GetNodeValue(i, values);

    // Copy over the result XRGB
    for (int j = 0; j < 4; ++j) {
      values[j] = points[i][j];
    }

    // Set it on the LUT
    lut->SetNodeValue(i, values);
  }
}

double getVoxelValue(vtkImageData* data, const vtkVector3d& point,
                     vtkVector3i& indices, bool& ok)
{
  vtkVector3d p(point.GetData());
  auto pointId = data->FindPoint(p.GetData());
  if (pointId < 0) {
    ok = false;
    return 0;
  }

  vtkVector3d origin;
  data->GetOrigin(origin.GetData());

  vtkVector3d spacing;
  data->GetSpacing(spacing.GetData());

  for (int i = 0; i < 3; ++i) {
    indices[i] = round((point[i] - origin[i]) / spacing[i]);
  }

  double scalar = data->GetScalarComponentAsDouble(
    indices[0], indices[1], indices[2], 0);
  ok = true;
  return scalar;
}

QString userDataPath() {
  // Ensure the tomviz directory exists
  QStringList locations =
      QStandardPaths::standardLocations(QStandardPaths::HomeLocation);
  QString home = locations[0];
  QString path = QString("%1%2tomviz").arg(home).arg(QDir::separator());
  QDir dir(path);
  // dir.mkpath() returns true if the path already exists or if it was
  // successfully created.
  if (!dir.mkpath(path)) {
    QMessageBox::warning(
      tomviz::mainWidget(), "Could not create tomviz directory",
      QString("Could not create tomviz directory '%1'.").arg(path));
    return QString();
  }
  return path;
}

} // namespace tomviz

namespace tomviz {

namespace {

static bool areClose(double a, double b, double tol = 1.e-8)
{
  return std::abs(a - b) < tol;
}

static bool nodeColorsMatch(double* a, double* b)
{
  for (int i = 1; i < 4; ++i) {
    if (!areClose(a[i], b[i])) {
      return false;
    }
  }

  return true;
}

static bool nodeYsMatch(double* a, double* b)
{
  return areClose(a[1], b[1]);
}

} // namespace

void addPlaceholderNodes(vtkColorTransferFunction* lut, DataSource* ds)
{
  // Add nodes on the data ends as placeholders
  auto numNodes = lut->GetSize();
  if (numNodes == 0) {
    return;
  }

  double range[2];
  ds->getRange(range);

  auto* dataArray = lut->GetDataPointer();
  int nodeStride = 4;
  double* firstNode = dataArray;
  double* backNode = firstNode + (numNodes - 1) * nodeStride;

  std::vector<std::array<double, 4>> addPoints;
  if (!areClose(firstNode[0], range[0])) {
    addPoints.push_back({ range[0], firstNode[1], firstNode[2], firstNode[3] });
  }

  if (!areClose(backNode[0], range[1])) {
    addPoints.push_back({ range[1], backNode[1], backNode[2], backNode[3] });
  }

  for (const auto& point : addPoints) {
    lut->AddRGBPoint(point[0], point[1], point[2], point[3]);
  }
}

void removePlaceholderNodes(vtkColorTransferFunction* lut)
{
  // Remove all nodes on the ends that match their neighboring nodes
  auto numNodes = lut->GetSize();
  if (numNodes == 0) {
    return;
  }

  auto* dataArray = lut->GetDataPointer();
  int nodeStride = 4;
  double* firstNode = dataArray;
  double* backNode = firstNode + (numNodes - 1) * nodeStride;

  std::vector<double> removePoints;
  double* currentNode = firstNode;
  double* nextNode = currentNode + nodeStride;
  while (nodeColorsMatch(currentNode, nextNode) && nextNode != backNode) {
    removePoints.push_back(currentNode[0]);
    currentNode += nodeStride;
    nextNode += nodeStride;
  }

  currentNode = backNode;
  nextNode = currentNode - nodeStride;

  while (nodeColorsMatch(currentNode, nextNode) && nextNode != firstNode) {
    removePoints.push_back(currentNode[0]);
    currentNode -= nodeStride;
    nextNode -= nodeStride;
  }

  for (auto point : removePoints) {
    lut->RemovePoint(point);
  }
}

void addPlaceholderNodes(vtkPiecewiseFunction* opacity, DataSource* ds)
{
  // Add nodes on the data ends as placeholders
  auto numNodes = opacity->GetSize();
  if (numNodes == 0) {
    return;
  }

  double range[2];
  ds->getRange(range);

  auto* dataArray = opacity->GetDataPointer();
  int nodeStride = 2;
  double* firstNode = dataArray;
  double* backNode = firstNode + (numNodes - 1) * nodeStride;

  std::vector<std::array<double, 2>> addPoints;
  if (!areClose(firstNode[0], range[0])) {
    addPoints.push_back({ range[0], firstNode[1] });
  }

  if (!areClose(backNode[0], range[1])) {
    addPoints.push_back({ range[1], backNode[1] });
  }

  for (const auto& point : addPoints) {
    opacity->AddPoint(point[0], point[1]);
  }
}

void removePlaceholderNodes(vtkPiecewiseFunction* opacity)
{
  // Remove all nodes on the ends that match their neighboring nodes
  auto numNodes = opacity->GetSize();
  if (numNodes == 0) {
    return;
  }

  auto* dataArray = opacity->GetDataPointer();
  int nodeStride = 2;
  double* firstNode = dataArray;
  double* backNode = firstNode + (numNodes - 1) * nodeStride;

  std::vector<double> removePoints;
  double* currentNode = firstNode;
  double* nextNode = currentNode + nodeStride;
  while (nodeYsMatch(currentNode, nextNode) && nextNode != backNode) {
    removePoints.push_back(currentNode[0]);
    currentNode += nodeStride;
    nextNode += nodeStride;
  }

  currentNode = backNode;
  nextNode = currentNode - nodeStride;

  while (nodeYsMatch(currentNode, nextNode) && nextNode != firstNode) {
    removePoints.push_back(currentNode[0]);
    currentNode -= nodeStride;
    nextNode -= nodeStride;
  }

  for (auto point : removePoints) {
    opacity->RemovePoint(point);
  }
}

double rescale(double val, double oldMin, double oldMax, double newMin,
               double newMax)
{
  return (val - oldMin) * (newMax - newMin) / (oldMax - oldMin) + newMin;
}

void rescaleNodes(vtkColorTransferFunction* lut, double newMin, double newMax)
{
  // Rescale all nodes to a new range
  auto numNodes = lut->GetSize();
  if (numNodes == 0) {
    return;
  }

  auto* dataArray = lut->GetDataPointer();
  int nodeStride = 4;
  double* firstNode = dataArray;
  double* backNode = firstNode + (numNodes - 1) * nodeStride;

  double oldMin = firstNode[0];
  double oldMax = backNode[0];

  std::vector<std::array<double, 4>> points;
  for (int i = 0; i < numNodes; ++i) {
    double* currentNode = firstNode + i * nodeStride;
    double newX = rescale(currentNode[0], oldMin, oldMax, newMin, newMax);
    points.push_back({ newX, currentNode[1], currentNode[2], currentNode[3] });
  }

  lut->RemoveAllPoints();

  for (const auto& point : points) {
    lut->AddRGBPoint(point[0], point[1], point[2], point[3]);
  }
}

void rescaleNodes(vtkPiecewiseFunction* opacity, double newMin, double newMax)
{
  // Rescale all nodes to a new range
  auto numNodes = opacity->GetSize();
  if (numNodes == 0) {
    return;
  }

  auto* dataArray = opacity->GetDataPointer();
  int nodeStride = 2;
  double* firstNode = dataArray;
  double* backNode = firstNode + (numNodes - 1) * nodeStride;

  double oldMin = firstNode[0];
  double oldMax = backNode[0];

  std::vector<std::array<double, 2>> points;
  for (int i = 0; i < numNodes; ++i) {
    double* currentNode = firstNode + i * nodeStride;
    double newX = rescale(currentNode[0], oldMin, oldMax, newMin, newMax);
    points.push_back({ newX, currentNode[1] });
  }

  opacity->RemoveAllPoints();

  for (const auto& point : points) {
    opacity->AddPoint(point[0], point[1]);
  }
}

void removePointsOutOfRange(vtkColorTransferFunction* lut, DataSource* ds)
{
  // Remove points outside the range of the data source
  // This will also add points on the ends of the range
  double range[2];
  ds->getRange(range);

  // Make sure there are points on the ends of the data
  double startColor[3], endColor[3];
  lut->GetColor(range[0], startColor);
  lut->GetColor(range[1], endColor);

  // Remove all points out of range
  std::vector<double> removePoints;
  for (int i = 0; i < lut->GetSize(); ++i) {
    double node[6];
    lut->GetNodeValue(i, node);
    double x = node[0];
    if (x < range[0] || x > range[1]) {
      removePoints.push_back(x);
    }
  }

  for (auto point : removePoints) {
    lut->RemovePoint(point);
  }

  lut->AddRGBPoint(range[0], startColor[0], startColor[1], startColor[2]);
  lut->AddRGBPoint(range[1], endColor[0], endColor[1], endColor[2]);
}

void removePointsOutOfRange(vtkPiecewiseFunction* opacity, DataSource* ds)
{
  // Remove points outside the range of the data source
  // This will also add points on the ends of the range

  double range[2];
  ds->getRange(range);

  // Make sure there are points on the ends of the data
  double startY = opacity->GetValue(range[0]);
  double endY = opacity->GetValue(range[1]);

  std::vector<double> removePoints;
  for (int i = 0; i < opacity->GetSize(); ++i) {
    double node[4];
    opacity->GetNodeValue(i, node);
    double x = node[0];
    if (x < range[0] || x > range[1]) {
      removePoints.push_back(x);
    }
  }

  for (auto point : removePoints) {
    opacity->RemovePoint(point);
  }

  opacity->AddPoint(range[0], startY);
  opacity->AddPoint(range[1], endY);
}

bool loadPlugin(QString path)
{
  if (!path.startsWith("/")) {
    // A relative path. Prepend it with the application directory path.
    path = QApplication::applicationDirPath() + "/" + path;
  }

  auto pluginManager = vtkSMProxyManager::GetProxyManager()->GetPluginManager();
  bool success = pluginManager->LoadLocalPlugin(path.toLatin1().data());
  if (!success) {
    qCritical() << "Failed to load plugin:" << path;
  }
  return success;
}

bool loadLookingGlassPlugin()
{
  return loadPluginsWithSubstring("LookingGlass");
}

bool hasLookingGlassPlugin()
{
  return pluginsWithSubstring("LookingGlass").size() != 0;
}

QStringList pluginsWithSubstring(const QString& substring)
{
  QStringList ret;

  auto pluginPaths =
    QString(TOMVIZ_PLUGIN_PATHS).split(';', Qt::SkipEmptyParts);
  for (auto path : pluginPaths) {
    QFileInfo info(path);
    if (info.fileName().contains(substring)) {
      ret.append(path);
    }
  }

  return ret;
}

bool loadPluginsWithSubstring(const QString& substring)
{
  auto plugins = pluginsWithSubstring(substring);
  if (plugins.size() == 0) {
    return false;
  }

  bool success = true;
  for (auto& plugin : plugins) {
    if (!loadPlugin(plugin)) {
      success = false;
    }
  }

  return success;
}

bool loadPlugins()
{
  bool success = true;

  // TOMVIZ_PLUGIN_PATHS is a semicolon delimited list of plugins to load
  auto pluginPaths =
    QString(TOMVIZ_PLUGIN_PATHS).split(';', Qt::SkipEmptyParts);
  for (auto path : pluginPaths) {
    if (!loadPlugin(path)) {
      success = false;
    }
  }

  return success;
}

void relabelXAndZAxes(vtkImageData* image)
{
  if (!image)
    return;

  // We want the preserve the field data
  vtkSmartPointer<vtkFieldData> fd = image->GetFieldData();
  image->SetFieldData(nullptr);

  int dim[3];
  double spacing[3], origin[3];
  image->GetDimensions(dim);
  image->GetSpacing(spacing);
  image->GetOrigin(origin);

  std::swap(dim[0], dim[2]);
  std::swap(spacing[0], spacing[2]);
  std::swap(origin[0], origin[2]);

  image->SetDimensions(dim);
  image->SetSpacing(spacing);
  image->SetOrigin(origin);

  // Reinstate the field data
  image->SetFieldData(fd);
}

} // namespace tomviz
