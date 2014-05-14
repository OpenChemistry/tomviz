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
#include "RecentFilesMenu.h"

#include "LoadDataReaction.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMSourceProxy.h"
#include "pqPipelineSource.h"

namespace TEM
{
//-------------------------------------------------------------------------
RecentFilesMenu::RecentFilesMenu(QMenu& menu, QObject* parentObject)
  : Superclass(menu, parentObject)
{
}

//-------------------------------------------------------------------------
RecentFilesMenu::~RecentFilesMenu()
{
}

//-------------------------------------------------------------------------
pqPipelineSource* RecentFilesMenu::createReader(
    const QString& readerGroup,
    const QString& readerName,
    const QStringList& files,
    pqServer* server) const
{
  vtkNew<vtkSMParaViewPipelineController> controller;
  pqPipelineSource* reader = this->Superclass::createReader(
    readerGroup, readerName, files, server);
  if (reader)
    {
    vtkSMSourceProxy* dataSource = LoadDataReaction::createDataSource(reader);
    controller->UnRegisterProxy(reader->getProxy());
    reader = NULL;
    return TEM::convert<pqPipelineSource*>(dataSource);
    }
  return NULL;
}

}
