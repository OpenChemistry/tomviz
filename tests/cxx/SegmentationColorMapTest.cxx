/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>

#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSmartPointer.h>

#include "pipeline/PortDataMetadata.h"
#include "pipeline/TransformNode.h"
#include "pipeline/transforms/LegacyPythonTransform.h"
#include "pipeline/data/LabelMapData.h"
#include "pipeline/data/VolumeData.h"

#include <array>
#include <map>
#include <set>
#include <vector>
#include <tuple>

using namespace tomviz;
using namespace tomviz::pipeline;

namespace {

// A transform with a primary input and a second one, declared in that
// order, like Fourier Mask (volume, mask) or Image Math (volume,
// second_dataset).
class TwoInputNode : public TransformNode
{
public:
  explicit TwoInputNode(const QString& second)
  {
    addInput("volume", PortType::ImageData);
    addInput(second, PortType::ImageData);
    addOutput("volume", PortType::ImageData);
  }

protected:
  QMap<QString, PortData> transform(const QMap<QString, PortData>&) override
  {
    return {};
  }
};

vtkSmartPointer<vtkImageData> smallImage(int labels)
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(8, 8, 8);
  image->AllocateScalars(VTK_INT, 1);
  auto* p = static_cast<int*>(image->GetScalarPointer());
  for (int i = 0; i < 8 * 8 * 8; ++i) {
    p[i] = i % labels;
  }
  return image;
}

// A plain volume with a one-color map, so a copy of it is recognizable
VolumeDataPtr coloredVolume(double r, double g, double b)
{
  auto vol = std::make_shared<VolumeData>(smallImage(100));
  vol->initColorMap();
  auto* ctf = vol->colorTransferFunction();
  ctf->RemoveAllPoints();
  ctf->AddRGBPoint(0.0, r, g, b);
  ctf->AddRGBPoint(99.0, r, g, b);
  return vol;
}

VolumeDataPtr labelMap()
{
  VolumeDataPtr labels = std::make_shared<LabelMapData>(smallImage(5));
  applyLabelMapColors(labels);
  return labels;
}

std::array<double, 3> colorAt(const VolumeDataPtr& vol, double value)
{
  std::array<double, 3> rgb;
  vol->colorTransferFunction()->GetColor(value, rgb.data());
  return rgb;
}

} // namespace

class SegmentationColorMapTest : public QObject
{
  Q_OBJECT

private slots:
  // The segmentation preset must land in data coordinates immediately:
  // no rescale (manual "Reset data range" or otherwise) may be needed
  // for each label to get its own color. Regression test for the
  // ApplyPreset rescale-to-current-range bug that squeezed the per-label
  // node positions into the fresh colormap's default [0, 1] range.
  void segmentationColorMapUsesDataCoordinates()
  {
    const int numLabels = 152; // labels 0..151
    vtkNew<vtkImageData> image;
    image->SetDimensions(40, 40, 40);
    image->AllocateScalars(VTK_INT, 1);
    auto* p = static_cast<int*>(image->GetScalarPointer());
    for (int i = 0; i < 40 * 40 * 40; ++i) {
      p[i] = i % numLabels;
    }
    auto vol = std::make_shared<VolumeData>(
      vtkSmartPointer<vtkImageData>(image.GetPointer()));

    QVERIFY(applySegmentationColorMap(*vol));

    auto* ctf = vol->colorTransferFunction();
    QVERIFY(ctf);
    QCOMPARE(ctf->GetSize(), 2 * numLabels);

    double range[2];
    ctf->GetRange(range);
    QCOMPARE(range[0], 0.0);
    QCOMPARE(range[1], double(numLabels - 1));

    // Every label maps to its own color, straight after the apply.
    std::map<std::tuple<int, int, int>, std::vector<int>> seen;
    for (int v = 0; v < numLabels; ++v) {
      double rgb[3];
      ctf->GetColor(double(v), rgb);
      seen[{ int(rgb[0] * 255), int(rgb[1] * 255),
             int(rgb[2] * 255) }].push_back(v);
    }
    for (auto& entry : seen) {
      if (entry.second.size() > 1) {
        QString labels;
        for (int v : entry.second) {
          labels += QString::number(v) + " ";
        }
        qWarning("color (%d %d %d) shared by labels: %s",
                 std::get<0>(entry.first), std::get<1>(entry.first),
                 std::get<2>(entry.first), qPrintable(labels));
      }
    }
    QCOMPARE(int(seen.size()), numLabels);

    // Rescaling to the data range (what "Reset data range" and the
    // per-node post-execution rescale do) must be an identity: every
    // label keeps its own color afterwards.
    vol->rescaleColorMap();
    std::set<std::tuple<int, int, int>> after;
    for (int v = 0; v < numLabels; ++v) {
      double rgb[3];
      ctf->GetColor(double(v), rgb);
      after.insert({ int(rgb[0] * 255), int(rgb[1] * 255),
                     int(rgb[2] * 255) });
    }
    QCOMPARE(int(after.size()), numLabels);

    // Background label 0 renders transparent.
    auto* opacity = vol->scalarOpacity();
    QVERIFY(opacity);
    QCOMPARE(opacity->GetValue(0.0), 0.0);
    QCOMPARE(opacity->GetValue(1.0), 1.0);
  }

