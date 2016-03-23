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

#include <QScopedPointer>
#include <QWidget>
#include <QMap>
#include <QPointer>
#include <vtkNew.h>
#include <vtkWeakPointer.h>
#include <vtkSmartPointer.h>

class vtkColorTransferControlPointsItem;
class vtkColorTransferFunctionItem;
class vtkObject;
class vtkSMSourceProxy;
class vtkContextView;
class vtkEventQtSlotConnect;
class vtkImageData;
class vtkPiecewiseFunction;
class vtkPVDiscretizableColorTransferFunction;
class vtkTable;
class vtkChartHistogram;

class QThread;

namespace tomviz
{
class DataSource;
class HistogramMaker;
class Module;

/// CentralWidget is a QWidget that is used as the central widget
/// for the application. This include a histogram at the top and a
/// ParaView view-layout widget at the bottom.
class CentralWidget : public QWidget
{
  Q_OBJECT

  typedef QWidget Superclass;

public:
  CentralWidget(QWidget* parent = nullptr, Qt::WindowFlags f = nullptr);
  virtual ~CentralWidget();

public slots:
  /// Set the data source that is shown and color by the data source's
  /// color map
  void setActiveDataSource(DataSource*);
  /// Set the data source that is shown to the module's data source and color
  /// by the module's color map
  void setActiveModule(Module*);

  void onColorMapUpdated();

private slots:
  void histogramReady(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>);
  void histogramClicked(vtkObject *caller);
  void onDataSourceChanged();
  void refreshHistogram();
  void onScalarOpacityFunctionChanged();
  void onCurrentPointEditEvent();

private:
  Q_DISABLE_COPY(CentralWidget)

  /// Set the data source to from which the data is "histogrammed" and shown
  /// in the histogram view.
  void setDataSource(DataSource*);
  void setHistogramTable(vtkTable *table);

  class CWInternals;
  QScopedPointer<CWInternals> Internals;
  vtkNew<vtkContextView> HistogramView;
  vtkNew<vtkChartHistogram> Chart;

  vtkNew<vtkContextView> TransferFunctionView;
  vtkNew<vtkColorTransferControlPointsItem> ColorTransferControlPointsItem;
  vtkNew<vtkColorTransferFunctionItem> ColorTransferFunctionItem;

  vtkNew<vtkEventQtSlotConnect> EventLink;
  QPointer<DataSource> ADataSource;
  QPointer<Module> AModule;
  HistogramMaker *HistogramGen;
  QThread *Worker;
  QMap<vtkImageData *, vtkSmartPointer<vtkTable> > HistogramCache;
  vtkPVDiscretizableColorTransferFunction *LUT;
  vtkPiecewiseFunction *ScalarOpacityFunction;
};

}

#endif
