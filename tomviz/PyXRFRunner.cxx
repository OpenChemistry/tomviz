/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFRunner.h"

#include "DataSource.h"
#include "EmdFormat.h"
#include "LoadDataReaction.h"
#include "ProgressDialog.h"
#include "PyXRFMakeHDF5Dialog.h"
#include "PyXRFProcessDialog.h"
#include "PythonUtilities.h"
#include "SelectItemsDialog.h"
#include "Utilities.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QtConcurrent>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

namespace tomviz {

class PyXRFRunner::Internal : public QObject
{
public:
  QPointer<PyXRFRunner> parent;
  QPointer<QWidget> parentWidget;
  QPointer<PyXRFMakeHDF5Dialog> makeHDF5Dialog;
  QPointer<PyXRFProcessDialog> processDialog;
  QPointer<ProgressDialog> progressDialog;
  QProcess makeHDF5Process;
  QProcess remakeCsvFileProcess;
  QProcess processProjectionsProcess;

  // Python modules and functions
  Python::Module pyxrfModule;

  // Options we will use as arguments to various pieces

  // General options
  QString workingDirectory;
  QString pyxrfUtilsCommand;

  // Make HDF5 options
  int scanStart = 0;
  int scanStop = 0;
  bool successfulScansOnly = true;
  bool remakeCsvFile = false;
  QString defaultLogFileName = "tomo_info.csv";

  // Process projection options
  QString parametersFile;
  QString logFile;
  QString icName;
  QString outputDirectory;
  bool skipProcessed = true;
  double pixelSizeX = -1;
  double pixelSizeY = -1;
  bool rotateDatasets = true;

  // Recon options
  QStringList selectedElements;

  bool autoLoadFinalData = true;

  Internal(PyXRFRunner* p) : parent(p)
  {
    setParent(p);
    parentWidget = mainWidget();

    progressDialog = new ProgressDialog(parentWidget);
    progressDialog->setWindowTitle("Tomviz");
    progressDialog->showOutputWidget(true);
    progressDialog->resize(progressDialog->width(), 500);

    setupConnections();
  }

  void setupConnections()
  {
    connect(&makeHDF5Process, &QProcess::finished, this,
            &Internal::makeHDF5Finished);
    connect(&makeHDF5Process, &QProcess::readyReadStandardOutput, this,
            &Internal::_printProcStdout);
    connect(&makeHDF5Process, &QProcess::readyReadStandardError, this,
            &Internal::_printProcStderr);

    connect(&remakeCsvFileProcess, &QProcess::finished, this,
            &Internal::remakeCsvFileFinished);
    connect(&remakeCsvFileProcess, &QProcess::readyReadStandardOutput, this,
            &Internal::_printProcStdout);
    connect(&remakeCsvFileProcess, &QProcess::readyReadStandardError, this,
            &Internal::_printProcStderr);


    connect(&processProjectionsProcess, &QProcess::finished, this,
            &Internal::processProjectionsFinished);
    connect(&processProjectionsProcess, &QProcess::readyReadStandardOutput, this,
            &Internal::_printProcStdout);
    connect(&processProjectionsProcess, &QProcess::readyReadStandardError, this,
            &Internal::_printProcStderr);
  }

  void importModule()
  {
    Python python;

    if (pyxrfModule.isValid()) {
      return;
    }

    pyxrfModule = python.import("tomviz.pyxrf");
    if (!pyxrfModule.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf\" module";
    }
  }

  bool isInstalled()
  {
    importModule();

    Python python;

    auto installed = pyxrfModule.findFunction("installed");
    if (!installed.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf.installed\"";
      return false;
    }

    auto res = installed.call();

    if (!res.isValid()) {
      qCritical() << "Error calling \"tomviz.pyxrf.installed\"";
      return false;
    }

    return res.toBool();
  }

  QString importError()
  {
    importModule();

    Python python;

    auto func = pyxrfModule.findFunction("import_error");
    if (!func.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf.import_error\"";
      return "import_error not found";
    }

    auto res = func.call();

    if (!res.isValid()) {
      qCritical() << "Error calling \"tomviz.pyxrf.import_error\"";
      return "import_error not found";
    }

    return res.toString();
  }

  void importFunctions()
  {
    importModule();
  }

  template <typename T>
  void clearWidget(QPointer<T> d)
  {
    if (!d) {
      return;
    }

    d->hide();
    d->deleteLater();
    d.clear();
  }

