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
#ifndef tomvizRotateAlignWidget_h
#define tomvizRotateAlignWidget_h

#include <QWidget>

#include <QScopedPointer>

namespace tomviz
{
class DataSource;

class RotateAlignWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;
public:
  RotateAlignWidget(DataSource *data, QWidget* parent = NULL);
  ~RotateAlignWidget();

public slots:
  void setDataSource(DataSource *source);

signals:
  void creatingAlignedData();

protected slots:
  void onProjectionNumberChanged();
  void onRotationAxisChanged();
  void onReconSlice0Changed();
  void onReconSlice1Changed();
  void onReconSlice2Changed();

  void updateWidgets();

  void onFinalReconButtonPressed();

private:
  void onReconSliceChanged(int idx);

private:
  Q_DISABLE_COPY(RotateAlignWidget)

  class RAWInternal;
  QScopedPointer<RAWInternal> Internals;
};

}
#endif
