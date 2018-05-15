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

#include "DataSource.h"
#include "ImageStackModel.h"

#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScopedPointer>

namespace Ui {

class ImageStackDialog;
}

namespace tomviz {
class ImageInfo;
class ImageStackDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ImageStackDialog(QWidget* parent = nullptr);
  ~ImageStackDialog() override;

  void setStackSummary(const QList<ImageInfo>& summary);
  void setStackType(const DataSource::DataSourceType& stackType);
  QList<ImageInfo> stackSummary() const;

public slots:
  void onOpenFileClick();
  void onOpenFolderClick();
  void onImageToggled(int row, bool value);
  void onStackTypeChanged(int stackType);

signals:
  void summaryChanged(const QList<ImageInfo>&);
  void stackTypeChanged(const DataSource::DataSourceType&);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent* event) override;

private:
  QScopedPointer<Ui::ImageStackDialog> m_ui;

  QList<ImageInfo> m_summary;
  DataSource::DataSourceType m_stackType;
  ImageStackModel m_tableModel;
  void openFileDialog(int mode);
  void processDirectory(QString path);
  void processFiles(QStringList fileNames);
  bool detectVolume(QStringList fileNames, QList<ImageInfo>& summary, bool matchPrefix = true);
  bool detectTilt(QStringList fileNames, QList<ImageInfo>& summary, bool matchPrefix = true);
  void defaultOrder(QStringList fileNames, QList<ImageInfo>& summary);
};
} // namespace tomviz

#endif
