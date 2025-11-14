/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFProcessDialog.h"
#include "ui_PyXRFProcessDialog.h"

#include "PythonUtilities.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>

namespace tomviz {

class PyXRFProcessDialog::Internal : public QObject
{
public:
  Ui::PyXRFProcessDialog ui;
  QPointer<PyXRFProcessDialog> parent;

  bool pyxrfIsRunning = false;
  QString workingDirectory;

  // We're going to assume these files will be small and just
  // load the whole thing into memory...
  QList<QStringList> logFileData;
  QMap<QString, int> logFileColumnIndices;
  QMap<int, QString> tableColumns;

  double pixelSizeX = -1;
  double pixelSizeY = -1;

  Python::Module pyxrfModule;

  Internal(QString workingDir, PyXRFProcessDialog* p)
    : parent(p), workingDirectory(workingDir)
  {
    ui.setupUi(p);
    setParent(p);

    // Our embedded python in conda doesn't seem to have its paths set up
    // correctly. No idea why. Fix it.
    fixPythonPaths();

    setupTable();
    setupComboBoxes();
    setupConnections();

    if (QDir(workingDirectory).exists("tomo_info.csv")) {
      // Set the csv file automatically
      auto path = QDir(workingDirectory).filePath("tomo_info.csv");
      setLogFile(path);
    }
  }

  void setupConnections()
  {
    connect(ui.startPyXRFGUI, &QPushButton::clicked, this,
            &Internal::startPyXRFGUI);
    connect(ui.logFile, &QLineEdit::textChanged, this, &Internal::updateTable);

    connect(ui.selectLogFile, &QPushButton::clicked, this,
            &Internal::selectLogFile);
    connect(ui.selectParametersFile, &QPushButton::clicked, this,
            &Internal::selectParametersFile);
    connect(ui.selectOutputDirectory, &QPushButton::clicked, this,
            &Internal::selectOutputDirectory);

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
            &Internal::accepted);
  }

  void setupTable()
  {
    auto* table = ui.logFileTable;
    auto& columns = tableColumns;

    columns.clear();
    columns[0] = "Scan ID";
    columns[1] = "Theta";
    columns[2] = "Status";
    columns[3] = "Use";

    table->setColumnCount(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
      auto* header = new QTableWidgetItem(columns[i]);
      table->setHorizontalHeaderItem(i, header);
    }
  }

  void setupComboBoxes()
  {
    ui.icName->clear();
    ui.icName->addItems(icNames());
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

  void accepted()
  {
    QString reason;
    if (!validate(reason)) {
      QString title = "Invalid Settings";
      QMessageBox::critical(parent.data(), title, reason);
      parent->show();
      return;
    }

    readPixelSizes();
    writeLogFile();
    writeSettings();
    parent->accept();
  }

  bool validate(QString& reason)
  {
    // Make the parameters file and log file absolute if they are not
    if (!QFileInfo(logFile()).isAbsolute()) {
      setLogFile(QDir(workingDirectory).filePath(logFile()));
    }

    if (!QFileInfo(parametersFile()).isAbsolute()) {
      setParametersFile(QDir(workingDirectory).filePath(parametersFile()));
    }

    if (logFile().isEmpty() || !QFile::exists(logFile())) {
      reason = "Log file does not exist: " + logFile();
      return false;
    }

    if (parametersFile().isEmpty() || !QFile::exists(parametersFile())) {
      reason = "Parameters file does not exist: " + parametersFile();
      return false;
    }

    if (!QDir(outputDirectory()).exists()) {
      // First ask if the user wants to make it.
      QString title = "Directory does not exist";
      auto text = QString("Output directory \"%1\" does not exist. Create it?")
                    .arg(outputDirectory());
      if (QMessageBox::question(parent, title, text) == QMessageBox::Yes) {
        QDir().mkpath(outputDirectory());
      }
    }

    if (outputDirectory().isEmpty() || !QDir(outputDirectory()).exists()) {
      reason = "Output directory does not exist: " + outputDirectory();
      return false;
    }

    // Check if there are any duplicate angles selected
    QStringList anglesUsed;
    for (int i = 0; i < logFileData.size(); ++i) {
      auto use = logFileValue(i, "Use");
      if (use != "x" && use != "1") {
        // This angle wasn't used.
        continue;
      }

      auto angle = logFileValue(i, "Theta");
      if (anglesUsed.contains(angle)) {
        reason = "Angle '" + angle + "' was selected more than once.";
        return false;
      }

      anglesUsed.append(angle);
    }

    return true;
  }

  void updateTable()
  {
    auto* table = ui.logFileTable;
    table->clearContents();

    readLogFile();

    table->setRowCount(logFileData.size());
    for (int i = 0; i < logFileData.size(); ++i) {
      for (auto j : tableColumns.keys()) {
        auto column = tableColumns[j];
        auto value = logFileValue(i, column);
        if (column == "Use") {
          // Special case
          auto* cb = createUseCheckbox(i, value);
          table->setCellWidget(i, j, cb);
          continue;
        }

        auto* item = new QTableWidgetItem(value);
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, j, item);
      }
    }
  }

