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
#ifndef tomvizBehaviors_h
#define tomvizBehaviors_h

#include <QObject>

class QMainWindow;
class vtkSMTransferFunctionPresets;

namespace tomviz {

class MoveActiveObject;

/// Behaviors instantiates tomviz relevant ParaView behaviors (and any new
/// ones) as needed.

class Behaviors : public QObject
{
  Q_OBJECT

public:
  Behaviors(QMainWindow* mainWindow);
  virtual ~Behaviors();

  MoveActiveObject* moveActiveBehavior() { return m_moveActiveBehavior; }
private:
  Q_DISABLE_COPY(Behaviors)

  MoveActiveObject* m_moveActiveBehavior;

  QString getMatplotlibColorMapFile();

  // Caller is responsible for calling Delete() on the returned object.
  vtkSMTransferFunctionPresets* getPresets();

  /// Use the named color map preset as default.
  void setDefaultColorMapFromPreset(const char* name);
};
}
#endif
