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

#include "DataSource.h"

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
  ImageStackModel(QObject* parent = nullptr);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

  Qt::ItemFlags flags(const QModelIndex& index) const;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole);

  QList<ImageInfo> getFileInfo() const;

public slots:
  void onFilesInfoChanged(QList<ImageInfo> filesInfo);
  void onStackTypeChanged(DataSource::DataSourceType stackType);

signals:
  void toggledSelected(int row, bool value);

private:
  QList<ImageInfo> m_filesInfo;
  DataSource::DataSourceType m_stackType = DataSource::DataSourceType::Volume;
  const int c_numCol = 5;
  const int c_checkCol = 0;
  const int c_fileCol = 1;
  const int c_xCol = 2;
  const int c_yCol = 3;
  const int c_posCol = 4;
};

/// Basic image metadata container
struct ImageInfo
{
  ImageInfo(QString fileName, int pos_ = 0, int m_ = -1, int n_ = -1,
            bool consistent_ = false);
  QFileInfo fileInfo;
  int pos;
  int m;
  int n;
  bool consistent;
  bool selected;
};

} // namespace tomviz

#endif
