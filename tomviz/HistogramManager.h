/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
