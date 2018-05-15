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

class vtkImageData;
class vtkSMProxy;

namespace tomviz {
class DataSource;
class ImageInfo;

/// LoadStackReaction handles the "Load Stack" action in tomviz. On trigger,
/// this will open a dialog where the user can drag-n-drop or open multiple
/// files
/// or a folder. After selecting the files in the stack, options will be
/// presented
/// to include or exclude each file, and to label which type of stack it is
///
class LoadStackReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadStackReaction(QAction* parentAction);
  ~LoadStackReaction() override;

  static DataSource* loadData();
  static DataSource* loadData(QStringList fileNames);
  static QList<ImageInfo> loadTiffStack(const QStringList& fileNames);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(LoadStackReaction)

  static QStringList summaryToFileNames(const QList<ImageInfo>& summary);
};
} // namespace tomviz

#endif
