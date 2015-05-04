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
#ifndef __TEM_ScaleActorBehavior_h
#define __TEM_ScaleActorBehavior_h

#include <QObject>

class pqView;

namespace TEM
{
class ScaleActorBehavior : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;
public:
  ScaleActorBehavior(QObject* parent=NULL);
  ~ScaleActorBehavior();
private slots:
  void viewAdded(pqView*);

private:
  Q_DISABLE_COPY(ScaleActorBehavior)
};
}

#endif
