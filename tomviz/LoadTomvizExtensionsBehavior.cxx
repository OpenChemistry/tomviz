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

#include "LoadTomvizExtensionsBehavior.h"

#include "pqApplicationCore.h"
#include "pqObjectBuilder.h"
#include "pqServer.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkSMSessionProxyManager.h"

#include <QDebug>

namespace tomviz
{

LoadTomvizExtensionsBehavior::LoadTomvizExtensionsBehavior(QObject* p)
  : QObject(p)
{
  pqApplicationCore* core = pqApplicationCore::instance();
  this->connect(core->getObjectBuilder(), SIGNAL(finishedAddingServer(pqServer*)),
                SLOT(onServerLoaded(pqServer*)));
}

LoadTomvizExtensionsBehavior::~LoadTomvizExtensionsBehavior()
{
}

void LoadTomvizExtensionsBehavior::onServerLoaded(pqServer *server)
{
  const char* tomvizXML =
    "<ServerManagerConfiguration>"
    "<ProxyGroup name=\"tomviz_proxies\">"
    "<Proxy name=\"NonOrthogonalSlice\">"
    "<IntVectorProperty default_values=\"1\" number_of_elements=\"1\" name=\"ShowArrow\">"
    "<BooleanDomain name=\"bool\"/>"
    "</IntVectorProperty>"
    "</Proxy>"
    "</ProxyGroup>"
    "</ServerManagerConfiguration>";

  vtkSMProxyDefinitionManager* defnMgr =
    server->proxyManager()->GetProxyDefinitionManager();
  defnMgr->LoadConfigurationXMLFromString(tomvizXML);
}

}
