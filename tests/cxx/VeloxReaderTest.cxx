/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#pragma push_macro("slots")
#undef slots
#include "vtkPython.h" // must be first
#pragma pop_macro("slots")

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <pqPVApplicationCore.h>

#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include "EmdFormat.h"
#include "FileFormatManager.h"
#include "FileReader.h"
#include "PythonUtilities.h"

#include "TomvizTest.h"

using namespace tomviz;

class VeloxReaderTest : public QObject
{
  Q_OBJECT

private:
  QTemporaryDir m_dir;

  // A Velox file (as velox.py describes the format) of two 4 x 3 frames
  // taken at -20 and +20 degrees of alpha tilt, written with h5py.
  QString writeVelox()
  {
    QString path = m_dir.filePath("velox.emd");
    QString script =
      QStringLiteral(
        "import json, h5py, numpy as np\n"
        "with h5py.File(r'%1', 'w') as f:\n"
        "    f['Version'] = np.array([json.dumps(\n"
        "        {'format': 'Velox', 'version': '3'}).encode()])\n"
        "    g = f.create_group('Data/Image/0')\n"
        "    g['Data'] = np.arange(24, dtype=np.uint16).reshape(3, 4, 2)\n"
        "    cols = [json.dumps({'BinaryResult': {'PixelSize': {\n"
        "        'width': '5e-10', 'height': '5e-10'}, 'PixelUnitX': 'm'},\n"
        "        'Stage': {'AlphaTilt': repr(float(np.radians(a)))}})\n"
        "        .encode() for a in (-20.0, 20.0)]\n"
        "    m = np.zeros((300, 2), dtype=np.uint8)\n"
        "    for i, c in enumerate(cols):\n"
        "        m[:len(c), i] = np.frombuffer(c, np.uint8)\n"
        "    g['Metadata'] = m\n")
        .arg(path);
    Python python;
    if (PyRun_SimpleString(script.toUtf8().constData()) != 0) {
      return QString();
    }
    return path;
  }

private slots:
  void initTestCase()
  {
    QVERIFY(m_dir.isValid());
    // Registering stores the reader list in the application's settings.
    QStandardPaths::setTestModeEnabled(true);
    FileFormatManager::instance().registerPythonReaders();
    QVERIFY(FileFormatManager::instance().pythonReaderFactory("emd"));
  }

  // An .emd without an EMD node goes on to the Velox reader.
  void veloxFileIsRead()
  {
    QString path = writeVelox();
    QVERIFY(!path.isEmpty());
    QVERIFY(!EmdFormat::isEmd(path.toStdString()));

    auto result = readImageData({ path });
    QVERIFY(result.imageData);
    int dims[3];
    result.imageData->GetDimensions(dims);
    QCOMPARE(dims[0], 4);
    QCOMPARE(dims[1], 3);
    QCOMPARE(dims[2], 2);
    auto* angles = result.imageData->GetFieldData()->GetArray("tilt_angles");
    QVERIFY(angles);
    QCOMPARE(angles->GetNumberOfTuples(), vtkIdType(2));
    QVERIFY(qAbs(angles->GetComponent(0, 0) + 20.0) < 1e-9);
  }

  // A Berkeley EMD file is still read by the EMD reader.
  void berkeleyEmdFileIsReadAsBefore()
  {
    vtkNew<vtkImageData> image;
    image->SetDimensions(3, 4, 5);
    image->AllocateScalars(VTK_FLOAT, 1);
    image->GetPointData()->GetScalars()->Fill(2.0);
    QString path = m_dir.filePath("berkeley.emd");
    QVERIFY(EmdFormat::write(path.toStdString(), image));
    QVERIFY(EmdFormat::isEmd(path.toStdString()));

    auto result = readImageData({ path });
    QVERIFY(result.imageData);
    int dims[3];
    result.imageData->GetDimensions(dims);
    QCOMPARE(dims[0], 3);
    QCOMPARE(dims[1], 4);
    QCOMPARE(dims[2], 5);
  }
};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  pqPVApplicationCore appCore(argc, argv);

  Python::initialize();

  VeloxReaderTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "VeloxReaderTest.moc"
