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

#ifndef tomvizImageStackDialog_h
#define tomvizImageStackDialog_h

#include <QDialog>

#include <QScopedPointer>

namespace Ui {

class ImageStackDialog;
}

namespace tomviz {

class ImageStackModel;
class ImageInfo;

class ImageStackDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ImageStackDialog(QWidget* parent = nullptr);
  ~ImageStackDialog() override;

  void setStackSummary(const QList<ImageInfo>& summary);
  QList<ImageInfo> stackSummary() const;

public slots:
  void onOpenFileClick();
  void onOpenFolderClick();
  void onImageToggled(int row, bool value);

signals:
  void summaryChanged(const QList<ImageInfo>&);

private:
  QScopedPointer<Ui::ImageStackDialog> m_ui;
  QList<ImageInfo> m_summary;
  void openFileDialog(QString mode);
};
} // namespace tomviz

#endif
