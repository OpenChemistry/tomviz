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
#ifndef __Module_h
#define __Module_h

#include <QObject>
#include <QIcon>
#include "vtkWeakPointer.h"

class vtkSMSourceProxy;
class vtkSMViewProxy;


namespace TEM
{
  /// Abstract parent class for all Modules in MatViz.
  class Module : public QObject
  {
  Q_OBJECT;
  typedef QObject Superclass;
public:
  Module(QObject* parent=NULL);
  virtual ~Module();

  /// Returns a  label for this module.
  virtual QString label() const = 0;

  /// Returns an icon to use for this module.
  virtual QIcon icon() const=0;

  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  virtual bool initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view);

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  virtual bool finalize()=0;

  vtkSMSourceProxy* dataSource() const;
  vtkSMViewProxy* view() const;

public slots:
  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  virtual bool setVisibility(bool val)=0;

  bool show() { this->setVisibility(true); }
  bool hide() { this->setVisibility(false); }

private:
  Q_DISABLE_COPY(Module);
  vtkWeakPointer<vtkSMSourceProxy> DataSource;
  vtkWeakPointer<vtkSMViewProxy> View;
  };
}
#endif
