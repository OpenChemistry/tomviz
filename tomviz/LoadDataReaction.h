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
#ifndef tomvizLoadDataReaction_h
#define tomvizLoadDataReaction_h

#include <pqReaction.h>

class vtkImageData;
class vtkSMProxy;

namespace tomviz {
class DataSource;

/// LoadDataReaction handles the "Load Data" action in tomviz. On trigger,
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
  static DataSource* createDataSource(vtkSMProxy* reader,
                                      bool defaultModules = true,
                                      bool child = false);

  /// Create a data source that can be populated with data.
  static DataSource* createDataSource(vtkImageData* imageData);

  /// Create a data source using Tomviz readers (no proxy).
  static DataSource* createDataSourceLocal(const QString& fileName,
                                           bool defaultModules = true,
                                           bool child = false);

  static QList<DataSource*> loadData();

  /// Load a data file from the specified location.
  static DataSource* loadData(const QString& fileName,
                              bool defaultModules = true,
                              bool addToRecent = true,
                              bool child = false);

  /// Load a data files from the specified location.
  static DataSource* loadData(const QStringList& fileNames,
                              bool defaultModules = true,
                              bool addToRecent = true,
                              bool child = false);

  /// Handle creation of a new data source.
  static void dataSourceAdded(DataSource* dataSource,
                              bool defaultModules = true,
                              bool child = false);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(LoadDataReaction)

  static void addDefaultModules(DataSource* dataSource);
};
}
#endif
