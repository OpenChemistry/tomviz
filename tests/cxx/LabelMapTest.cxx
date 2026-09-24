/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QColor>
#include <QTest>

#include <set>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkDataArray.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include "pipeline/data/LabelMapData.h"

using namespace tomviz::pipeline;

namespace {

// A volume carrying labels 0, 1 and 2 in equal measure.
vtkSmartPointer<vtkImageData> threeLabelImage()
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(9, 9, 9);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto* p = static_cast<unsigned char*>(image->GetScalarPointer());
  for (int i = 0; i < 9 * 9 * 9; ++i) {
    p[i] = static_cast<unsigned char>(i % 3);
  }
  return vtkSmartPointer<vtkImageData>(image.GetPointer());
}

// A volume of a given scalar type whose values sweep a range.
vtkSmartPointer<vtkImageData> rampImage(int vtkType, double span)
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(16, 16, 16);
  image->AllocateScalars(vtkType, 1);
  auto* scalars = image->GetPointData()->GetScalars();
  for (vtkIdType i = 0; i < scalars->GetNumberOfTuples(); ++i) {
    scalars->SetTuple1(i, (i * span) / scalars->GetNumberOfTuples());
  }
  scalars->Modified();
  return vtkSmartPointer<vtkImageData>(image.GetPointer());
}

// Labels 0..5, with the values in @a removed sent to the background
vtkSmartPointer<vtkImageData> sixLabelImage(std::set<int> removed = {})
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(12, 12, 12);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto* p = static_cast<unsigned char*>(image->GetScalarPointer());
  for (int i = 0; i < 12 * 12 * 12; ++i) {
    const int label = i % 6;
    p[i] = static_cast<unsigned char>(removed.count(label) ? 0 : label);
  }
  return vtkSmartPointer<vtkImageData>(image.GetPointer());
}

QColor colorOf(LabelMapData& data, double value)
{
  const int index = data.labels().indexOfValue(value);
  return index >= 0 ? data.labels().at(index).color : QColor();
}

} // namespace

class LabelMapTest : public QObject
{
  Q_OBJECT

private slots:
  // Hiding a label drives its opacity to zero and leaves its neighbors
  // alone, and the choice survives the producing node running again.
  // Each execution publishes a brand new payload, so without the
  // hand-off in adoptLabelsFrom() every re-run would silently switch
  // the label back on.
  // A segmentation read from a file arrives as a plain volume, because
  // the reader cannot know its numbers are labels. What can be offered
  // as a label map is decided from the data rather than the port type.
  void plainVolumesAreReadableAsLabelsWhenTheirValuesAllow()
  {
    auto labels = std::make_shared<VolumeData>(threeLabelImage());
    QVERIFY(canInterpretAsLabelMap(labels));

    // Continuous data rules itself out by its scalar type alone: there
    // is nothing to enumerate in a float field.
    auto continuous =
      std::make_shared<VolumeData>(rampImage(VTK_FLOAT, 100.0));
    QVERIFY(!canInterpretAsLabelMap(continuous));

    // So does integer data spanning more values than the table holds.
    auto wide = std::make_shared<VolumeData>(
      rampImage(VTK_INT, 4.0 * LabelTable::maxLabels));
    QVERIFY(!canInterpretAsLabelMap(wide));

    // Right at the limit it is still offered.
    auto atLimit = std::make_shared<VolumeData>(
      rampImage(VTK_INT, LabelTable::maxLabels - 1));
    QVERIFY(canInterpretAsLabelMap(atLimit));

    // Nothing loaded is not something to offer it for.
    QVERIFY(!canInterpretAsLabelMap(nullptr));
  }

  // A label map adopted from a plain volume keeps its table in the sink
  // rather than in the port's payload, so this round trip is the only
  // thing carrying the user's colors and names into a state file.
  void labelTableSurvivesAStateFile()
  {
    auto data = std::make_shared<LabelMapData>(threeLabelImage());
    data->refreshLabels();
    auto& labels = data->labels();
    QCOMPARE(labels.count(), 3);

    labels.setName(1, "particle A");
    labels.setColor(1, QColor(10, 200, 30));
    labels.setVisible(2, false);
    const auto json = labels.serialize();

    LabelTable restored;
    restored.deserialize(json);
    QCOMPARE(restored.count(), 3);
    QCOMPARE(restored.at(1).name, QString("particle A"));
    QCOMPARE(restored.at(1).color, QColor(10, 200, 30));
    QCOMPARE(restored.at(2).visible, false);
    // Label 0 is background and starts hidden; that must not come back
    // as visible either.
    QCOMPARE(restored.at(0).visible, false);
  }

  void hiddenLabelSurvivesReexecution()
  {
    auto first = std::make_shared<LabelMapData>(threeLabelImage());
    first->refreshLabels();
    first->applyLabels();

    auto& labels = first->labels();
    QCOMPARE(labels.count(), 3);

    const int hidden = labels.indexOfValue(1.0);
    QVERIFY(hidden >= 0);
    labels.setVisible(hidden, false);
    first->applyLabels();

    auto* opacity = first->scalarOpacity();
    QVERIFY(opacity);
    QCOMPARE(opacity->GetValue(1.0), 0.0);
    QCOMPARE(opacity->GetValue(2.0), 1.0);

    // The node re-runs: same labels, different payload.
    auto second = std::make_shared<LabelMapData>(threeLabelImage());
    second->adoptLabelsFrom(*first);
    second->applyLabels();

    QCOMPARE(second->labels().count(), 3);
    QCOMPARE(second->labels().at(hidden).visible, false);
    QCOMPARE(second->scalarOpacity()->GetValue(1.0), 0.0);
    QCOMPARE(second->scalarOpacity()->GetValue(2.0), 1.0);
  }

  // Removing labels from the middle leaves every other label its color:
  // colors follow the label values, not the order labels were found in
  void labelColorsFollowTheirValues()
  {
    auto all = std::make_shared<LabelMapData>(sixLabelImage());
    all->refreshLabels();
    auto fewer = std::make_shared<LabelMapData>(sixLabelImage({ 2, 3 }));
    fewer->refreshLabels();

    QCOMPARE(fewer->labels().count(), 4);
    for (double value : { 1.0, 4.0, 5.0 }) {
      QVERIFY(colorOf(*fewer, value).isValid());
      QCOMPARE(colorOf(*fewer, value), colorOf(*all, value));
    }
    // Neighbors still differ
    QVERIFY(colorOf(*all, 4.0) != colorOf(*all, 5.0));
  }
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  LabelMapTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "LabelMapTest.moc"
