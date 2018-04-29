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

class QDir;

namespace tomviz {

class Pipeline;

class PipelineManager : public QObject
{
  Q_OBJECT

public:
  static PipelineManager& instance();

public slots:
  void addPipeline(Pipeline*);
  void removePipeline(Pipeline*);
  void removeAllPipelines();

private:
  Q_DISABLE_COPY(PipelineManager)
  PipelineManager(QObject* parent = nullptr);
  ~PipelineManager() override;

  QList<QPointer<Pipeline>> m_pipelines;
};
}

#endif
