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
#ifndef __LoadDataReaction_h
#define __LoadDataReaction_h

#include "pqReaction.h"

class vtkSMProxy;

namespace TEM
{
class DataSource;

/// LoadDataReaction handles the "Load Data" action in TomViz. On trigger,
/// this will open the data file and necessary subsequent actions, including:
/// \li make the data source "active".
///
class LoadDataReaction : public pqReaction
{
  Q_OBJECT

  typedef pqReaction Superclass;

public:
  LoadDataReaction(QAction* parentAction);
  virtual ~LoadDataReaction();

  /// Create a raw data source from the reader.
  static DataSource* createDataSource(vtkSMProxy* reader);

  /// Load a data file from the specified location.
  static DataSource* loadData(const QString &fileName);

  /// Handle creation of a new data source.
  static void dataSourceAdded(DataSource*);

protected:
  /// Called when the action is triggered.
  virtual void onTriggered();

private:
  Q_DISABLE_COPY(LoadDataReaction)
};

}
#endif
