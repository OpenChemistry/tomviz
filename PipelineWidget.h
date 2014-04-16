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
#ifndef __PipelineWidget_h
#define __PipelineWidget_h

#include <QTreeWidget>
#include <QScopedPointer>

class pqPipelineSource;

namespace TEM
{
  /// PipelineWidget is a QTreeWidget to show the visualization "Pipeline" in MatViz.
  /// This is not same as the underlying ParaView visualization pipeline. We
  /// show higher level abstractions for MatViz users, than the raw VTK
  /// pipelines.
  class PipelineWidget : public QTreeWidget
  {
  Q_OBJECT;
  typedef QTreeWidget Superclass;
public:
  PipelineWidget(QWidget* parent=0);
  virtual ~PipelineWidget();

private slots:
  /// Slots connect to pqServerManagerModel to monitor pipeline proxies
  /// being registered/unregistered.
  void sourceAdded(pqPipelineSource*);
  void sourceRemoved(pqPipelineSource*);

  /// Called when current item selected in the widget changes.
  void currentItemChanged(QTreeWidgetItem*);

  /// Called when the ActiveObjects' active data source changes.
  void setCurrent(pqPipelineSource*);

private:
  /// Called by sourceAdded/sourceRemoved when a data producer is detected.
  void addDataProducer(pqPipelineSource* producer);
  void removeDataProducer(pqPipelineSource* producer);

private:
  Q_DISABLE_COPY(PipelineWidget);
  class PWInternals;
  QScopedPointer<PWInternals> Internals;
  };
}
#endif
