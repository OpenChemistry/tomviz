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

#ifndef tomvizHistogramManager_h

#include <QObject>

#include <vtkSmartPointer.h>

#include <QMap>

class QThread;

class vtkImageData;
class vtkTable;

namespace tomviz {
class HistogramMaker;

class HistogramManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  static HistogramManager& instance();

  void finalize();

  vtkSmartPointer<vtkTable> getHistogram(vtkSmartPointer<vtkImageData> image);
  vtkSmartPointer<vtkImageData> getHistogram2D(
    vtkSmartPointer<vtkImageData> image);

  bool hasHistogramsPending();

signals:
  void histogramReady(vtkSmartPointer<vtkImageData>, vtkSmartPointer<vtkTable>);
  void histogram2DReady(vtkSmartPointer<vtkImageData> input,
                        vtkSmartPointer<vtkImageData> output);

private slots:
  void histogramReadyInternal(vtkSmartPointer<vtkImageData>,
                              vtkSmartPointer<vtkTable>);
  void histogram2DReadyInternal(vtkSmartPointer<vtkImageData> input,
                                vtkSmartPointer<vtkImageData> output);

private:
  HistogramManager();
  ~HistogramManager();

  QMap<vtkImageData*, vtkSmartPointer<vtkTable>> m_histogramCache;
  QMap<vtkImageData*, vtkSmartPointer<vtkImageData>> m_histogram2DCache;
  QList<vtkImageData*> m_histogramsInProgress;
  QList<vtkImageData*> m_histogram2DsInProgress;
  HistogramMaker* m_histogramGen;
  QThread* m_worker;
};
} // namespace tomviz

#endif
