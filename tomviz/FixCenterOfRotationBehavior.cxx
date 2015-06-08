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

#include "FixCenterOfRotationBehavior.h"

#include "pqApplicationCore.h"
#include "pqRenderView.h"
#include "pqServerManagerModel.h"

namespace tomviz
{

FixCenterOfRotationBehavior::FixCenterOfRotationBehavior(QObject* parent)
  : QObject(parent)
{
  pqServerManagerModel* smmodel = 
    pqApplicationCore::instance()->getServerManagerModel();
  QObject::connect(smmodel, SIGNAL(viewAdded(pqView*)),
      this, SLOT(onViewAdded(pqView*)));
}

FixCenterOfRotationBehavior::~FixCenterOfRotationBehavior() {}

void FixCenterOfRotationBehavior::onViewAdded(pqView* view)
{
  pqRenderView* renderView = dynamic_cast<pqRenderView*>(view);
  if (renderView)
    {
    std::cout << "Setting center of rotation." << std::endl;
    renderView->setResetCenterWithCamera(false);
    renderView->setCenterOfRotation(0,0,0);
    }
}

}
