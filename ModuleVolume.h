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
#ifndef __ModuleVolume_h
#define __ModuleVolume_h

#include "Module.h"
#include "vtkWeakPointer.h"

class vtkSMProxy;

namespace TEM
{

class ModuleVolume : public Module
{
  Q_OBJECT

  typedef Module Superclass;

public:
  ModuleVolume(QObject* parent=NULL);
  virtual ~ModuleVolume();

  virtual QString label() const { return  "Volume"; }
  virtual QIcon icon() const;
  virtual bool initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view);
  virtual bool finalize();
  virtual bool setVisibility(bool val);

private:
  Q_DISABLE_COPY(ModuleVolume)
  vtkWeakPointer<vtkSMProxy> Representation;
};

}

#endif
