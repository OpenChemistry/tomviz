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
#ifndef __Utilties_h
#define __Utilties_h

// Collection of miscellaneous utility functions.

#include "pqApplicationCore.h"
#include "pqServerManagerModel.h"
#include "pqProxy.h"

namespace TEM
{
  /// Converts a vtkSMProxy to a pqProxy subclass by forwarding the call to
  /// pqServerManagerModel instance.
  template <class T>
  T convert(vtkSMProxy* proxy)
    {
    pqApplicationCore* appcore = pqApplicationCore::instance();
    Q_ASSERT(appcore);
    pqServerManagerModel* smmodel = appcore->getServerManagerModel();
    Q_ASSERT(smmodel);
    return smmodel->findItem<T>(proxy);
    }

  /// convert a pqProxy to vtkSMProxy.
  vtkSMProxy* convert(pqProxy* pqproxy)
    {
    return pqproxy? pqproxy->getProxy() : NULL;
    }
}
#endif
