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
  void onReconSlice0Changed() { this->onReconSliceChanged(0); };
  void onReconSlice1Changed() { this->onReconSliceChanged(1); };
  void onReconSlice2Changed() { this->onReconSliceChanged(2); };

  void updateWidgets();

  void onFinalReconButtonPressed();

  void showChangeColorMapDialog0() { this->showChangeColorMapDialog(0); };
  void showChangeColorMapDialog1() { this->showChangeColorMapDialog(1); };
  void showChangeColorMapDialog2() { this->showChangeColorMapDialog(2); };

  void changeColorMap0() { this->changeColorMap(0); }
  void changeColorMap1() { this->changeColorMap(1); }
  void changeColorMap2() { this->changeColorMap(2); }

private:
  void onReconSliceChanged(int idx);
  void showChangeColorMapDialog(int reconSlice);
  void changeColorMap(int reconSlice);

private:
  Q_DISABLE_COPY(RotateAlignWidget)

  class RAWInternal;
  QScopedPointer<RAWInternal> Internals;
};

}
#endif
