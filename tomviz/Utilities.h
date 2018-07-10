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
#ifndef tomvizUtilties_h
#define tomvizUtilties_h

// Collection of miscellaneous utility functions.

#include <pqApplicationCore.h>
#include <pqProxy.h>
#include <pqServerManagerModel.h>
#include <vtkSMProperty.h>
#include <vtkSMSourceProxy.h>
#include <vtkVariant.h>

#include <Variant.h>
#include <vtkVector.h>
#include <vtk_pugixml.h>

#include <QFileInfo>
#include <QJsonDocument>
#include <QStringList>
#include <QVariant>

class pqAnimationScene;

class vtkDiscretizableColorTransferFunction;
class vtkImageSliceMapper;
class vtkMolecule;
class vtkRenderer;
class vtkSMProxyLocator;
class vtkSMRenderViewProxy;
class vtkPVArrayInformation;
class vtkPiecewiseFunction;
class vtkTable;

class QDir;
class QLayout;

namespace tomviz {

struct Attributes
{
  static const char* TYPE;
  static const char* DATASOURCE_FILENAME;
  static const char* LABEL;
  static const char* FILENAME;
};

class DataSource;

//===========================================================================
// Functions for converting from pqProxy to vtkSMProxy and vice-versa.
//===========================================================================

/// Converts a vtkSMProxy to a pqProxy subclass by forwarding the call to
/// pqServerManagerModel instance.
template <class T>
T convert(vtkSMProxy* proxy)
{
  pqApplicationCore* appcore = pqApplicationCore::instance();
  Q_ASSERT(appcore);
  pqServerManagerModel* smmodel = appcore->getServerManagerModel();
  Q_ASSERT(smmodel);
  return smmodel->findItem<T>(proxy);
}

/// Convert a pqProxy to vtkSMProxy.
inline vtkSMProxy* convert(pqProxy* pqproxy)
{
  return pqproxy ? pqproxy->getProxy() : nullptr;
}

//===========================================================================
// Functions for annotation proxies to help identification in tomviz.
//===========================================================================

// Annotate a proxy to be recognized as the data producer in the application.
inline bool annotateDataProducer(vtkSMProxy* proxy, const char* filename)
{
  if (proxy) {
    proxy->SetAnnotation(Attributes::TYPE, "DataSource");
    QFileInfo fileInfo(filename);
    proxy->SetAnnotation(Attributes::DATASOURCE_FILENAME, filename);
    proxy->SetAnnotation(Attributes::LABEL,
                         fileInfo.fileName().toLatin1().data());
    return true;
  }
  return false;
}

inline bool annotateDataProducer(pqProxy* pqproxy, const char* filename)
{
  return annotateDataProducer(convert(pqproxy), filename);
}

//// Check if the proxy is a data producer.
// inline bool isDataProducer(vtkSMProxy* proxy)
//{
//  return proxy &&
//      proxy->HasAnnotation(Attributes::TYPE) &&
//      (QString("DataSource") == proxy->GetAnnotation(Attributes::TYPE));
//}
//
// inline bool isDataProducer(pqProxy* pqproxy)
//{
//  return isDataProducer(convert(pqproxy));
//}

// Returns the tomviz label for a proxy, if any, otherwise simply returns the
// XML label for it.
inline QString label(vtkSMProxy* proxy)
{
  if (proxy && proxy->HasAnnotation(Attributes::LABEL)) {
    return proxy->GetAnnotation(Attributes::LABEL);
  }
  return proxy ? proxy->GetXMLLabel() : nullptr;
}

inline QString label(pqProxy* proxy)
{
  return label(convert(proxy));
}

//// Making a function to serialize some ParaView proxies we are interested in.
QJsonObject serialize(vtkSMProxy* proxy);
bool deserialize(vtkSMProxy* proxy, const QJsonObject& json);

/// Serialize a proxy to a pugi::xml node.
bool serialize(vtkSMProxy* proxy, pugi::xml_node& out,
               const QStringList& properties = QStringList(),
               const QDir* relDir = nullptr);
bool deserialize(vtkSMProxy* proxy, const pugi::xml_node& in,
                 const QDir* relDir = nullptr,
                 vtkSMProxyLocator* locator = nullptr);

/// Serialize/deserialize a QVariant to a pugi::xml node
bool serialize(const QVariant& variant, pugi::xml_node& out);
bool deserialize(QVariant& variant, const pugi::xml_node& in);
/// Serialize/deserialize a QVariantMap to a pugi::xml node
bool serialize(const QVariantMap& map, pugi::xml_node& out);
bool deserialize(QVariantMap& map, const pugi::xml_node& in);
/// Serialize/deserialize a vtkPiecewiseFunction
bool serialize(vtkPiecewiseFunction* func, pugi::xml_node& out);
bool deserialize(vtkPiecewiseFunction* func, const pugi::xml_node& in);

QJsonObject serialize(vtkDiscretizableColorTransferFunction* func);
bool deserialize(vtkDiscretizableColorTransferFunction* func,
                 const QJsonObject& json);
QJsonObject serialize(vtkPiecewiseFunction* func);
bool deserialize(vtkPiecewiseFunction* func, const QJsonObject& json);

/// Returns the vtkPVArrayInformation for scalars array produced by the given
/// source proxy.
vtkPVArrayInformation* scalarArrayInformation(vtkSMSourceProxy* proxy);

/// Rescales the colorMap (and associated opacityMap) using the transformed data
/// range from the data source. This will respect the "LockScalarRange" property
/// on the colorMap i.e. if user locked the scalar range, it won't be rescaled.
bool rescaleColorMap(vtkSMProxy* colorMap, DataSource* dataSource);

// Given the root of a file and an extension, reades the file fileName +
// extension and returns the content in a QString.
QString readInTextFile(const QString& fileName, const QString& extension);

// Given the name of a python script, find the script file and return the
// contents. This assumes that the given script is one of the built-in tomviz
// Python operator scripts.
QString readInPythonScript(const QString& scriptName);

// Given the name of an operator python script, find the JSON description
// file and return the contents. This assumes that the given script is one
// of the built-in tomviz python operator scripts.
QString readInJSONDescription(const QString& scriptName);

// Create a camera orbit animation for the given renderview around the given
// object
void createCameraOrbit(vtkSMSourceProxy* data,
                       vtkSMRenderViewProxy* renderView);

// Set up the renderer to show the given slice in parallel projection
// This function attempts to zoom the renderer so that the enitire slice is
// visible while minimizing the empty regions of the view (zoom so the slice's
// target dimension barely fits in the view).
void setupRenderer(vtkRenderer* renderer, vtkImageSliceMapper* mapper);

// Delete all widgets within a layout
void deleteLayoutContents(QLayout* layout);

Variant toVariant(const QVariant& value);
Variant toVariant(const QVariantList& value);

/// Find common prefix for collection of file names
QString findPrefix(const QStringList& fileNames);

/// Convenience function to get the main widget (useful for dialog parenting).
QWidget* mainWidget();

QJsonValue toJson(vtkVariant variant);
QJsonValue toJson(vtkSMProperty* prop);
bool setProperties(const QJsonObject& props, vtkSMProxy* proxy);
bool setProperty(const QJsonValue& value, vtkSMProperty* prop, int index = 0);
bool setProperty(const QJsonArray& array, vtkSMProperty* prop);

/// Write a vtkTable to json file
bool jsonToFile(const QJsonDocument& json);
QJsonDocument tableToJson(vtkTable* table);
QJsonDocument vectorToJson(const QVector<vtkVector2i> vector);

/// Write a vtkMolecule to json file
bool moleculeToFile(vtkMolecule* molecule);
extern double offWhite[3];
} // namespace tomviz

#endif
