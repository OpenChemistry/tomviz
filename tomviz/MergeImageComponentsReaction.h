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
#ifndef tomvizMergeImageComponentsReaction_h
#define tomvizMergeImageComponentsReaction_h

#include <pqReaction.h>

#include <QSet>

namespace tomviz {

class DataSource;

class MergeImageComponentsReaction : pqReaction
{
  Q_OBJECT

public:
  MergeImageComponentsReaction(QAction* action);

  DataSource* mergeComponents();

public slots:
  void updateDataSources(QSet<DataSource*>);

protected:
  void onTriggered() override;
  void updateEnableState() override;

private:
  Q_DISABLE_COPY(MergeImageComponentsReaction)

  QSet<DataSource*> m_dataSources;
};
}

#endif