  void clear()
  {
    clearWidget(makeHDF5Dialog);
    clearWidget(processDialog);
  }

  void start()
  {
    clear();
    importModule();
    showMakeHDF5Dialog();
  }

  void showMakeHDF5Dialog()
  {
    clearWidget(makeHDF5Dialog);

    makeHDF5Dialog = new PyXRFMakeHDF5Dialog(parentWidget);
    connect(makeHDF5Dialog.data(), &QDialog::accepted, this,
            &Internal::makeHDF5DialogAccepted);
    makeHDF5Dialog->show();
  }

  void makeHDF5DialogAccepted()
  {
    // Gather the settings and decide what to do
    pyxrfUtilsCommand = makeHDF5Dialog->command();
    workingDirectory = makeHDF5Dialog->workingDirectory();
    scanStart = makeHDF5Dialog->scanStart();
    scanStop = makeHDF5Dialog->scanStop();
    successfulScansOnly = makeHDF5Dialog->successfulScansOnly();
    remakeCsvFile = makeHDF5Dialog->remakeCsvFile();

    auto useAlreadyExistingData = makeHDF5Dialog->useAlreadyExistingData();
    if (useAlreadyExistingData) {
      if (remakeCsvFile) {
        runRemakeCsvFile();
      } else {
        // Proceed to the next step
        showProcessProjectionsDialog();
      }
    } else {
      runMakeHDF5();
    }
  }

  void runMakeHDF5()
  {
    progressDialog->clearOutputWidget();
    progressDialog->setText("Generating HDF5 Files...");
    progressDialog->show();

    QString program = pyxrfUtilsCommand;
    QStringList args;

    args << "make-hdf5" << workingDirectory
         << "-s" << QString::number(scanStart)
         << "-e" << QString::number(scanStop)
         << "-l" << defaultLogFileName;

    if (successfulScansOnly) {
      args.append("-b");
    }

    qInfo() << "Running:" << program + " " + args.join(" ");
    makeHDF5Process.start(program, args);
  }

  void makeHDF5Finished()
  {
    progressDialog->accept();

    auto success = makeHDF5Process.exitStatus() == QProcess::NormalExit;
    if (!success) {
      QString msg = "Make HDF5 failed";
      qCritical() << msg;
      QMessageBox::critical(parentWidget, "Tomviz", msg);
      // Show the dialog again
      showMakeHDF5Dialog();
      return;
    }

    showProcessProjectionsDialog();
  }

  void runRemakeCsvFile()
  {
    progressDialog->clearOutputWidget();
    progressDialog->setText("Remaking CSV file...");
    progressDialog->show();

    QString program = pyxrfUtilsCommand;
    QStringList args;

    QString rangeString = QString::number(scanStart) +
                          ":" + QString::number(scanStop + 1);
    args << "make-csv" << "-i"
         << "-w" << workingDirectory
         << "-s" << rangeString
         << defaultLogFileName;

    qInfo() << "Running:" << program + " " + args.join(" ");
    remakeCsvFileProcess.start(program, args);
  }

  void remakeCsvFileFinished()
  {
    progressDialog->accept();

    auto success = remakeCsvFileProcess.exitStatus() == QProcess::NormalExit;
    if (!success) {
      QString msg = "Remake CSV file failed";
      qCritical() << msg;
      QMessageBox::critical(parentWidget, "Tomviz", msg);
      // Show the dialog again
      showMakeHDF5Dialog();
      return;
    }

    showProcessProjectionsDialog();
  }

  void showProcessProjectionsDialog()
  {
    if (!validateWorkingDirectory()) {
      // Go back to the make HDF5 dialog
      showMakeHDF5Dialog();
      return;
    }

    clearWidget(processDialog);

    processDialog = new PyXRFProcessDialog(workingDirectory, parentWidget);
    connect(processDialog.data(), &QDialog::accepted, this,
            &Internal::processDialogAccepted);
    // If the user rejects the process dialog, go back to
    // the make HDF5 dialog.
    connect(processDialog.data(), &QDialog::rejected, this,
            &Internal::showMakeHDF5Dialog);
    processDialog->show();
  }

