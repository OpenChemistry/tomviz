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
#ifndef tomvizReconstructionWidget_h
#define tomvizReconstructionWidget_h

#include <QWidget>

namespace tomviz
{
class DataSource;

class ReconstructionWidget : public QWidget
{
  Q_OBJECT

public:
  ReconstructionWidget(DataSource *source, QWidget *parent = nullptr);
  virtual ~ReconstructionWidget();

public slots:

  void startReconstruction();
  void cancelReconstruction();

signals:
  void reconstructionFinished();
  void reconstructionCancelled();

private:
  Q_DISABLE_COPY(ReconstructionWidget)

  class RWInternal;
  RWInternal *Internals;
};

}

#endif
