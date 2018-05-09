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

#ifndef tomvizImageStackModel_h
#define tomvizImageStackModel_h

#include <QAbstractTableModel>

#include <QFileInfo>
#include <QModelIndex>
#include <QString>

namespace tomviz {

struct ImageInfo;

/// Adapter to visualize the ImageInfo of a stack of images in a QTableView
class ImageStackModel : public QAbstractTableModel
{
  Q_OBJECT
public:
  ImageStackModel(QObject* parent);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

  Qt::ItemFlags flags(const QModelIndex &index) const;
  bool setData(const QModelIndex &index, const QVariant &value,
                int role = Qt::EditRole);

  QList<ImageInfo> getFileInfo() const;

public slots:
  // void on_stackType_changed(QString stackType);
  void onFilesInfoChanged(QList<ImageInfo> filesInfo);

signals:
  void toggledSelected(int row, bool value);

private:
  QList<ImageInfo> m_filesInfo;
  // const int c_numCol = 4;
  const int NUM_COL = 4;
  const int CHECK_COL = 0;
  const int FILE_COL = 1;
  const int X_COL = 2;
  const int Y_COL = 3;
};

/// Basic image metadata container
struct ImageInfo
{
  ImageInfo(QString fileName, int m_, int n_, bool consistent_);
  QFileInfo fileInfo;
  int m;
  int n;
  bool consistent;
  bool selected;
};

} // namespace tomviz

#endif