  bool validateWorkingDirectory()
  {
    // Make sure there is at least one .h5 file inside
    auto dataFiles = QDir(workingDirectory).entryList(QStringList("*.h5"));
    if (dataFiles.isEmpty()) {
      QString title = "Invalid Settings";
      auto msg =
        QString("Working directory \"%1\" must contain at least one .h5 file")
          .arg(workingDirectory);
      QMessageBox::critical(parentWidget, title, msg);
    }
    return !dataFiles.isEmpty();
  }

  void processDialogAccepted()
  {
    // Pull out the options that were chosen
    pyxrfUtilsCommand = processDialog->command();
    parametersFile = processDialog->parametersFile();
    logFile = processDialog->logFile();
    icName = processDialog->icName();
    outputDirectory = processDialog->outputDirectory();
    pixelSizeX = processDialog->pixelSizeX();
    pixelSizeY = processDialog->pixelSizeY();
    skipProcessed = processDialog->skipProcessed();
    rotateDatasets = processDialog->rotateDatasets();

    // Make sure the output directory exists
    QDir().mkpath(outputDirectory);

    // Run process projections
    runProcessProjections();
  }

  void runProcessProjections()
  {
    progressDialog->clearOutputWidget();
    progressDialog->setText("Processing projections...");
    progressDialog->show();

    QString program = pyxrfUtilsCommand;
    QStringList args;

    args << "process-projections" << workingDirectory
         << "-p" << parametersFile
         << "-l" << logFile
         << "-i" << icName
         << "-o" << outputDirectory;

    if (skipProcessed) {
      args << "-s";
    }

    qInfo() << "Running:" << program + " " + args.join(" ");
    processProjectionsProcess.start(program, args);
  }

  void _printProcStdout()
  {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) {
      return;
    }

    auto output = proc->readAllStandardOutput();
    if (output.size() == 0) {
      return;
    }

    // Remove the ending newline because qInfo() will add one
    if (output.endsWith("\r\n")) {
      output.chop(2);
    } else if (output.endsWith('\n')) {
      output.chop(1);
    }
    qInfo() << output.constData();
  }

  void _printProcStderr()
  {
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) {
      return;
    }

    auto output = proc->readAllStandardError();
    if (output.size() == 0) {
      return;
    }

