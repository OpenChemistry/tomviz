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

#include "DataSource.h"
#include "pqProxiesWidget.h"
#include "pqView.h"
#include "Utilities.h"
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
bool Module::initialize(DataSource* dataSource, vtkSMViewProxy* view)
{
  this->View = view;
  this->ADataSource = dataSource;
  if (this->View && this->ADataSource)
    {
    // FIXME: we're connecting this too many times. Fix it.
    TEM::convert<pqView*>(view)->connect(
      this->ADataSource, SIGNAL(dataChanged()), SLOT(render()));
    }
  return (this->View && this->ADataSource);
}

//-----------------------------------------------------------------------------
vtkSMViewProxy* Module::view() const
{
  return this->View;
}

//-----------------------------------------------------------------------------
DataSource* Module::dataSource() const
{
  return this->ADataSource;
}

//-----------------------------------------------------------------------------
void Module::addToPanel(pqProxiesWidget* panel)
{
  (void)panel;
}

//-----------------------------------------------------------------------------
bool Module::serialize(pugi::xml_node& ns) const
{
  Q_UNUSED(ns);
  return false;
}

} // end of namespace TEM
