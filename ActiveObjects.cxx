/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "ActiveObjects.h"

#include "pqPipelineSource.h"
#include "pqServer.h"

namespace TEM
{

//-----------------------------------------------------------------------------
ActiveObjects::ActiveObjects()
  : Superclass(),
  ActiveDataSource(NULL),
  VoidActiveDataSource(NULL)
{
  this->connect(&pqActiveObjects::instance(), SIGNAL(viewChanged(pqView*)),
    SIGNAL(viewChanged(pqView*)));
}

//-----------------------------------------------------------------------------
ActiveObjects::~ActiveObjects()
{
}

//-----------------------------------------------------------------------------
ActiveObjects& ActiveObjects::instance()
{
  static ActiveObjects theInstance;
  return theInstance;
}

//-----------------------------------------------------------------------------
void ActiveObjects::setActiveView(pqView* view)
{
  pqActiveObjects::instance().setActiveView(view);
}

//-----------------------------------------------------------------------------
void ActiveObjects::setActiveDataSource(pqPipelineSource* source)
{
  if (this->VoidActiveDataSource != source)
    {
    this->ActiveDataSource = source;
    this->VoidActiveDataSource = source;
    emit this->dataSourceChanged(this->ActiveDataSource);
    }
}

//-----------------------------------------------------------------------------
vtkSMSessionProxyManager* ActiveObjects::proxyManager() const
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  return server? server->proxyManager(): NULL;
}

//-----------------------------------------------------------------------------

} // end of namespace TEM
