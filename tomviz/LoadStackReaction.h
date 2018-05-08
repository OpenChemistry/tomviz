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
#ifndef tomvizLoadStackReaction_h
#define tomvizLoadStackReaction_h

#include <pqReaction.h>

#include <QJsonObject>

#include "ImageStackModel.h"

class vtkImageData;
class vtkSMProxy;

namespace tomviz {
class DataSource;

/// LoadStackReaction handles the "Load Stack" action in tomviz. On trigger,
/// this will open a dialog where the user can drag-n-drop or open multiple files
/// or a folder. After selecting the files in the stack, options will be presented
/// to include or exclude each file, and to label which type of stack it is
///
class LoadStackReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadStackReaction(QAction* parentAction);
  ~LoadStackReaction() override;

  static DataSource* loadData(QStringList fileNames);

  // static void loadData();

  

  /// Load data files from the specified locations, options can be used to pass
  /// additional parameters to the method, such as defaultModules, addToRecent,
  /// and child, or pvXML to pass to the ParaView reader.
  // static DataSource* loadData(const QStringList& fileNames,
  //                             const QJsonObject& options = QJsonObject());

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(LoadStackReaction)

  // static void addDefaultModules(DataSource* dataSource);
  // static QJsonObject readerProperties(vtkSMProxy* reader);
  // static void setFileNameProperties(const QJsonObject& props,
  //                                   vtkSMProxy* reader);
  // static bool loadTiffStack(const QStringList& fileNames,
  //                           QList<ImageInfo>& summary);
  static void stackDialog();
  static void stackDialog(QList<ImageInfo>& summary);
  static QStringList summaryToFileNames(const QList<ImageInfo>& summary);
  static QList<ImageInfo> loadTiffStack(const QStringList& fileNames);
};
} // namespace tomviz

#endif
