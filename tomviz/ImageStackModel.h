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
#include <QObject>
#include <QString>

struct ImageInfo;

/// Adapter to visualize the ImageInfo of a stack of images in a QTableView
class ImageStackModel : public QAbstractTableModel
{
public:
  ImageStackModel(QObject* parent, const QList<ImageInfo>& filesInfo);
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

private:
  const QList<ImageInfo> m_filesInfo;
};

/// Basic image metadata container
struct ImageInfo
{
  ImageInfo(QString fileName, int m_, int n_, bool consistent_);
  QFileInfo fileInfo;
  int m;
  int n;
  bool consistent;
};

#endif
