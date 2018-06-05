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
#ifndef tomvizPipelineSettingsDialog_h
#define tomvizPipelineSettingsDialog_h

#include <QDialog>
#include <QMetaEnum>
#include <QScopedPointer>

#include "Pipeline.h"

namespace Ui {
class PipelineSettingsDialog;
}

namespace tomviz {

class PipelineSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  PipelineSettingsDialog(QWidget* parent = nullptr);
  ~PipelineSettingsDialog() override;
  void readSettings();

private slots:
  void writeSettings();

private:
  Q_DISABLE_COPY(PipelineSettingsDialog)
  QScopedPointer<Ui::PipelineSettingsDialog> m_ui;
  QMetaEnum m_executorTypeMetaEnum;
  void checkEnableOk();
};
} // namespace tomviz

#endif
