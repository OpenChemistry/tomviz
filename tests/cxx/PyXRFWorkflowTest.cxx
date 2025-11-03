/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>
#include <QThread>
#include <QTimer>

#include <pqPVApplicationCore.h>

#include "PythonUtilities.h"
#include "PyXRFRunner.h"
#include "PyXRFMakeHDF5Dialog.h"
#include "PyXRFProcessDialog.h"
#include "SelectItemsDialog.h"

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

class PyXRFWorkflowTest : public QObject
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

    QString url = "https://data.kitware.com/api/v1/file/690891b803d1a144cbcf422b/download";

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
    QString workingDir = DATA_DIR.absolutePath() + "/";

    auto* runner = new PyXRFRunner(this);
    runner->start();

    auto* makeHDF5Dialog = findWidget<PyXRFMakeHDF5Dialog>();
    QVERIFY(makeHDF5Dialog);

    // Set the method to already existing
    auto* method = makeHDF5Dialog->findChild<QComboBox*>("method");
    QVERIFY(method);
    method->setCurrentText("Already Existing");

    auto* workingDirEdit = makeHDF5Dialog->findChild<QLineEdit*>("workingDirectory");
    QVERIFY(workingDirEdit);
    workingDirEdit->setText(workingDir);

    makeHDF5Dialog->accept();

    auto* processDialog = findWidget<PyXRFProcessDialog>();
    QVERIFY(processDialog);

    auto* logFile = processDialog->findChild<QLineEdit*>("logFile");
    QVERIFY(logFile);
    logFile->setText(workingDir + "log.csv");

    auto* paramsFile = processDialog->findChild<QLineEdit*>("parametersFile");
    QVERIFY(paramsFile);
    paramsFile->setText(workingDir + "pyxrf_model_parameters_157397.json");

    auto* outputDir = processDialog->findChild<QLineEdit*>("outputDirectory");
    QVERIFY(outputDir);
    outputDir->setText(workingDir + "recon");

    auto* icName = processDialog->findChild<QComboBox*>("icName");
    QVERIFY(icName);
    icName->setCurrentText("sclr1_ch4");

    // After accepting, a modal dialog will appear. Start posting
    // events to check it and accept it when it appears.
    bool allFound = false;
    auto checkFunc = [&allFound](){
      auto* dialog = findWidget<SelectItemsDialog>();
      if (!dialog) {
        return;
      }

      // Just accept the dialog
      // Another dialog will appear asking to load the datasets into
      // Tomviz. We can just decline it.
      // Declare the function first so we can call it recursively.
      std::function<void()> declineNextDialog;
      declineNextDialog = [&allFound, &declineNextDialog]() {
        auto* finalDialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!finalDialog) {
          QTimer::singleShot(500, declineNextDialog);
          return;
        }

        allFound = true;
        finalDialog->reject();
      };
      QTimer::singleShot(0, declineNextDialog);
      dialog->accept();
      QApplication::processEvents();
    };

    QTimer::singleShot(0, checkFunc);
    processDialog->accept();

    // Keep processing events until all dialogs have
    // been found and accepted/rejected.
    int timeElapsed = 0;
    int maxTime = 30;
    while (!allFound && timeElapsed < maxTime) {
      QThread::sleep(1);
      QTimer::singleShot(0, checkFunc);
      QApplication::processEvents();
      timeElapsed += 1;
    }

    // Verify everything was found
    QVERIFY(allFound);

    // Verify that one of the output files now exist

  }

};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  pqPVApplicationCore appCore(argc, argv);

  Python::initialize();

  PyXRFWorkflowTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "PyXRFWorkflowTest.moc"
