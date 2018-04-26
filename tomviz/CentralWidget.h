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
#ifndef tomvizCentralWidget_h
#define tomvizCentralWidget_h

#include <QMap>
#include <QPointer>
#include <QScopedPointer>
#include <QWidget>

#include <vtkSmartPointer.h>

class vtkImageData;
class vtkPVDiscretizableColorTransferFunction;
class vtkTable;

class QThread;
class QTimer;

namespace Ui {
class CentralWidget;
}

namespace tomviz {
class DataSource;
class HistogramMaker;
class Module;
class Transfer2DModel;
class Operator;

/// CentralWidget is a QWidget that is used as the central widget
/// for the application. This include a histogram at the top and a
/// ParaView view-layout widget at the bottom.
class CentralWidget : public QWidget
{
  Q_OBJECT

public:
  CentralWidget(QWidget* parent = nullptr, Qt::WindowFlags f = nullptr);
  ~CentralWidget() override;

public slots:
  /// Set the data source that is shown and color by the data source's
  /// color map
  void setActiveColorMapDataSource(DataSource*);

  /// Set the data source that is shown to the module's data source and color
  /// by the module's color map
  void setActiveModule(Module*);
  void setActiveOperator(Operator*);
  void onColorMapUpdated();
  void onColorLegendToggled(bool visibility);

private slots:
  void histogramReady(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>);
  void histogram2DReady(vtkSmartPointer<vtkImageData> input,
                        vtkSmartPointer<vtkImageData> output);
  void onColorMapDataSourceChanged();
  void refreshHistogram();

  /// The active transfer mode is tracked through the tab index of the TabWidget
  /// holding the 1D/2D histograms (tabs are expected to follow the order of
  /// Module::TransferMode).
  void onTransferModeChanged(const int mode);

private:
  Q_DISABLE_COPY(CentralWidget)

  /// Set of input checks shared between 1D and 2D histograms.
  vtkImageData* getInputImage(vtkSmartPointer<vtkImageData> input);

  /// Set the data source to from which the data is "histogrammed" and shown
  /// in the histogram view.
  void setColorMapDataSource(DataSource*);
  void setHistogramTable(vtkTable* table);

  QScopedPointer<Ui::CentralWidget> m_ui;
  QScopedPointer<QTimer> m_timer;

  QPointer<DataSource> m_activeColorMapDataSource;
  QPointer<Module> m_activeModule;
  HistogramMaker* m_histogramGen;
  QThread* m_worker;
  QMap<vtkImageData*, vtkSmartPointer<vtkTable>> m_histogramCache;
  Transfer2DModel* m_transfer2DModel;
};
}

#endif
