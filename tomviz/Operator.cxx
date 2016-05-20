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
#include "Operator.h"

namespace tomviz
{

Operator::Operator(QObject* parentObject)
  : Superclass(parentObject), supportsCancel(false)
{
}

Operator::~Operator()
{
}

bool Operator::transform(vtkDataObject *data)
{
  emit this->transformingStarted();
  emit this->updateProgress(0);
  bool result = this->applyTransform(data);
  emit this->transformingDone(result);
  return result;
}
}
