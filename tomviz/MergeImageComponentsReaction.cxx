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
#include "MergeImageComponentsReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "LoadDataReaction.h"

#include <QFileInfo>
#include <QSet>

#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>

namespace tomviz {

MergeImageComponentsReaction::MergeImageComponentsReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  updateEnableState();
}

DataSource* MergeImageComponentsReaction::mergeComponents()
{
  if (m_dataSources.size() < 1) {
    return nullptr;
  }

  QList<DataSource*> sourceList = m_dataSources.toList();

  // Eventually, we'll offer the option to merge compontents in a single array.
  // For now, we will simply append the point data arrays.
  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  vtkSMSourceProxy* filter =
    vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("filters", "AppendAttributes"));
  Q_ASSERT(filter);

  for (int i = 0; i < sourceList.size(); ++i) {
    vtkSMPropertyHelper(filter, "Input").Add(sourceList[i]->producer(), 0);  
  }

  filter->UpdateVTKObjects();
  filter->UpdatePipeline();

  DataSource* newDataset = new DataSource(filter);
  QString mergedFilename(QFileInfo(sourceList[0]->filename()).baseName());
  for (int i = 1; i < sourceList.size(); ++i) {
    mergedFilename.append(" + ");
    mergedFilename.append(QFileInfo(sourceList[i]->filename()).baseName());
  }
  newDataset->setFilename(mergedFilename);
  filter->Delete();

  return newDataset;
}

void MergeImageComponentsReaction::onTriggered()
{
  DataSource* source = mergeComponents();
  if (source) {
    LoadDataReaction::dataSourceAdded(source);
  }
}

void MergeImageComponentsReaction::updateDataSources(QSet<DataSource*> sources)
{
  m_dataSources = sources;
  updateEnableState();
}

void MergeImageComponentsReaction::updateEnableState()
{
  parentAction()->setEnabled(m_dataSources.size() > 1);
}

}