  // Fourier Mask's output is continuous data: the label map on its mask
  // input must not color it, even though "mask" sorts before "volume".
  void outputsNeverTakeALabelMapsColors()
  {
    TwoInputNode node("mask");
    auto primary = coloredVolume(1.0, 0.0, 0.0);
    QMap<QString, PortData> inputs{
      { "volume", PortData(primary, PortType::Volume) },
      { "mask", PortData(labelMap(), PortType::LabelMap) },
    };
    QCOMPARE(colorMapSource(&node, inputs), primary);

    auto output = std::make_shared<VolumeData>(smallImage(100));
    inheritOutputMetadata(&node, inputs,
                          { { "volume", PortData(output, PortType::Volume) } });
    QVERIFY(output->hasColorMap());
    QCOMPARE(colorAt(output, 50.0), (std::array<double, 3>{ 1.0, 0.0, 0.0 }));

    // With only a label map to copy from, the output starts afresh
    QMap<QString, PortData> onlyLabels{
      { "volume", PortData(labelMap(), PortType::LabelMap) },
      { "mask", PortData(labelMap(), PortType::LabelMap) },
    };
    QVERIFY(!colorMapSource(&node, onlyLabels));
    auto fresh = std::make_shared<VolumeData>(smallImage(100));
    inheritOutputMetadata(&node, onlyLabels,
                          { { "volume", PortData(fresh, PortType::Volume) } });
    QVERIFY(!fresh->hasColorMap());
  }

  // Image Math: the primary input's colors, though "second_dataset"
  // sorts first
  void outputsTakeThePrimaryInputsColors()
  {
    TwoInputNode node("second_dataset");
    auto primary = coloredVolume(1.0, 0.0, 0.0);
    auto second = coloredVolume(0.0, 0.0, 1.0);
    QMap<QString, PortData> inputs{
      { "volume", PortData(primary, PortType::Volume) },
      { "second_dataset", PortData(second, PortType::Volume) },
    };
    QCOMPARE(colorMapSource(&node, inputs), primary);

    // A primary input with no color map yet gives way to the other
    auto bare = std::make_shared<VolumeData>(smallImage(100));
    inputs["volume"] = PortData(bare, PortType::Volume);
    QCOMPARE(colorMapSource(&node, inputs), second);
  }

  // The FFT's output is a spectrum: a color map tuned for the data it
  // came from means nothing there, so its description opts out.
  void operatorsCanDeclineInheritance()
  {
    auto primary = coloredVolume(1.0, 0.0, 0.0);
    QMap<QString, PortData> inputs{
      { "volume", PortData(primary, PortType::Volume) },
    };

    LegacyPythonTransform fft;
    fft.setJSONDescription(
      R"({"name": "FFT_AbsLog", "inheritColorMap": false, "parameters": []})");
    QVERIFY(!fft.inheritsColorMap());
    QVERIFY(!colorMapSource(&fft, inputs));

    LegacyPythonTransform blur;
    blur.setJSONDescription(R"({"name": "GaussianFilter", "parameters": []})");
    QVERIFY(blur.inheritsColorMap());
    QCOMPARE(colorMapSource(&blur, inputs), primary);
  }
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  SegmentationColorMapTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "SegmentationColorMapTest.moc"