  QWidget* createUseCheckbox(int row, QString value)
  {
    auto cb = new QCheckBox(parent);
    cb->setChecked(value == "x" || value == "1");
    connect(cb, &QCheckBox::toggled, this, [this, row](bool b) {
      QString val = b ? "x" : "0";
      setLogFileValue(row, "Use", val);
    });

    return createTableWidget(cb);
  }

  QWidget* createTableWidget(QWidget* w)
  {
    // This is required to center the widget
    auto* tw = new QWidget(ui.logFileTable);
    auto* layout = new QHBoxLayout(tw);
    layout->addWidget(w);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return tw;
  }

  void readLogFile()
  {
    logFileData.clear();
    logFileColumnIndices.clear();

    QFile file(logFile());
    if (!file.exists()) {
      // No problem. Maybe the user is typing.
      return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
      qCritical()
        << QString("Failed to open log file \"%1\" with error: ").arg(logFile())
        << file.errorString();
      return;
    }

    QTextStream reader(&file);

    // Read the first line and save the column indices
    auto firstLineSplit = reader.readLine().split(',');
    for (int i = 0; i < firstLineSplit.size(); ++i) {
      logFileColumnIndices[firstLineSplit[i]] = i;
    }

    while (!reader.atEnd()) {
      auto line = reader.readLine();
      logFileData.append(line.split(','));
    }
  }

  void readPixelSizes()
  {
    pixelSizeX = -1;
    pixelSizeY = -1;

    // Find the first selected scan index
    int firstScanIdx = -1;
    for (int i = 0; i < logFileData.size(); ++i) {
      auto use = logFileValue(i, "Use");
      if (use == "x" || use == "1") {
        firstScanIdx = i;
        break;
      }
    }

    if (firstScanIdx == -1) {
      // Don't need to print an error message here, because we have
      // bigger problems that will be reported elsewhere.
      return;
    }

    auto scanId = logFileValue(firstScanIdx, "Scan ID");
    qInfo() << "Reading pixel sizes from the first checked scan: " << scanId;

    static QStringList columnsNeeded = {
      "X Start",
      "X Stop",
      "Num X",
      "Y Start",
      "Y Stop",
      "Num Y"
    };

    // We will store the values in here
    QMap<QString, double> values;
    for (const auto& colName : columnsNeeded) {
      auto value = logFileValue(firstScanIdx, colName);
      if (value.isEmpty()) {
        qCritical() << "Failed to locate value for column:" << colName;
        qCritical() << "Pixel sizes will not be set.";
        return;
      }

      bool ok;
      auto valueD = value.toDouble(&ok);
      if (!ok) {
        qCritical() << "Failed to convert column value for column" << colName
                    << "to double. Column value was:" << value;
        qCritical() << "Pixel sizes will not be set.";
        return;
      }
      values[colName] = valueD;
    }

    // If we made it here, we must have all column values we need.
    // Compute and set.
    pixelSizeX = (values["X Stop"] - values["X Start"]) / values["Num X"] * 1e3;
    pixelSizeY = (values["Y Stop"] - values["Y Start"]) / values["Num Y"] * 1e3;

    qInfo() << "Pixel sizes determined to be: " << pixelSizeX << pixelSizeY;
  }

