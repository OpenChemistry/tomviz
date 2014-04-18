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
#include "Module.h"

#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

namespace TEM
{
//-----------------------------------------------------------------------------
Module::Module(QObject* parentObject) : Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
Module::~Module()
{
}

//-----------------------------------------------------------------------------
bool Module::initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view)
{
  this->View = view;
  this->DataSource = dataSource;
  return (this->View && this->DataSource);
}

//-----------------------------------------------------------------------------
vtkSMViewProxy* Module::view() const
{
  return this->View;
}

//-----------------------------------------------------------------------------
vtkSMSourceProxy* Module::dataSource() const
{
  return this->DataSource;
}

} // end of namespace TEM
