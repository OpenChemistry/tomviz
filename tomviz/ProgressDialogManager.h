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
#ifndef tomvizProgressDialogManager_h
#define tomvizProgressDialogManager_h

#include <QObject>

class QMainWindow;

namespace tomviz {
class Operator;
class DataSource;

class ProgressDialogManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  ProgressDialogManager(QMainWindow* mw);
  virtual ~ProgressDialogManager();

private slots:
  void operationStarted();
  void operationProgress(int progress);
  void operatorAdded(Operator* op);

private:
  QMainWindow* mainWindow;
  Q_DISABLE_COPY(ProgressDialogManager)
};
}
#endif
