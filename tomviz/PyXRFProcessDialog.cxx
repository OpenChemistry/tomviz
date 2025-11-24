/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFProcessDialog.h"
#include "ui_PyXRFProcessDialog.h"

#include "PythonUtilities.h"
#include "Utilities.h"

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
  QMap<QString, int> sidToRow;

  QStringList filteredSidList;

  double pixelSizeX = -1;
  double pixelSizeY = -1;

  Python::Module pyxrfModule;

  Internal(QString workingDir, PyXRFProcessDialog* p)
    : parent(p), workingDirectory(workingDir)
  {
    ui.setupUi(p);
    setParent(p);

    setupTableColumns();
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
    connect(ui.filterSidsString, &QLineEdit::editingFinished, this,
            &Internal::onFilterSidsStringChanged);

    connect(ui.selectLogFile, &QPushButton::clicked, this,
            &Internal::selectLogFile);
    connect(ui.selectParametersFile, &QPushButton::clicked, this,
            &Internal::selectParametersFile);
    connect(ui.selectOutputDirectory, &QPushButton::clicked, this,
            &Internal::selectOutputDirectory);

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
            &Internal::accepted);
    connect(ui.buttonBox, &QDialogButtonBox::helpRequested, this,
            [](){ openHelpUrl("https://tomviz.readthedocs.io/en/latest/workflows_pyxrf.html#process-projections"); });
  }

  void setupTableColumns()
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

    setHiddenRowsToUnused();
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

    if (filteredSidList.isEmpty()) {
      reason = "No SIDs were selected";
      return false;
    }

    // Check if there are any duplicate angles selected
    QStringList anglesUsed;
    QStringList anglesDuplicated;
    for (int i = 0; i < logFileData.size(); ++i) {
      auto use = logFileValue(i, "Use");
      if (use != "x" && use != "1") {
        // This angle wasn't used.
        continue;
      }

      auto angle = logFileValue(i, "Theta");
      if (anglesUsed.contains(angle) && !anglesDuplicated.contains(angle)) {
        anglesDuplicated.append(angle);
      }

      anglesUsed.append(angle);
    }

    if (anglesDuplicated.size() != 0) {
      QString title = "Duplicate angles detected";
      QString text = "The following duplicate angles were detected. Proceed anyways?";
      text += ("\n\n" + anglesDuplicated.join(", "));
      if (QMessageBox::question(parent, title, text) == QMessageBox::No) {
        reason = "Rejected proceeding with duplicate angles.";
        return false;
      }
    }

    return true;
  }

  void updateTable()
  {
    auto* table = ui.logFileTable;
    table->clear();

    readLogFile();

    setupTableColumns();
    table->setRowCount(filteredSidList.size());
    for (int row = 0; row < filteredSidList.size(); ++row) {
      auto sid = filteredSidList[row];
      int logFileRow = sidToRow[sid];
      for (auto col : tableColumns.keys()) {
        auto columnName = tableColumns[col];
        auto value = logFileValue(logFileRow, columnName);
        if (columnName == "Use") {
          // Special case
          auto* cb = createUseCheckbox(row, value);
          table->setCellWidget(row, col, cb);
          continue;
        }

        auto* item = new QTableWidgetItem(value);
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, col, item);
      }
    }
  }

  QWidget* createUseCheckbox(int row, QString value)
  {
    auto cb = new QCheckBox(parent);
    cb->setChecked(value == "x" || value == "1");
    connect(cb, &QCheckBox::toggled, this, [this, row](bool b) {
      QString val = b ? "1" : "0";
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
    sidToRow.clear();

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

    for (int row = 0; row < logFileData.size(); ++row) {
      auto sid = logFileValue(row, "Scan ID");
      sidToRow[sid] = row;
    }

    updateFilteredSidList();
  }

  QMap<QString, bool> sidVisibleMap()
  {
    QMap<QString, bool> ret;
    for (auto s : allSids()) {
      ret[s] = false;
    }

    for (auto s : filteredSidList) {
      ret[s] = true;
    }

    return ret;
  }

  void setHiddenRowsToUnused()
  {
    auto visibleMap = sidVisibleMap();
    for (int row = 0; row < logFileData.size(); ++row) {
      auto sid = logFileValue(row, "Scan ID");
      if (!visibleMap[sid]) {
        // Make sure "Use" is turned off
        setLogFileValue(row, "Use", "0");
      }
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

  QStringList allSids()
  {
    return sidToRow.keys();
  }

  void onFilterSidsStringChanged()
  {
    updateTable();
  }

  void updateFilteredSidList()
  {
    auto filterString = filterSidsString().trimmed();
    if (filterString.isEmpty()) {
      filteredSidList = allSids();
      return;
    }

    // Otherwise, run the Python function to filter out the list
    importModule();

    Python python;

    auto func = pyxrfModule.findFunction("filter_sids");
    if (!func.isValid()) {
      qCritical() << "Failed to import tomviz.pyxrf.filter_sids";
      return;
    }

    Python::Dict kwargs;
    kwargs.set("all_sids", allSids());
    kwargs.set("filter_string", filterString);
    auto res = func.call(kwargs);

    if (!res.isValid() || !res.isList()) {
      qCritical() << "Error calling tomviz.pyxrf.filter_sids";
      return;
    }

    filteredSidList.clear();
    auto resList = res.toList();
    for (int i = 0; i < resList.length(); ++i) {
      filteredSidList.append(resList[i].toString());
    }
  }

  QString defaultOutputDirectory() { return QDir::home().filePath("recon"); }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");

    setCommand(settings->value("pyxrfUtilsCommand", "pyxrf-utils").toString());

    settings->beginGroup("process");
    // Only load these settings if we are re-using the same previous
    // working directory. Otherwise, use all new settings
    auto previousWorkingDir = settings->value("previousProcessWorkingDir", "");
    setPyxrfGUICommand(
      settings->value("pyxrfGUICommand", "pyxrf").toString());
    if (workingDirectory == previousWorkingDir) {
      if (logFile().isEmpty()) {
        setLogFile(settings->value("logFile", "").toString());
      }
      setFilterSidsString(settings->value("filterSidsString", "").toString());
      setParametersFile(settings->value("parametersFile", "").toString());
      setOutputDirectory(
        settings->value("outputDirectory", defaultOutputDirectory()).toString());
    }
    setIcName(settings->value("icName", "sclr1_ch4").toString());
    setSkipProcessed(settings->value("skipProcessed", true).toBool());
    setRotateDatasets(
      settings->value("rotateDatasets", true).toBool());
    settings->endGroup();

    settings->endGroup();

    // Table might have been modified from the settings
    updateTable();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");

    // Do this in the general pyxrf settings
    settings->setValue("pyxrfUtilsCommand", command());

    settings->beginGroup("process");
    settings->setValue("previousProcessWorkingDir", workingDirectory);
    settings->setValue("logFile", logFile());
    settings->setValue("filterSidsString", filterSidsString());
    settings->setValue("pyxrfGUICommand", pyxrfGUICommand());
    settings->setValue("parametersFile", parametersFile());
    settings->setValue("outputDirectory", outputDirectory());
    settings->setValue("icName", icName());
    settings->setValue("skipProcessed", skipProcessed());
    settings->setValue("rotateDatasets", rotateDatasets());
    settings->endGroup();

    settings->endGroup();
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

    QString program = pyxrfGUICommand();
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

  QString command() const { return ui.command->text(); }

  void setCommand(const QString& s) { ui.command->setText(s); }

  QString pyxrfGUICommand() const { return ui.pyxrfGUICommand->text(); }

  void setPyxrfGUICommand(const QString& s) { ui.pyxrfGUICommand->setText(s); }

  QString parametersFile() const { return ui.parametersFile->text(); }

  void setParametersFile(QString s) { ui.parametersFile->setText(s); }

  QString logFile() const { return ui.logFile->text(); }

  void setLogFile(QString s) { ui.logFile->setText(s); }

  QString filterSidsString() const { return ui.filterSidsString->text(); }

  void setFilterSidsString(QString s) const { ui.filterSidsString->setText(s); }

  QString icName() const { return ui.icName->currentText(); }

  void setIcName(QString s) { ui.icName->setCurrentText(s); }

  QString outputDirectory() const { return ui.outputDirectory->text(); }

  void setOutputDirectory(QString s) { ui.outputDirectory->setText(s); }

  bool skipProcessed() const { return ui.skipProcessed->isChecked(); }

  void setSkipProcessed(bool b) { ui.skipProcessed->setChecked(b); }

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

QString PyXRFProcessDialog::command() const
{
  return m_internal->command();
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

bool PyXRFProcessDialog::skipProcessed() const
{
  return m_internal->skipProcessed();
}

bool PyXRFProcessDialog::rotateDatasets() const
{
  return m_internal->rotateDatasets();
}

} // namespace tomviz
