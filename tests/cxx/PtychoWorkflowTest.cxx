/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>
#include <QThread>

#include <pqPVApplicationCore.h>

#include "PythonUtilities.h"
#include "PtychoDialog.h"
#include "PtychoRunner.h"

#include "TomvizTest.h"

using namespace tomviz;

const QDir ROOT_DATA_DIR = QString(SOURCE_DIR) + "/data";
const QDir DATA_DIR = ROOT_DATA_DIR.absolutePath() + "/Pt_Zn_Phase";

template <typename T>
T* findWidget()
{
  for (auto* widget : QApplication::topLevelWidgets()) {
    if (qobject_cast<T*>(widget)) {
      return qobject_cast<T*>(widget);
    }
  }

  return nullptr;
}

class PtychoWorkflowTest : public QObject
{
  Q_OBJECT

private:
  void downloadDataIfMissing()
  {
    if (DATA_DIR.exists()) {
      // Nothing to do
      return;
    }
    // Download the data if it is not present
    QString python = "python";
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains("TOMVIZ_TEST_PYTHON_EXECUTABLE")) {
      python = env.value("TOMVIZ_TEST_PYTHON_EXECUTABLE");
    }

    auto scriptFile = QFileInfo(QString(SOURCE_DIR) + "/fixtures/download_and_unzip.py");
    QString scriptPath = scriptFile.absoluteFilePath();

    QString url = "https://data.kitware.com/api/v1/file/6914aad883abdcd84d150c91/download";

    // We unzip into the parent directory, which will then have `Pt_Zn_Phase`
    // after unzipping.
    QStringList arguments;
    arguments << scriptPath << url << ROOT_DATA_DIR.absolutePath();

    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(python, arguments);

    // Timeout in seconds
    int timeout = 60 * 10;
    QVERIFY(process.waitForFinished(timeout * 1000));
    QVERIFY(process.exitCode() == 0);
  }

private slots:
  void initTestCase()
  {
    downloadDataIfMissing();
  }

  void cleanupTestCase() {}

  void runTest()
  {
    QString ptychoDir = DATA_DIR.absolutePath() + "/Ptycho/recon_result/";
    QString outputDir = DATA_DIR.absolutePath() + "/output/";

    auto* runner = new PtychoRunner(this);
    runner->setAutoLoadFinalData(false);
    runner->start();

    auto* dialog = findWidget<PtychoDialog>();
    QVERIFY(dialog);

    auto* ptychoDirEdit = dialog->findChild<QLineEdit*>("ptychoDirectory");
    QVERIFY(ptychoDirEdit);
    ptychoDirEdit->setText(ptychoDir);

    // Trigger the necessary changes
    emit ptychoDirEdit->editingFinished();

    auto* outputDirEdit = dialog->findChild<QLineEdit*>("outputDirectory");
    QVERIFY(outputDirEdit);
    outputDirEdit->setText(outputDir);

    dialog->accept();

    QStringList outputFileNames = {"ptycho_object.emd", "ptycho_probe.emd"};

    auto checkAllExist = [&outputDir, &outputFileNames]() {
      for (const auto& filename : outputFileNames) {
        if (!QFile::exists(outputDir + filename)) {
          return false;
        }

      }
      return true;
    };

    // Wait for it to finish, and verify the output files were written
    int timeout = 30;
    int waitTime = 0;
    bool found = false;

    while (waitTime < timeout) {
      if (checkAllExist()) {
        found = true;
        break;
      }
      QThread::sleep(1);
      ++waitTime;
    }
    // Verify everything was found
    QVERIFY(found);
  }

};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  pqPVApplicationCore appCore(argc, argv);

  Python::initialize();

  PtychoWorkflowTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "PtychoWorkflowTest.moc"
