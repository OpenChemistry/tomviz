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
#ifndef __ModuleThreshold_h
#define __ModuleThreshold_h

#include "Module.h"
#include "vtkWeakPointer.h"

class vtkSMProxy;
class vtkSMSourceProxy;

namespace TEM
{

class ModuleThreshold : public Module
{
  Q_OBJECT;
  typedef Module Superclass;
public:
  ModuleThreshold(QObject* parent=NULL);
  virtual ~ModuleThreshold();

  virtual QString label() const { return  "Threshold"; }
  virtual QIcon icon() const;
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);
  virtual bool finalize();
  virtual bool setVisibility(bool val);
  virtual bool visibility() const;
  virtual void addToPanel(pqProxiesWidget*);
private:
  Q_DISABLE_COPY(ModuleThreshold);
  vtkWeakPointer<vtkSMSourceProxy> ThresholdFilter;
  vtkWeakPointer<vtkSMProxy> ThresholdRepresentation;
};

}

#endif
