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
#ifndef tomvizFixCenterOfRotationBehavior_h
#define tomvizFixCenterOfRotationBehavior_h

#include <QObject>

class pqView;
class vtkObject;

namespace tomviz
{

// This is a custom behavior for tomviz that prevents automatic center of
// rotation changes when the camera is moved and fixes the initial center of
// rotation to be at (0,0,0)
class FixCenterOfRotationBehavior : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;
public:
  FixCenterOfRotationBehavior(QObject* parent=0);
  virtual ~FixCenterOfRotationBehavior();

protected slots:
  void onViewAdded(pqView*);

private:
  Q_DISABLE_COPY(FixCenterOfRotationBehavior)
};

}

#endif