  void writeLogFile()
  {
    QFile file(logFile());
    if (!file.exists()) {
      qCritical() << "Log file does not exist: " << logFile();
      return;
    }

    if (!file.open(QIODevice::WriteOnly)) {
      qCritical()
        << QString("Failed to open log file \"%1\" with error: ").arg(logFile())
        << file.errorString();
      return;
    }

    // Write the header row
    QStringList headerRow;
    for (int i = 0; i < logFileColumnIndices.size(); ++i) {
      auto key = logFileColumnIndices.key(i);
      if (key.isEmpty()) {
        qCritical() << "Could not find key for index: " << i;
        return;
      }

      headerRow.append(key);
    }

    QTextStream writer(&file);
    writer << headerRow.join(",") << "\n";

    for (int i = 0; i < logFileData.size(); ++i) {
      writer << logFileData[i].join(",");
      if (i < logFileData.size() - 1) {
        writer << "\n";
      }
    }
  }

  QString logFileValue(int row, QString column)
  {
    if (logFileData.isEmpty()) {
      qCritical() << "No log file data";
      return "";
    }

    if (!logFileColumnIndices.contains(column)) {
      qCritical() << "Column not found in log file: " << column;
      return "";
    }

    if (row >= logFileData.size()) {
      qCritical() << QString("Row %1 is out of bounds in log file").arg(row);
      return "";
    }

    auto col = logFileColumnIndices[column];
    if (col >= logFileData[row].size()) {
      qCritical()
        << QString("Column %1 is out of bounds in log file").arg(column);
      return "";
    }

    return logFileData[row][col];
  }

  void setLogFileValue(int row, QString column, QString value)
  {
    if (!logFileColumnIndices.contains(column)) {
      qCritical() << "Column not found in log file: " << column;
      return;
    }

    if (row >= logFileData.size()) {
      qCritical() << QString("Row %1 is out of bounds in log file").arg(row);
      return;
    }

    auto col = logFileColumnIndices[column];
    if (col >= logFileData[row].size()) {
      qCritical()
        << QString("Column %1 is out of bounds in log file").arg(column);
      return;
    }

    logFileData[row][col] = value;
  }

  QString defaultOutputDirectory() { return QDir::home().filePath("recon"); }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");
    settings->beginGroup("process");

    // Only load these settings if we are re-using the same previous
    // working directory. Otherwise, use all new settings
    auto previousWorkingDir = settings->value("previousProcessWorkingDir", "");
    if (workingDirectory == previousWorkingDir) {
      setParametersFile(settings->value("parametersFile", "").toString());
      setOutputDirectory(
        settings->value("outputDirectory", defaultOutputDirectory()).toString());
      if (logFile().isEmpty()) {
        setLogFile(settings->value("logFile", "").toString());
      }
    }
    setIcName(settings->value("icName", "").toString());
    setRotateDatasets(
      settings->value("rotateDatasets", true).toBool());

    settings->endGroup();
    settings->endGroup();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");
    settings->beginGroup("process");

    settings->setValue("previousProcessWorkingDir", workingDirectory);
    settings->setValue("parametersFile", parametersFile());
    settings->setValue("logFile", logFile());
    settings->setValue("icName", icName());
    settings->setValue("outputDirectory", outputDirectory());
    settings->setValue("rotateDatasets", rotateDatasets());

