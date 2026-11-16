/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqServerResource.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkSMProxyManager.h>
#include <vtkSMReaderFactory.h>

#include "LoadDataReaction.h"
#include "pipeline/OutputPort.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sources/ReaderSourceNode.h"

using namespace tomviz;

namespace {

// An MRC file of 4 x 3 x 2 float voxels, 10 Angstrom apart
bool writeMrc(const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);
  out.setFloatingPointPrecision(QDataStream::SinglePrecision);
  const qint32 nx = 4, ny = 3, nz = 2;
  out << nx << ny << nz << qint32(2);            // mode 2: float32
  out << qint32(0) << qint32(0) << qint32(0);    // nxstart, nystart, nzstart
  out << nx << ny << nz;                         // mx, my, mz
  out << 10.0f * nx << 10.0f * ny << 10.0f * nz; // cell size, Angstrom
  out << 90.0f << 90.0f << 90.0f;                // cell angles
  out << qint32(1) << qint32(2) << qint32(3);    // mapc, mapr, maps
  out << 0.0f << 1.0f << 0.5f;                   // dmin, dmax, dmean
  while (file.pos() < 108) {
    out << quint8(0);
  }
  out << qint32(20140);                          // nversion: MRC2014
  while (file.pos() < 208) {
    out << quint8(0);
  }
  out.writeRawData("MAP ", 4);
  out << quint8(0x44) << quint8(0x44) << quint8(0) << quint8(0);
  while (file.pos() < 1024) {
    out << quint8(0);
  }
  for (int i = 0; i < nx * ny * nz; ++i) {
    out << float(i);
  }
  return out.status() == QDataStream::Ok;
}

} // namespace

class MrcReloadTest : public QObject
{
  Q_OBJECT

private slots:
  // IMOD's .st, .rec and .ali open through ParaView's MRC reader. A state
  // file reopens them through readImageData with the reader descriptor
  // saved at first load, which must name the file; for a state saved
  // before it did, the node's own file names stand in.
  void stateReloadReadsTheFile_data()
  {
    QTest::addColumn<QString>("suffix");
    // A state saved before the fix has no file name in the descriptor
    QTest::addColumn<bool>("oldState");
    for (const QString suffix : { "st", "rec", "ali" }) {
      QTest::newRow(qPrintable(suffix)) << suffix << false;
      QTest::newRow(qPrintable(suffix + " (old state)")) << suffix << true;
    }
  }

  void stateReloadReadsTheFile()
  {
    QFETCH(QString, suffix);
    QFETCH(bool, oldState);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("tilts." + suffix);
    QVERIFY(writeMrc(path));

    // First load, as File > Open does
    QJsonObject options;
    options["defaultModules"] = false;
    options["addToRecent"] = false;
    options["addToPipeline"] = false;
    options["createCameraOrbit"] = false;
    std::unique_ptr<pipeline::ReaderSourceNode> source(
      qobject_cast<pipeline::ReaderSourceNode*>(
        LoadDataReaction::loadData(path, options)));
    QVERIFY(source);
    auto first = source->outputPort("volume")
                   ->data()
                   .value<pipeline::VolumeDataPtr>();
    QVERIFY(first && first->isValid());

    // Save and reload the state: a new node from the saved JSON, executed
    QJsonObject state = source->serialize();
    auto readerOptions = state["readerOptions"].toObject();
    auto descriptor = readerOptions["reader"].toObject();
    QCOMPARE(descriptor["fileName"].toString(), path);
    if (oldState) {
      descriptor.remove("fileName");
      readerOptions["reader"] = descriptor;
      state["readerOptions"] = readerOptions;
    }
    pipeline::ReaderSourceNode reloaded;
    QVERIFY(reloaded.deserialize(state));
    QVERIFY(reloaded.execute());
    auto second = reloaded.outputPort("volume")
                    ->data()
                    .value<pipeline::VolumeDataPtr>();
    QVERIFY(second && second->isValid());
    int dims[3];
    second->imageData()->GetDimensions(dims);
    QCOMPARE(dims[0], 4);
    QCOMPARE(dims[1], 3);
    QCOMPARE(dims[2], 2);
    auto* a = first->imageData()->GetPointData()->GetScalars();
    auto* b = second->imageData()->GetPointData()->GetScalars();
    QVERIFY(b);
    for (vtkIdType i = 0; i < a->GetNumberOfTuples(); ++i) {
      QCOMPARE(b->GetTuple1(i), a->GetTuple1(i));
    }
  }
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  // As Tomviz's Behaviors does: the readers File > Open may choose from
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "MRCSeriesReader");
  vtkSMProxyManager::GetProxyManager()->GetReaderFactory()
    ->UpdateAvailableReaders();
  MrcReloadTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "MrcReloadTest.moc"
