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
#ifndef tomvizPipelineManager_h
#define tomvizPipelineManager_h

#include <QObject>
#include <QScopedPointer>
#include <vtk_pugixml.h>

class pqView;
class vtkSMSourceProxy;
class vtkSMViewProxy;
class vtkPVXMLElement;
class vtkSMProxyLocator;
class vtkSMRepresentationProxy;
class QDir;

namespace tomviz {
class DataSource;
class Module;
class Pipeline;


class PipelineManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  static PipelineManager& instance();

  /// save the application state as xml.
  /// Parameter stateDir: the location to use as the base of all relative file
  /// paths
  bool serialize(pugi::xml_node& ns, const QDir& stateDir,
                 bool interactive = true) const;
  bool deserialize(const pugi::xml_node& ns, const QDir& stateDir);


public slots:
  void addPipeline(Pipeline*);
  void removePipeline(Pipeline*);
  void removeAllPipelines();

private:
  Q_DISABLE_COPY(PipelineManager)
  PipelineManager(QObject* parent = nullptr);
  ~PipelineManager();

  class PMInternals;
  QScopedPointer<PMInternals> Internals;
};
}

#endif
