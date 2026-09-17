/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCentralWidget_h
#define tomvizCentralWidget_h

#include <QPointer>
#include <QScopedPointer>
#include <QWidget>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>

#include <memory>
class vtkPVDiscretizableColorTransferFunction;

class QThread;
class QTimer;

namespace Ui {
class CentralWidget;
}

namespace tomviz {
class Transfer2DModel;

namespace pipeline {
class LegacyModuleSink;
class VolumeData;
using VolumeDataPtr = std::shared_ptr<VolumeData>;
} // namespace pipeline

/// CentralWidget is a QWidget that is used as the central widget
/// for the application. This include a histogram at the top and a
/// ParaView view-layout widget at the bottom.
class CentralWidget : public QWidget
{
  Q_OBJECT

public:
  CentralWidget(QWidget* parent = nullptr,
                Qt::WindowFlags f = Qt::WindowFlags());
  ~CentralWidget() override;

public slots:
  void onColorMapUpdated();
  void onColorLegendToggled(bool visibility);

  /// Set the active sink node for color map editing.
  void setActiveSinkNode(pipeline::LegacyModuleSink* sink);

  /// Set the active VolumeData for color map editing.
  void setActiveVolumeData(pipeline::VolumeDataPtr volumeData);

private slots:
  void histogramReady(vtkSmartPointer<vtkImageData>,
                      vtkSmartPointer<vtkTable>);
  void histogram2DReady(vtkSmartPointer<vtkImageData> input,
                        vtkSmartPointer<vtkImageData> output);
  void onColorMapDataSourceChanged();
  void refreshHistogram();

private:
  Q_DISABLE_COPY(CentralWidget)

  /// Set of input checks shared between 1D and 2D histograms.
  vtkImageData* getInputImage(vtkSmartPointer<vtkImageData> input);

  void setHistogramTable(vtkTable* table);

  QScopedPointer<Ui::CentralWidget> m_ui;
  QScopedPointer<QTimer> m_timer;

  Transfer2DModel* m_transfer2DModel;

  // New pipeline color map state
  QPointer<pipeline::LegacyModuleSink> m_activeSink;
  pipeline::VolumeDataPtr m_activeVolumeData;
};
} // namespace tomviz

#endif
