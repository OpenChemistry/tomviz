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
#ifndef __TEM_ModuleSlice_h
#define __TEM_ModuleSlice_h

#include "Module.h"
#include "vtkWeakPointer.h"
#include "vtkSmartPointer.h"

class vtkSMProxy;
class vtkSMSourceProxy;
class vtkColorImagePlaneWidget;

namespace TEM
{
class ModuleSlice : public Module
{
  Q_OBJECT;
  typedef Module Superclass;
public:
  ModuleSlice(QObject* parent=NULL);
  virtual ~ModuleSlice();

  virtual QString label() const { return  "Slice"; }
  virtual QIcon icon() const;
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);
  virtual bool finalize();
  virtual bool setVisibility(bool val);
  virtual bool visibility() const;
  virtual void addToPanel(pqProxiesWidget*);
  virtual bool serialize(pugi::xml_node& ns) const;
  virtual bool deserialize(const pugi::xml_node& ns);

private:
  Q_DISABLE_COPY(ModuleSlice);
  vtkWeakPointer<vtkSMSourceProxy> PassThrough;
  vtkSmartPointer<vtkColorImagePlaneWidget> Widget;
  vtkWeakPointer<vtkSMProxy> TransferFunction;
};

}
#endif
