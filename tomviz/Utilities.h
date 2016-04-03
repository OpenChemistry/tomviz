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

#include "pqApplicationCore.h"
#include "pqProxy.h"
#include "pqServerManagerModel.h"
#include "vtkSMSourceProxy.h"

#include <vtk_pugixml.h>
#include <QFileInfo>
#include <QStringList>

class pqAnimationScene;

class vtkSMProxyLocator;
class vtkSMRenderViewProxy;
class vtkPVArrayInformation;

class QDir;

namespace tomviz
{

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

/// convert a pqProxy to vtkSMProxy.
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
  if (proxy)
  {
    proxy->SetAnnotation("tomviz.Type", "DataSource");
    QFileInfo fileInfo(filename);
    proxy->SetAnnotation("tomviz.DataSource.FileName", filename);
    proxy->SetAnnotation("tomviz.Label", fileInfo.fileName().toLatin1().data());
    return true;
  }
  return false;
}

inline bool annotateDataProducer(pqProxy* pqproxy, const char* filename)
{
  return annotateDataProducer(convert(pqproxy), filename);
}

//// Check if the proxy is a data producer.
//inline bool isDataProducer(vtkSMProxy* proxy)
//{
//  return proxy &&
//      proxy->HasAnnotation("tomviz.Type") &&
//      (QString("DataSource") == proxy->GetAnnotation("tomviz.Type"));
//}
//
//inline bool isDataProducer(pqProxy* pqproxy)
//{
//  return isDataProducer(convert(pqproxy));
//}

// Returns the tomviz label for a proxy, if any, otherwise simply returns the
// XML label for it.
inline QString label(vtkSMProxy* proxy)
{
  if (proxy && proxy->HasAnnotation("tomviz.Label"))
  {
    return proxy->GetAnnotation("tomviz.Label");
  }
  return proxy? proxy->GetXMLLabel() : nullptr;
}

inline QString label(pqProxy* proxy)
{
  return label(convert(proxy));
}

/// Serialize a proxy to a pugi::xml node.
bool serialize(vtkSMProxy* proxy, pugi::xml_node& out, const QStringList& properties=QStringList(), const QDir* relDir = nullptr);
bool deserialize(vtkSMProxy* proxy, const pugi::xml_node& in, const QDir* relDir = nullptr, vtkSMProxyLocator* locator=nullptr);

/// Returns the vtkPVArrayInformation for scalars array produced by the given
/// source proxy.
vtkPVArrayInformation* scalarArrayInformation(vtkSMSourceProxy* proxy);

/// Rescales the colorMap (and associated opacityMap) using the transformed data
/// range from the data source. This will respect the "LockScalarRange" property
/// on the colorMap i.e. if user locked the scalar range, it won't be rescaled.
bool rescaleColorMap(vtkSMProxy* colorMap, DataSource* dataSource);

// Given the name of a python script, find the script file and return the contents
// This assumes that the given script is one of the built-in tomviz python operator
// scripts.
QString readInPythonScript(const QString &scriptName);

// Create a camera orbit animation for the given renderview around the given object
void createCameraOrbit(vtkSMSourceProxy *data, vtkSMRenderViewProxy *renderView);

}

#endif
