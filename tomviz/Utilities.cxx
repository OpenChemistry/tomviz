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
#include "Utilities.h"

#include "DataSource.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqCoreUtilities.h>
#include <pqPVApplicationCore.h>
#include <pqSMAdaptor.h>
#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkPVXMLElement.h>
#include <vtkPVXMLParser.h>
#include <vtkSMNamedPropertyIterator.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMUtilities.h>

#include <vtkBoundingBox.h>
#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkImageSliceMapper.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkStringList.h>
#include <vtkTrivialProducer.h>

#include <sstream>
#include <vector>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QLayout>
#include <QMessageBox>
#include <QString>

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
  if (!document.load(stream.str().c_str())) {
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
  switch (value.type()) {
    case QVariant::Int:
      out.append_attribute("type").set_value("int");
      out.append_attribute("value").set_value(value.toInt());
      return true;
    case QVariant::Double:
      out.append_attribute("type").set_value("double");
      out.append_attribute("value").set_value(value.toDouble());
      return true;
    case QVariant::Bool:
      out.append_attribute("type").set_value("bool");
      out.append_attribute("value").set_value(value.toBool());
      return true;
    case QVariant::String: {
      out.append_attribute("type").set_value("string");
      out.append_attribute("value").set_value(
        value.toString().toLatin1().data());
      return true;
    }
    case QVariant::List: {
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
  pqSMAdaptor::setElementProperty(
    scene->getProxy()->GetProperty("NumberOfFrames"), 200);

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

void setupRenderer(vtkRenderer* renderer, vtkImageSliceMapper* mapper)
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
  renderer->SetBackground(1.0, 1.0, 1.0);
  vtkCamera* camera = renderer->GetActiveCamera();
  renderer->SetViewport(0.0, 0.0, 1.0, 1.0);

  double* bounds = mapper->GetBounds();
  vtkVector3d point;
  point[0] = 0.5 * (bounds[0] + bounds[1]);
  point[1] = 0.5 * (bounds[2] + bounds[3]);
  point[2] = 0.5 * (bounds[4] + bounds[5]);
  camera->SetFocalPoint(point.GetData());
  point[axis] += 50 + 0.5 * (bounds[axis * 2 + 1] - bounds[axis * 2]);
  camera->SetPosition(point.GetData());
  double viewUp[3] = { 0.0, 0.0, 0.0 };
  viewUp[vertical] = 1.0;
  camera->SetViewUp(viewUp);
  camera->ParallelProjectionOn();
  double parallelScale;
  if (bounds[horizontal * 2 + 1] - bounds[horizontal * 2] <
      bounds[vertical * 2 + 1] - bounds[vertical * 2]) {
    parallelScale = 0.5 * (bounds[vertical * 2 + 1] - bounds[vertical * 2] + 1);
  } else {
    parallelScale =
      0.5 * (bounds[horizontal * 2 + 1] - bounds[horizontal * 2] + 1);
  }
  camera->SetParallelScale(parallelScale);
  double clippingRange[2];
  camera->GetClippingRange(clippingRange);
  clippingRange[1] =
    clippingRange[0] + (bounds[axis * 2 + 1] - bounds[axis * 2] + 50);
  camera->SetClippingRange(clippingRange);
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
  switch (value.type()) {
    case QVariant::Int:
      return Variant(value.toInt());
    case QVariant::Double:
      return Variant(value.toDouble());
    case QVariant::Bool:
      return Variant(value.toBool());
    case QVariant::String: {
      QString str = value.toString();
      return Variant(str.toStdString());
    }
    case QVariant::List: {
      QVariantList list = value.toList();
      return toVariant(list);
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

void rasterTransferFunction2DBox(vtkImageData* histogram2D, const vtkRectd& box,
                                 vtkImageData* transferFunction,
                                 vtkColorTransferFunction* colorFunc,
                                 vtkPiecewiseFunction* opacFunc)
{
  if (!histogram2D) {
    std::cerr << "Invalid histogram" << std::endl;
    return;
  }
  if (!transferFunction) {
    std::cerr << "Invalid output image" << std::endl;
    return;
  }
  if (!opacFunc || !colorFunc) {
    std::cerr << "Invalid transfer functions!" << std::endl;
    return;
  }

  int bins[3];
  transferFunction->GetDimensions(bins);

  // If the transfer function image is uninitialized, initialize it
  if (bins[0] == 0 && bins[1] == 0) {
    histogram2D->GetDimensions(bins);
    transferFunction->SetDimensions(bins[0], bins[1], 1);
    transferFunction->AllocateScalars(VTK_FLOAT, 4);
  }

  double spacing[3];
  histogram2D->GetSpacing(spacing);
  const vtkIdType width = static_cast<vtkIdType>(box.GetWidth() / spacing[0]);
  const vtkIdType height = static_cast<vtkIdType>(box.GetHeight() / spacing[1]);

  if (width <= 0 || height <= 0) {
    return;
  }

  // Assume color and opacity share the same data range
  double range[2];
  colorFunc->GetRange(range);

  double* dataRGB = new double[width * 3];
  colorFunc->GetTable(range[0], range[1], width, dataRGB);

  double* dataAlpha = new double[width];
  opacFunc->GetTable(range[0], range[1], width, dataAlpha);

  // Copy the values into Transfer2D
  vtkFloatArray* transfer =
    vtkFloatArray::SafeDownCast(transferFunction->GetPointData()->GetScalars());

  const vtkIdType x0 = static_cast<vtkIdType>(box.GetX() / spacing[0]);
  const vtkIdType y0 = static_cast<vtkIdType>(box.GetY() / spacing[1]);

  for (vtkIdType j = 0; j < height; j++)
    for (vtkIdType i = 0; i < width; i++) {
      double color[4];

      color[0] = dataRGB[i * 3];
      color[1] = dataRGB[i * 3 + 1];
      color[2] = dataRGB[i * 3 + 2];
      color[3] = dataAlpha[i];

      const vtkIdType index = (y0 + j) * bins[1] + (x0 + i);
      transfer->SetTuple(index, color);
    }

  delete[] dataRGB;
  delete[] dataAlpha;
}

QJsonValue toJson(vtkVariant variant)
{
  auto type = variant.GetType();
  switch (type) {
    case VTK_STRING:
      return QJsonValue(variant.ToString());
    case VTK_UNICODE_STRING:
      return QJsonValue(
        QString::fromUtf8(variant.ToUnicodeString().utf8_str()));
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

double offWhite[3] = { 204.0 / 255, 204.0 / 255, 204.0 / 255 };
} // namespace tomviz
