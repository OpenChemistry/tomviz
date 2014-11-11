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
#include "LoadDataReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "pqLoadDataReaction.h"
#include "pqPipelineSource.h"
#include "pqProxyWidgetDialog.h"
#include "RecentFilesMenu.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

namespace TEM
{
//-----------------------------------------------------------------------------
LoadDataReaction::LoadDataReaction(QAction* parentObject)
  : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
LoadDataReaction::~LoadDataReaction()
{
}

//-----------------------------------------------------------------------------
void LoadDataReaction::onTriggered()
{
  vtkNew<vtkSMParaViewPipelineController> controller;

  // Let ParaView deal with reading the data. If we need more customization, we
  // can show our own "File/Open" dialog and then create the appropriate reader
  // proxies.
  QList<pqPipelineSource*> readers = pqLoadDataReaction::loadData();

  // Since we want to ditch the reader pipeline and just keep the raw data
  // around. We do this magic.
  foreach (pqPipelineSource* reader, readers)
    {
    DataSource* dataSource = this->createDataSource(reader->getProxy());
    // dataSource may be NULL if user cancelled the action.
    if (dataSource)
      {
      // add the file to recent files menu.
      RecentFilesMenu::pushDataReader(reader->getProxy());
      }
    controller->UnRegisterProxy(reader->getProxy());
    }
  readers.clear();
}

DataSource* LoadDataReaction::loadData(const QString &fileName)
{
  vtkNew<vtkSMParaViewPipelineController> controller;
  QStringList files;
  files << fileName;
  pqPipelineSource* reader = pqLoadDataReaction::loadData(files);

  DataSource* dataSource = createDataSource(reader->getProxy());
  // dataSource may be NULL if user cancelled the action.
  if (dataSource)
    {
    // add the file to recent files menu.
    RecentFilesMenu::pushDataReader(reader->getProxy());
    }
  controller->UnRegisterProxy(reader->getProxy());
}

//-----------------------------------------------------------------------------
DataSource* LoadDataReaction::createDataSource(vtkSMProxy* reader)
{
  // Prompt user for reader configuration.
  pqProxyWidgetDialog dialog(reader);
  dialog.setObjectName("ConfigureReaderDialog");
  dialog.setWindowTitle("Configure Reader Parameters");
  if (dialog.hasVisibleWidgets() == false || dialog.exec() == QDialog::Accepted)
    {
    DataSource* dataSource = new DataSource(vtkSMSourceProxy::SafeDownCast(reader));
    // do whatever we need to do with a new data source.
    LoadDataReaction::dataSourceAdded(dataSource);
    return dataSource;
    }
  return NULL;
}

//-----------------------------------------------------------------------------
void LoadDataReaction::dataSourceAdded(DataSource* dataSource)
{
  ModuleManager::instance().addDataSource(dataSource);

  vtkSMViewProxy* view = ActiveObjects::instance().activeView();
  // Create an outline module for the source in the active view.
  if (Module* module = ModuleManager::instance().createAndAddModule(
      "Outline", dataSource, view))
    {
    ActiveObjects::instance().setActiveModule(module);
    }
  if (Module* module = ModuleManager::instance().createAndAddModule(
        "Orthogonal Slice", dataSource, view))
    {
    ActiveObjects::instance().setActiveModule(module);
    }
}

} // end of namespace TEM
