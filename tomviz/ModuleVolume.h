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
#ifndef tomvizModuleVolume_h
#define tomvizModuleVolume_h

#include "Module.h"
#include "vtkWeakPointer.h"

class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz
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
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);
  virtual bool finalize();
  virtual bool setVisibility(bool val);
  virtual bool visibility() const;
  virtual bool serialize(pugi::xml_node& ns) const;
  virtual bool deserialize(const pugi::xml_node& ns);
  virtual bool isColorMapNeeded() const { return true; }

protected:
  virtual void updateColorMap();

private:
  Q_DISABLE_COPY(ModuleVolume)
  vtkWeakPointer<vtkSMSourceProxy> PassThrough;
  vtkWeakPointer<vtkSMProxy> Representation;
};

}

#endif
