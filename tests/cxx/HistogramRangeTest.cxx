/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>

#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkSMProxy.h>
#include <vtkSmartPointer.h>

#include "HistogramWidget.h"
#include "pipeline/data/VolumeData.h"

using namespace tomviz;
using namespace tomviz::pipeline;

class HistogramRangeTest : public QObject
{
  Q_OBJECT

private slots:
  // Every range change strips the placeholder nodes the widget keeps on
  // the data ends, rescales the rest and puts them back. The opacity
  // proxy only learns of client-side edits on the next event-loop turn,
  // so the rescale used to act on the stale nodes: Reset Range left a
  // narrowed opacity window in place, and each auto-contrast Apply
  // compressed the window further until VTK ran out of texture size.
  void rescaleUsesTheCurrentOpacityNodes()
  {
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(101, 1, 1);
    vtkNew<vtkFloatArray> values;
    values->SetName("values");
    values->SetNumberOfTuples(101);
    for (int i = 0; i <= 100; ++i) {
      values->SetValue(i, i);
    }
    image->GetPointData()->SetScalars(values);

    auto volume = std::make_shared<VolumeData>(image);
    volume->initColorMap();
    auto* lut = vtkDiscretizableColorTransferFunction::SafeDownCast(
      volume->colorMap()->GetClientSideObject());
    QVERIFY(lut);

    HistogramWidget widget;
    widget.setVolumeData(volume);
    widget.setLUTProxy(volume->colorMap());

    // A window on [40, 60], with placeholders on the data ends
    auto* opacity = lut->GetScalarOpacityFunction();
    QVERIFY(opacity);
    opacity->RemoveAllPoints();
    opacity->AddPoint(0, 0);
    opacity->AddPoint(40, 0);
    opacity->AddPoint(60, 1);
    opacity->AddPoint(100, 1);
    QCoreApplication::processEvents();

    widget.onResetRangeClicked();

    // The window now spans the data: a ramp from 0 to 1
    QVERIFY(qAbs(opacity->GetValue(25) - 0.25) < 1e-6);
    QVERIFY(qAbs(opacity->GetValue(75) - 0.75) < 1e-6);
  }
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  HistogramRangeTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "HistogramRangeTest.moc"
