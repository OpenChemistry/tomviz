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
#ifndef tomvizModulePropertiesPanel_h
#define tomvizModulePropertiesPanel_h

#include <QScopedPointer>
#include <QWidget>

class vtkSMViewProxy;

namespace tomviz {
class Module;

class ModulePropertiesPanel : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  ModulePropertiesPanel(QWidget* parent = nullptr);
  virtual ~ModulePropertiesPanel();

private slots:
  void setModule(Module*);
  void setView(vtkSMViewProxy*);
  void updatePanel();
  void detachColorMap(bool);
  void arrayNameChanged(const QString&);

private:
  Q_DISABLE_COPY(ModulePropertiesPanel)

  class MPPInternals;
  const QScopedPointer<MPPInternals> Internals;
};
}

#endif