    settings->endGroup();
    settings->endGroup();
  }

  void fixPythonPaths()
  {
    importModule();

    Python python;

    auto func = pyxrfModule.findFunction("fix_python_paths");
    if (!func.isValid()) {
      qCritical() << "Failed to import tomviz.pyxrf.fix_python_paths";
      return;
    }

    auto res = func.call();
    if (!res.isValid()) {
      qCritical() << "Error calling tomviz.pyxrf.fix_python_paths";
    }
  }

  QStringList icNames()
  {
    QStringList ret;

    importModule();

    Python python;

    auto icNamesFunc = pyxrfModule.findFunction("ic_names");
    if (!icNamesFunc.isValid()) {
      qCritical() << "Failed to import tomviz.pyxrf.ic_names";
      return ret;
    }

    Python::Dict kwargs;
    kwargs.set("working_directory", workingDirectory);
    auto res = icNamesFunc.call(kwargs);

    if (!res.isValid()) {
      qCritical() << "Error calling tomviz.pyxrf.ic_names";
      return ret;
    }

    for (auto& item : res.toVariant().toList()) {
      ret.append(item.toString().c_str());
    }

    return ret;
  }

  void startPyXRFGUI()
  {
    if (pyxrfIsRunning) {
      // It's already running. Just return.
      return;
    }

    QString program = "pyxrf";
    auto environment = QProcessEnvironment::systemEnvironment();
    if (environment.contains("TOMVIZ_PYXRF_EXECUTABLE")) {
      program = environment.value("TOMVIZ_PYXRF_EXECUTABLE");
    }

    QStringList args;

    auto* process = new QProcess(this);

    // Forward stdout/stderr to this process
    process->setProcessChannelMode(QProcess::ForwardedChannels);

    process->start(program, args);

    pyxrfIsRunning = true;

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this]() { pyxrfIsRunning = false; });

    connect(
      process, &QProcess::errorOccurred, this,
      [this, process](QProcess::ProcessError err) {
        pyxrfIsRunning = false;

        QString title;
        QString msg;

        if (err == QProcess::FailedToStart) {
          title = "PyXRF failed to start";
          msg = QString("The program \"%1\" failed to start.\n\n")
                  .arg(process->program()) +
                "Try setting the environment variable "
                "\"TOMVIZ_PYXRF_EXECUTABLE\" to the full path, and restart "
                "tomviz.";
        } else {
          QString output = process->readAllStandardOutput();
          QString error = process->readAllStandardError();
          title = "PyXRF exited with an error";
          msg =
            QString("stdout: \"%1\"\n\nstderr: \"%2\"").arg(output).arg(error);
        }
        QMessageBox::critical(parent.data(), title, msg);
      });
  }

  void selectLogFile()
  {
    QString caption = "Select log file";
    QString filter = "*.csv";
    auto startPath = logFile() != "" ? logFile() : workingDirectory;
    auto file =
      QFileDialog::getOpenFileName(parent.data(), caption, startPath, filter);
    if (file.isEmpty()) {
      return;
    }

    setLogFile(file);
  }

  void selectParametersFile()
  {
    QString caption = "Select parameters file";
    QString filter = "*.json";
    auto startPath = parametersFile() != "" ? parametersFile() : workingDirectory;
    auto file = QFileDialog::getOpenFileName(parent.data(), caption,
                                             startPath, filter);
    if (file.isEmpty()) {
      return;
    }

    setParametersFile(file);
  }

  void selectOutputDirectory()
  {
    QString caption = "Select output directory";
    auto startPath = outputDirectory() != "" ? outputDirectory() : workingDirectory;
    auto dir = QFileDialog::getExistingDirectory(parent.data(), caption,
                                                 startPath);
    if (dir.isEmpty()) {
      return;
    }

    setOutputDirectory(dir);
  }

  QString parametersFile() const { return ui.parametersFile->text(); }

  void setParametersFile(QString s) { ui.parametersFile->setText(s); }

  QString logFile() const { return ui.logFile->text(); }

  void setLogFile(QString s) { ui.logFile->setText(s); }

  QString icName() const { return ui.icName->currentText(); }

  void setIcName(QString s) { ui.icName->setCurrentText(s); }

  QString outputDirectory() const { return ui.outputDirectory->text(); }

  void setOutputDirectory(QString s) { ui.outputDirectory->setText(s); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }

  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }
};

PyXRFProcessDialog::PyXRFProcessDialog(QString workingDirectory,
                                       QWidget* parent)
  : QDialog(parent), m_internal(new Internal(workingDirectory, this))
{
}

PyXRFProcessDialog::~PyXRFProcessDialog() = default;

void PyXRFProcessDialog::show()
{
  m_internal->readSettings();
  QDialog::show();
}

QString PyXRFProcessDialog::parametersFile() const
{
  return m_internal->parametersFile();
}

QString PyXRFProcessDialog::logFile() const
{
  return m_internal->logFile();
}

QString PyXRFProcessDialog::icName() const
{
  return m_internal->icName();
}

QString PyXRFProcessDialog::outputDirectory() const
{
  return m_internal->outputDirectory();
}

double PyXRFProcessDialog::pixelSizeX() const
{
  return m_internal->pixelSizeX;
}

double PyXRFProcessDialog::pixelSizeY() const
{
  return m_internal->pixelSizeY;
}

bool PyXRFProcessDialog::rotateDatasets() const
{
  return m_internal->rotateDatasets();
}

} // namespace tomviz
