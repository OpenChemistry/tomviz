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
#ifndef tomvizMergeImagesDialog_h
#define tomvizMergeImagesDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class MergeImagesDialog;
}

namespace tomviz {

class MergeImagesDialog : public QDialog
{
Q_OBJECT

public:
  MergeImagesDialog(QWidget* parent = nullptr);
  ~MergeImagesDialog() override;

  enum MergeMode : int { Arrays, Components };

  MergeMode getMode();

private:
  Q_DISABLE_COPY(MergeImagesDialog)
  QScopedPointer<Ui::MergeImagesDialog> m_ui;
};
}

#endif