    // Remove the ending newline because qWarning() will add one
    if (output.endsWith("\r\n")) {
      output.chop(2);
    } else if (output.endsWith('\n')) {
      output.chop(1);
    }
    qWarning() << output.constData();
  }

  void processProjectionsFinished()
  {
    progressDialog->accept();

    auto success = processProjectionsProcess.exitStatus() == QProcess::NormalExit;
    if (!success || !validateOutputDirectory()) {
      QString msg = "Process projections failed";
      qCritical() << msg;
      QMessageBox::critical(parentWidget, "Tomviz", msg);
      // Show the dialog again
      showProcessProjectionsDialog();
      return;
    }

    selectElements();
  }

  bool validateOutputDirectory()
  {
    if (!QDir(outputDirectory).exists("tomo.h5")) {
      auto msg =
        QString("Output \"tomo.h5\" file not found in output directory \"%1\"")
          .arg(outputDirectory);
      QMessageBox::critical(parentWidget, "Tomviz", msg);
      return false;
    }

    return true;
  }

  QString outputFile() { return QDir(outputDirectory).filePath("tomo.h5"); }

  QStringList outputVolumes()
  {
    QStringList ret;

    Python python;

    auto listElementsFunc = pyxrfModule.findFunction("list_elements");
    if (!listElementsFunc.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf.list_elements\"";
      return ret;
    }

    Python::Dict kwargs;
    kwargs.set("filename", outputFile());
    auto res = listElementsFunc.call(kwargs);

    if (!res.isValid()) {
      qCritical("Error calling tomviz.pyxrf.list_elements");
      return ret;
    }

    for (auto& item : res.toVariant().toList()) {
      ret.append(item.toString().c_str());
    }

    return ret;
  }

  void selectElements()
  {
    auto options = outputVolumes();

    // By default, select all items that start with a capital letter and
    // contain a hyphen.
    QList<bool> defaultSelections;
    for (auto& item : options) {
      bool select = item.contains("_") && item[0].isUpper();
      defaultSelections.append(select);
    }

    SelectItemsDialog dialog(options, parentWidget);
    dialog.setWindowTitle("Select elements to extract");
    dialog.setSelections(defaultSelections);

    selectedElements.clear();
    while (true) {
      if (!dialog.exec()) {
        return;
      }

      if (!dialog.selectedItems().isEmpty()) {
        break;
      }

      QString msg = "At least one element must be selected";
      qCritical() << msg;
      QMessageBox::critical(parentWidget, "Tomviz", msg);
    }

    selectedElements = dialog.selectedItems();
    extractSelectedElements();
  }

  void extractSelectedElements()
  {
    Python python;

    auto extractElementsFunc = pyxrfModule.findFunction("extract_elements");
    if (!extractElementsFunc.isValid()) {
      qCritical() << "Failed to import \"tomviz.pyxrf.extract_elements\"";
      return;
    }

    std::vector<Variant> variantList;
    for (auto& element : selectedElements) {
      variantList.push_back(element.toStdString());
    }

    QString outputPath = QDir(outputDirectory).filePath("extracted_elements");
    Python::Dict kwargs;
    kwargs.set("filename", outputFile());
    kwargs.set("elements", variantList);
    kwargs.set("output_path", outputPath);
    kwargs.set("rotate_datasets", rotateDatasets);
    kwargs.set("pixel_size_x", pixelSizeX);
    kwargs.set("pixel_size_y", pixelSizeY);
    auto res = extractElementsFunc.call(kwargs);

    if (!res.isValid()) {
      qCritical("Error calling tomviz.pyxrf.extract_elements");
      return;
    }

    QStringList ret;
    for (auto& item : res.toVariant().toList()) {
      ret.append(item.toString().c_str());
    }

    if (ret.size() == 0) {
      qCritical("No elements were extracted");
      return;
    }

    if (autoLoadFinalData) {
      loadElementsIntoArray(ret);
      QString title = "Element extraction complete";
      auto text =
        QString("Elements were extracted to \"%1\" and loaded into Tomviz")
          .arg(outputPath);
      QMessageBox::information(parentWidget, title, text);
    }
  }

  void loadElementsIntoArray(const QStringList& fileList) {
    // Load the first file into a DataSource
    auto* dataSource = LoadDataReaction::loadData(fileList[0]);
    if (!dataSource || !dataSource->imageData()) {
      qCritical() << "Failed to load file:" << fileList[0];
      return;
    }

    auto* rootImageData = dataSource->imageData();
    auto* rootPointData = rootImageData->GetPointData();
    auto newRootName = QFileInfo(fileList[0]).baseName();
    rootPointData->GetScalars()->SetName(newRootName.toStdString().c_str());

    // The other files should have identical metadata. We'll just load
    // the image data for those, and add them to the point data.
    EmdFormat format;
    for (int i = 1; i < fileList.size(); ++i) {
      vtkNew<vtkImageData> imageData;
      format.read(fileList[i].toStdString(), imageData);
      if (!imageData || !imageData->GetPointData()->GetScalars()) {
        qCritical() << "Failed to read image data for file:" << fileList[i];
        continue;
      }

      auto* scalars = imageData->GetPointData()->GetScalars();
      auto newName = QFileInfo(fileList[i]).baseName();
      scalars->SetName(newName.toStdString().c_str());

      // Add the array to the root image data
      rootPointData->AddArray(scalars);
    }

    // Sort the list, and make the first one alphabetically be selected
    auto sortedList = fileList;
    sortedList.sort();
    auto firstName = QFileInfo(sortedList[0]).baseName();

    dataSource->setActiveScalars(firstName.toStdString().c_str());
    dataSource->setLabel("Extracted Elements");
    dataSource->dataModified();

    // Write this to an EMD format
    QString saveFile = QFileInfo(sortedList[0]).dir().absoluteFilePath("extracted_elements.emd");
    EmdFormat::write(saveFile.toStdString(), dataSource);
    dataSource->setFileName(saveFile);
  }
};

PyXRFRunner::PyXRFRunner(QObject* parent)
  : QObject(parent), m_internal(new Internal(this))
{
}

PyXRFRunner::~PyXRFRunner() = default;

bool PyXRFRunner::isInstalled()
{
  return m_internal->isInstalled();
}

QString PyXRFRunner::importError()
{
  return m_internal->importError();
}

void PyXRFRunner::start()
{
  m_internal->start();
}

void PyXRFRunner::setAutoLoadFinalData(bool b)
{
  m_internal->autoLoadFinalData = b;
}

} // namespace tomviz
