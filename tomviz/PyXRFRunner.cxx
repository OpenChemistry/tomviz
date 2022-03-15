/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFRunner.h"

#include "LoadDataReaction.h"
#include "ProgressDialog.h"
#include "PyXRFMakeHDF5Dialog.h"
#include "PyXRFProcessDialog.h"
#include "PythonUtilities.h"
#include "SelectItemsDialog.h"
#include "Utilities.h"

#include <QDebug>
#include <QDir>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QPointer>
#include <QtConcurrent>

namespace tomviz {

class PyXRFRunner::Internal : public QObject
{
public:
  QPointer<PyXRFRunner> parent;
  QPointer<QWidget> parentWidget;
  QPointer<PyXRFMakeHDF5Dialog> makeHDF5Dialog;
  QPointer<PyXRFProcessDialog> processDialog;
  QPointer<ProgressDialog> progressDialog;
  QFutureWatcher<bool> makeHDF5FutureWatcher;
  QFutureWatcher<bool> processFutureWatcher;

  // Python modules and functions
  Python::Module pyxrfModule;
  Python::Function makeHDF5Func;
  Python::Function processProjectionsFunc;

  // Options we will use as arguments to various pieces

  // General options
  QString workingDirectory;

  // Make HDF5 options
  int scanStart = 0;
  int scanStop = 0;
  bool successfulScansOnly = true;
  QString defaultLogFileName = "tomo_info.csv";

  // Process projection options
  QString parametersFile;
  QString logFile;
  QString icName;
  QString outputDirectory;
  bool skipProcessed = true;

  // Recon options
  QStringList selectedElements;

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
    connect(&makeHDF5FutureWatcher, &QFutureWatcher<bool>::finished, this,
            &Internal::makeHDF5Finished);
    connect(&processFutureWatcher, &QFutureWatcher<bool>::finished, this,
            &Internal::processProjectionsFinished);
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

  void importFunctions()
  {
    importModule();

    if (!makeHDF5Func.isValid()) {
      makeHDF5Func = pyxrfModule.findFunction("make_hdf5");
      if (!makeHDF5Func.isValid()) {
        qCritical() << "Failed to find function \"make_hdf5\"";
      }
    }

    if (!processProjectionsFunc.isValid()) {
      processProjectionsFunc = pyxrfModule.findFunction("process_projections");
      if (!processProjectionsFunc.isValid()) {
        qCritical() << "Failed to find function \"process_projections\"";
      }
    }
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
    importFunctions();
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
    workingDirectory = makeHDF5Dialog->workingDirectory();
    scanStart = makeHDF5Dialog->scanStart();
    scanStop = makeHDF5Dialog->scanStop();
    successfulScansOnly = makeHDF5Dialog->successfulScansOnly();

    auto useAlreadyExistingData = makeHDF5Dialog->useAlreadyExistingData();
    if (useAlreadyExistingData) {
      // Proceed to the next step
      showProcessProjectionsDialog();
    } else {
      runMakeHDF5();
    }
  }

  void runMakeHDF5()
  {
    progressDialog->clearOutputWidget();
    progressDialog->setText("Generating HDF5 Files...");
    progressDialog->show();
    auto future = QtConcurrent::run(this, &Internal::_runMakeHDF5);
    makeHDF5FutureWatcher.setFuture(future);
  }

  bool _runMakeHDF5()
  {
    Python python;

    if (!makeHDF5Func.isValid()) {
      qCritical() << "Failed to find function \"make_hdf5\"";
      return false;
    }

    Python::Dict kwargs;
    kwargs.set("start_scan", scanStart);
    kwargs.set("stop_scan", scanStop);
    kwargs.set("successful_scans_only", successfulScansOnly);
    kwargs.set("working_directory", workingDirectory);
    kwargs.set("log_file_name", defaultLogFileName);
    auto res = makeHDF5Func.call(kwargs);

    if (!res.isValid()) {
      qCritical() << "Error calling tomviz.pyxrf.make_hdf5";
      return false;
    }

    return true;
  }

  void makeHDF5Finished()
  {
    progressDialog->accept();

    auto success = makeHDF5FutureWatcher.result();
    if (!success) {
      QString msg = "Make HDF5 failed";
      qCritical() << msg;
      QMessageBox::critical(parentWidget, "Tomviz", msg);
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
    parametersFile = processDialog->parametersFile();
    logFile = processDialog->logFile();
    icName = processDialog->icName();
    outputDirectory = processDialog->outputDirectory();

    // Run process projections
    runProcessProjections();
  }

  void runProcessProjections()
  {
    progressDialog->clearOutputWidget();
    progressDialog->setText("Processing projections...");
    progressDialog->show();
    auto future = QtConcurrent::run(this, &Internal::_runProcessProjections);
    processFutureWatcher.setFuture(future);
  }

  bool _runProcessProjections()
  {
    Python python;

    if (!processProjectionsFunc.isValid()) {
      qCritical() << "Failed to find function \"process_projections\"";
      return false;
    }

    Python::Dict kwargs;
    kwargs.set("working_directory", workingDirectory);
    kwargs.set("parameters_file_name", parametersFile);
    kwargs.set("log_file_name", logFile);
    kwargs.set("ic_name", icName);
    kwargs.set("skip_processed", skipProcessed);
    kwargs.set("output_directory", outputDirectory);
    auto res = processProjectionsFunc.call(kwargs);

    if (!res.isValid()) {
      qCritical() << "Error calling tomviz.pyxrf.process_projections";
      return false;
    }

    return true;
  }

  void processProjectionsFinished()
  {
    progressDialog->accept();

    auto success = processFutureWatcher.result();
    if (!success || !validateOutputDirectory()) {
      QString msg = "Process projections failed";
      qCritical() << msg;
      QMessageBox::critical(parentWidget, "Tomviz", msg);
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
    auto res = extractElementsFunc.call(kwargs);

    if (!res.isValid()) {
      qCritical("Error calling tomviz.pyxrf.extract_elements");
      return;
    }

    QStringList ret;
    for (auto& item : res.toVariant().toList()) {
      ret.append(item.toString().c_str());
    }

    QString title = "Element extraction complete";
    auto text =
      QString("Elements were extracted to \"%1\".\n\nLoad the first one "
              "(\"%2\") into Tomviz?")
        .arg(outputPath)
        .arg(ret[0]);
    if (QMessageBox::question(parentWidget, title, text) == QMessageBox::Yes) {
      // Load the first one
      LoadDataReaction::loadData(ret[0]);
    }
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

void PyXRFRunner::start()
{
  m_internal->start();
}

} // namespace tomviz
