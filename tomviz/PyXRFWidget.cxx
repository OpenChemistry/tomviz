/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PyXRFWidget.h"
#include "ui_PyXRFWidget.h"

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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTextStream>

namespace {

bool executableExists(const QString& command)
{
  if (command.isEmpty()) {
    return false;
  }
  if (command.contains('/') || command.contains(QDir::separator())) {
    QFileInfo info(command);
    return info.isFile() && info.isExecutable();
  }
  return !QStandardPaths::findExecutable(command).isEmpty();
}

QString findPyxrfUtilsCommand(const QString& savedCommand)
{
  if (executableExists(savedCommand)) {
    return savedCommand;
  }

  const QStringList candidates = { "run-pyxrf-utils", "pyxrf-utils" };
  for (const auto& candidate : candidates) {
    if (executableExists(candidate)) {
      return candidate;
    }
  }

  const QString absoluteFallback =
    "/nsls2/data/hxn/legacy/users/data_analysis/tomviz_utils/bin/"
    "run-pyxrf-utils";
  if (executableExists(absoluteFallback)) {
    return absoluteFallback;
  }

  return "";
}

} // anonymous namespace

namespace tomviz {

class PyXRFWidget::Internal : public QObject
{
public:
  Ui::PyXRFWidget ui;
  QPointer<PyXRFWidget> parent;

  bool pyxrfIsRunning = false;

  struct ScanEntry
  {
    int scanId;
    double theta;
    QString status;
    bool use;
  };
  QList<ScanEntry> scanEntries;

  Python::Module pyxrfModule;

  Internal(PyXRFWidget* p) : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    setupTableColumns();
    setupConnections();
  }

  void setupConnections()
  {
    connect(ui.selectWorkingDirectory, &QPushButton::clicked, this,
            &Internal::selectWorkingDirectory);
    connect(ui.selectParametersFile, &QPushButton::clicked, this,
            &Internal::selectParametersFile);
    connect(ui.selectCsvOutput, &QPushButton::clicked, this,
            &Internal::selectCsvOutput);

    connect(ui.skipDownloads, &QCheckBox::toggled, this,
            [this](bool checked) {
              ui.redownloadSuccessful->setEnabled(!checked);
              ui.downloadData->setEnabled(!checked);
            });

    connect(ui.downloadData, &QPushButton::clicked, this,
            &Internal::onDownloadData);

    connect(ui.loadSidsFromTxtOrCSV, &QPushButton::clicked, this,
            &Internal::onLoadSidsFromTxt);
    connect(ui.applyFilter, &QPushButton::clicked, this,
            &Internal::applyFilter);

    // Write the table out without running the operator, so a scan list
    // can be prepared up front and shared with the ptycho workflow.
    auto* saveScanListButton = new QPushButton("Save Scan List...", parent);
    saveScanListButton->setToolTip(
      "Save the listed scans as a CSV (Scan ID, Theta, Use) that this "
      "dialog and the ptycho dialog can load back in.");
    ui.filterLayout->addWidget(saveScanListButton);
    connect(saveScanListButton, &QPushButton::clicked, this,
            &Internal::saveScanList);

    connect(ui.startPyXRFGUI, &QPushButton::clicked, this,
            &Internal::startPyXRFGUI);

    connect(ui.workingDirectory, &QLineEdit::editingFinished, this,
            &Internal::onDirectoryOrRangeChanged);
    connect(ui.scanRange, &QLineEdit::editingFinished, this,
            &Internal::onDirectoryOrRangeChanged);
  }

  void setupTableColumns()
  {
    auto* table = ui.scanTable;
    table->setColumnCount(4);
    table->setHorizontalHeaderItem(0, new QTableWidgetItem("Scan ID"));
    table->setHorizontalHeaderItem(1, new QTableWidgetItem("Theta"));
    table->setHorizontalHeaderItem(2, new QTableWidgetItem("Status"));
    table->setHorizontalHeaderItem(3, new QTableWidgetItem("Use"));
  }

  void setupComboBoxes()
  {
    ui.icName->clear();
    auto names = icNames();
    if (names.isEmpty()) {
      names.append("sclr1_ch4");
    }
    ui.icName->addItems(names);
    int idx = ui.icName->findText("sclr1_ch4");
    if (idx >= 0) {
      ui.icName->setCurrentIndex(idx);
    }
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

  // --- Accessors ---

  QString command() const { return ui.command->text(); }
  void setCommand(const QString& s) { ui.command->setText(s); }

  QString workingDirectory() const { return ui.workingDirectory->text(); }
  void setWorkingDirectory(const QString& s)
  {
    ui.workingDirectory->setText(s);
  }

  QString scanRange() const { return ui.scanRange->text().trimmed(); }
  void setScanRange(const QString& s) { ui.scanRange->setText(s); }

  bool skipDownloads() const { return ui.skipDownloads->isChecked(); }
  void setSkipDownloads(bool b) { ui.skipDownloads->setChecked(b); }

  bool redownloadSuccessful() const { return ui.redownloadSuccessful->isChecked(); }
  void setRedownloadSuccessful(bool b) { ui.redownloadSuccessful->setChecked(b); }

  QString parametersFile() const { return ui.parametersFile->text(); }
  void setParametersFile(const QString& s) { ui.parametersFile->setText(s); }

  QString icName() const { return ui.icName->currentText(); }
  void setIcName(const QString& s) { ui.icName->setCurrentText(s); }

  bool skipProcessed() const { return ui.skipProcessed->isChecked(); }
  void setSkipProcessed(bool b) { ui.skipProcessed->setChecked(b); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }
  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }

  QString csvOutput() const { return ui.csvOutput->text().trimmed(); }
  void setCsvOutput(const QString& s) { ui.csvOutput->setText(s); }

  QString pyxrfGUICommand() const { return ui.pyxrfGUICommand->text(); }
  void setPyxrfGUICommand(const QString& s)
  {
    ui.pyxrfGUICommand->setText(s);
  }

  QString skipScanIds() const
  {
    QJsonArray arr;
    for (const auto& entry : scanEntries) {
      if (!entry.use) {
        arr.append(entry.scanId);
      }
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
  }

  // --- Helpers ---

  QString defaultWorkingDirectory() const
  {
    return QDir::home().filePath("data");
  }

  // --- UI Slots ---

  void selectWorkingDirectory()
  {
    auto dir = QFileDialog::getExistingDirectory(
      parent.data(), "Select data directory", workingDirectory());
    if (!dir.isEmpty()) {
      setWorkingDirectory(dir);
      onDirectoryOrRangeChanged();
    }
  }

  void onDirectoryOrRangeChanged()
  {
    populateScanTable();
    setupComboBoxes();
    autoApplyFilter();
  }

  void selectParametersFile()
  {
    auto startPath =
      parametersFile().isEmpty() ? workingDirectory() : parametersFile();
    auto file = QFileDialog::getOpenFileName(
      parent.data(), "Select parameters file", startPath, "*.json");
    if (!file.isEmpty()) {
      setParametersFile(file);
    }
  }

  void saveScanList()
  {
    if (scanEntries.isEmpty()) {
      QMessageBox::information(parent.data(), "Save Scan List",
                               "There are no scans to save.");
      return;
    }
    auto startPath =
      csvOutput().isEmpty() ? workingDirectory() : csvOutput();
    auto path = QFileDialog::getSaveFileName(parent.data(), "Save scan list",
                                             startPath, "CSV Files (*.csv)");
    if (path.isEmpty()) {
      return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::warning(parent.data(), "Save Scan List",
                           QString("Could not write %1").arg(path));
      return;
    }
    // No version column: PyXRF scans have no reconstruction versions.
    // The ptycho dialog reads this file as scan ids plus Use flags.
    QTextStream out(&file);
    out << "Scan ID,Theta,Use\n";
    for (const auto& entry : scanEntries) {
      bool hasTheta = entry.status != "fail" && entry.status != "missing";
      out << entry.scanId << ','
          << (hasTheta ? QString::number(entry.theta, 'f', 3) : QString())
          << ',' << (entry.use ? 1 : 0) << '\n';
    }
  }

  void selectCsvOutput()
  {
    auto startPath =
      csvOutput().isEmpty() ? workingDirectory() : csvOutput();
    auto file = QFileDialog::getSaveFileName(
      parent.data(), "Select output CSV file", startPath,
      "CSV Files (*.csv)");
    if (!file.isEmpty()) {
      setCsvOutput(file);
    }
  }

  // --- Download & Table ---

  void onDownloadData()
  {
    auto range = scanRange();
    if (range.isEmpty()) {
      QMessageBox::warning(parent.data(), "Missing Scan Range",
                           "Please enter a scan range before downloading.");
      return;
    }

    auto cmd = command();
    if (!executableExists(cmd)) {
      QMessageBox::critical(
        parent.data(), "Command Not Found",
        QString("The pyxrf-utils executable \"%1\" was not found.")
          .arg(cmd.isEmpty() ? QString("(empty)") : cmd));
      return;
    }

    QStringList args = { "make-hdf5", workingDirectory(), "--range", range };
    if (redownloadSuccessful()) {
      args.append("--force");
    }

    auto* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    ui.downloadData->setEnabled(false);
    ui.downloadData->setText("Downloading...");

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process](int exitCode, QProcess::ExitStatus) {
              ui.downloadData->setEnabled(true);
              ui.downloadData->setText("Download Data");
              process->deleteLater();
              if (exitCode == 0) {
                populateScanTable();
                setupComboBoxes();
              } else {
                QMessageBox::warning(parent.data(), "Download Failed",
                                     "pyxrf-utils make-hdf5 failed. "
                                     "Check the terminal output for details.");
              }
            });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError) {
              ui.downloadData->setEnabled(true);
              ui.downloadData->setText("Download Data");
              QMessageBox::critical(
                parent.data(), "Download Failed",
                QString("Failed to start \"%1\"").arg(process->program()));
              process->deleteLater();
            });

    process->start(cmd, args);
  }

  void populateScanTable()
  {
    scanEntries.clear();

    auto wd = workingDirectory();
    if (wd.isEmpty() || !QDir(wd).exists()) {
      rebuildTableUI();
      return;
    }

    importModule();

    Python python;
    auto func = pyxrfModule.findFunction("read_scan_metadata");
    if (!func.isValid()) {
      qCritical() << "Failed to find tomviz.pyxrf.read_scan_metadata";
      rebuildTableUI();
      return;
    }

    Python::Dict kwargs;
    kwargs.set("working_directory", wd);
    kwargs.set("scan_range", scanRange());
    auto res = func.call(kwargs);
    if (!res.isValid() || !res.isList()) {
      rebuildTableUI();
      return;
    }

    auto resList = res.toList();
    for (int i = 0; i < resList.length(); ++i) {
      auto item = resList[i];
      if (!item.isDict()) {
        continue;
      }

      auto dict = item.toDict();
      int scanId = static_cast<int>(dict["scan_id"].toLong());
      double theta = dict["theta"].toDouble();
      QString status = dict["status"].toString();
      ScanEntry entry;
      entry.scanId = scanId;
      entry.theta = theta;
      entry.status = status;
      entry.use = (status != "fail" && status != "missing");
      scanEntries.append(entry);
    }

    rebuildTableUI();
  }

  void rebuildTableUI()
  {
    auto* table = ui.scanTable;
    table->clearContents();
    table->setRowCount(scanEntries.size());

    for (int i = 0; i < scanEntries.size(); ++i) {
      const auto& entry = scanEntries[i];
      bool failed = (entry.status == "fail");

      auto* idItem = new QTableWidgetItem(QString::number(entry.scanId));
      idItem->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, 0, idItem);

      auto* thetaItem = new QTableWidgetItem(
        (failed || entry.status == "missing")
          ? QString("-")
          : QString::number(entry.theta, 'f', 3));
      thetaItem->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, 1, thetaItem);

      auto* statusItem = new QTableWidgetItem(entry.status);
      statusItem->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, 2, statusItem);

      auto* cb = new QCheckBox(parent);
      cb->setChecked(entry.use);
      cb->setEnabled(!failed);
      connect(cb, &QCheckBox::toggled, this, [this, i](bool b) {
        if (i < scanEntries.size()) {
          scanEntries[i].use = b;
        }
      });

      auto* tw = new QWidget(table);
      auto* layout = new QHBoxLayout(tw);
      layout->addWidget(cb);
      layout->setAlignment(Qt::AlignCenter);
      layout->setContentsMargins(0, 0, 0, 0);
      table->setCellWidget(i, 3, tw);
    }
  }

  // --- Filter SIDs ---

  void onLoadSidsFromTxt()
  {
    auto filePath = QFileDialog::getOpenFileName(
      parent.data(), "Select txt or csv file", workingDirectory(),
      "Text/CSV Files (*.txt *.csv)");
    if (filePath.isEmpty()) {
      return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      qCritical() << "Failed to open file:" << filePath;
      return;
    }

    QTextStream reader(&file);

    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
      loadSidsFromCsv(reader);
    } else {
      loadSidsFromTxt(reader);
    }
  }

  void loadSidsFromTxt(QTextStream& reader)
  {
    auto sids = readSidsFromText(reader);
    ui.filterSidsString->setText(sids.join(", "));
  }

  void loadSidsFromCsv(QTextStream& reader)
  {
    // Read header to find column indices
    auto header = reader.readLine().trimmed();
    auto columns = header.split(',');
    for (auto& col : columns) {
      col = col.trimmed();
    }

    // Match headers the way the ptycho scan list parser does: case-
    // and punctuation-insensitive, so "Scan ID", "Scan_ID", and "SID"
    // are all understood.
    auto normalized = [](const QString& s) {
      QString out;
      for (auto ch : s.toLower()) {
        if (ch.isLetterOrNumber()) {
          out.append(ch);
        }
      }
      return out;
    };
    int sidCol = -1;
    int useCol = -1;
    for (int i = 0; i < columns.size(); ++i) {
      auto key = normalized(columns[i]);
      if (sidCol < 0 && (key == "scanid" || key == "sid")) {
        sidCol = i;
      } else if (useCol < 0 && key == "use") {
        useCol = i;
      }
    }
    if (sidCol < 0) {
      qCritical() << "CSV file has no \"Scan ID\" column";
      return;
    }

    QStringList sids;
    QList<int> sidInts;
    QList<bool> useFlags;
    while (!reader.atEnd()) {
      auto line = reader.readLine().trimmed();
      if (line.isEmpty() || line.startsWith('#')) {
        continue;
      }
      auto fields = line.split(',');
      if (sidCol >= fields.size()) {
        continue;
      }
      auto sidStr = fields[sidCol].trimmed();
      sids.append(sidStr);
      sidInts.append(sidStr.toInt());

      if (useCol >= 0 && useCol < fields.size()) {
        auto val = fields[useCol].trimmed();
        auto lower = val.toLower();
        useFlags.append(lower == "1" || lower == "x" || lower == "true" ||
                        lower == "yes");
      }
    }

    ui.filterSidsString->setText(sids.join(", "));

    // If the CSV has a "Use" column, apply it to the scan table
    if (!useFlags.isEmpty()) {
      for (int i = 0; i < sidInts.size() && i < useFlags.size(); ++i) {
        for (auto& entry : scanEntries) {
          if (entry.scanId == sidInts[i]) {
            entry.use = useFlags[i];
            break;
          }
        }
      }
      rebuildTableUI();
    }
  }

  void applyFilter()
  {
    auto filterString = ui.filterSidsString->text().trimmed();
    if (filterString.isEmpty() || scanEntries.isEmpty()) {
      return;
    }

    importModule();

    Python python;
    auto func = pyxrfModule.findFunction("filter_sids");
    if (!func.isValid()) {
      qCritical() << "Failed to find tomviz.pyxrf.filter_sids";
      return;
    }

    QStringList allSids;
    for (const auto& entry : scanEntries) {
      if (entry.status != "fail") {
        allSids.append(QString::number(entry.scanId));
      }
    }

    Python::Dict kwargs;
    kwargs.set("all_sids", allSids);
    kwargs.set("filter_string", filterString);
    auto res = func.call(kwargs);

    if (!res.isValid() || !res.isList()) {
      qCritical() << "Error calling tomviz.pyxrf.filter_sids";
      return;
    }

    QSet<int> matchedIds;
    auto resList = res.toList();
    for (int i = 0; i < resList.length(); ++i) {
      matchedIds.insert(resList[i].toString().toInt());
    }

    for (int i = 0; i < scanEntries.size(); ++i) {
      auto& entry = scanEntries[i];
      if (entry.status == "fail" || entry.status == "missing") {
        continue;
      }
      entry.use = matchedIds.contains(entry.scanId);
    }

    rebuildTableUI();
  }

  void autoApplyFilter()
  {
    if (!ui.filterSidsString->text().trimmed().isEmpty() &&
        !scanEntries.isEmpty()) {
      applyFilter();
    }
  }

  // --- IC Names ---

  QStringList icNames()
  {
    QStringList ret;
    importModule();

    Python python;
    auto func = pyxrfModule.findFunction("ic_names");
    if (!func.isValid()) {
      return ret;
    }

    Python::Dict kwargs;
    kwargs.set("working_directory", workingDirectory());
    kwargs.set("scan_range", scanRange());
    auto res = func.call(kwargs);

    if (!res.isValid()) {
      return ret;
    }

    for (auto& item : res.toVariant().toList()) {
      ret.append(item.toString().c_str());
    }
    return ret;
  }

  // --- PyXRF GUI ---

  void startPyXRFGUI()
  {
    if (pyxrfIsRunning) {
      return;
    }

    QString program = pyxrfGUICommand();
    auto environment = QProcessEnvironment::systemEnvironment();
    if (environment.contains("TOMVIZ_PYXRF_EXECUTABLE")) {
      program = environment.value("TOMVIZ_PYXRF_EXECUTABLE");
    }

    auto* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->start(program, QStringList());

    pyxrfIsRunning = true;

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this]() { pyxrfIsRunning = false; });

    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError err) {
              pyxrfIsRunning = false;
              QString title;
              QString msg;
              if (err == QProcess::FailedToStart) {
                title = "PyXRF failed to start";
                msg = QString("The program \"%1\" failed to start.\n\n")
                        .arg(process->program()) +
                      "Set the \"PyXRF GUI Command\" field to the full path "
                      "of the PyXRF executable and try again.";
              } else {
                title = "PyXRF exited with an error";
                msg = process->readAllStandardError();
              }
              QMessageBox::critical(parent.data(), title, msg);
            });
  }

  bool validate(QString& reason)
  {
    auto workingDir = workingDirectory();

    if (workingDir.isEmpty() || !QDir(workingDir).exists()) {
      reason = "Data directory does not exist: " + workingDir;
      return false;
    }

    if (scanRange().isEmpty()) {
      reason = "Scan range is required.";
      return false;
    }

    // Make paths absolute
    if (!QFileInfo(parametersFile()).isAbsolute() &&
        !parametersFile().isEmpty()) {
      setParametersFile(QDir(workingDir).filePath(parametersFile()));
    }

    if (parametersFile().isEmpty() || !QFile::exists(parametersFile())) {
      reason = "Parameters file does not exist: " + parametersFile();
      return false;
    }

    auto cmd = command();
    if (!executableExists(cmd)) {
      reason = QString("The pyxrf-utils executable \"%1\" was not found.")
                 .arg(cmd.isEmpty() ? QString("(empty)") : cmd);
      return false;
    }

    return true;
  }

  // --- Settings ---

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");

    auto savedCommand = settings->value("pyxrfUtilsCommand", "").toString();
    setCommand(findPyxrfUtilsCommand(savedCommand));

    setWorkingDirectory(
      settings->value("workingDirectory", defaultWorkingDirectory())
        .toString());
    setScanRange(settings->value("scanRange", "").toString());
    setSkipDownloads(settings->value("skipDownloads", false).toBool());
    setRedownloadSuccessful(
      settings->value("redownloadSuccessful", false).toBool());
    ui.filterSidsString->setText(
      settings->value("filterSidsString", "").toString());

    settings->beginGroup("process");
    setPyxrfGUICommand(
      settings->value("pyxrfGUICommand", "pyxrf").toString());
    setParametersFile(settings->value("parametersFile", "").toString());
    setCsvOutput(settings->value("csvOutput", "").toString());
    setupComboBoxes();
    setIcName(settings->value("icName", "sclr1_ch4").toString());
    setSkipProcessed(settings->value("skipProcessed", true).toBool());
    setRotateDatasets(settings->value("rotateDatasets", true).toBool());
    settings->endGroup();

    settings->endGroup();

    populateScanTable();
    autoApplyFilter();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("pyxrf");

    settings->setValue("pyxrfUtilsCommand", command());
    settings->setValue("workingDirectory", workingDirectory());
    settings->setValue("scanRange", scanRange());
    settings->setValue("skipDownloads", skipDownloads());
    settings->setValue("redownloadSuccessful", redownloadSuccessful());
    settings->setValue("filterSidsString",
                       ui.filterSidsString->text().trimmed());

    settings->beginGroup("process");
    settings->setValue("pyxrfGUICommand", pyxrfGUICommand());
    settings->setValue("parametersFile", parametersFile());
    settings->setValue("csvOutput", csvOutput());
    settings->setValue("icName", icName());
    settings->setValue("skipProcessed", skipProcessed());
    settings->setValue("rotateDatasets", rotateDatasets());
    settings->endGroup();

    settings->endGroup();
  }

};

PyXRFWidget::PyXRFWidget(
  const QMap<QString, pipeline::PortData>& /*inputs*/, QWidget* p)
  : pipeline::CustomPythonNodeWidget(p), m_internal(new Internal(this))
{
}

PyXRFWidget::~PyXRFWidget() = default;

void PyXRFWidget::getValues(QMap<QString, QVariant>& map)
{
  map.insert("pyxrf_utils_command", m_internal->command());
  map.insert("working_directory", m_internal->workingDirectory());
  map.insert("scan_range", m_internal->scanRange());
  map.insert("skip_scan_ids", m_internal->skipScanIds());
  map.insert("skip_downloads", m_internal->skipDownloads());
  map.insert("redownload_successful", m_internal->redownloadSuccessful());
  map.insert("parameters_file", m_internal->parametersFile());
  map.insert("ic_name", m_internal->icName());
  map.insert("skip_processed", m_internal->skipProcessed());
  map.insert("rotate_datasets", m_internal->rotateDatasets());
  map.insert("csv_output", m_internal->csvOutput());

  QJsonObject uiState;
  uiState["filter_sids_string"] =
    m_internal->ui.filterSidsString->text().trimmed();
  map.insert("ui_state", QString::fromUtf8(
    QJsonDocument(uiState).toJson(QJsonDocument::Compact)));
}

void PyXRFWidget::setValues(const QMap<QString, QVariant>& map)
{
  auto wd = map.value("working_directory").toString();
  if (wd.isEmpty()) {
    m_internal->readSettings();
    return;
  }

  {
    QSignalBlocker b1(m_internal->ui.workingDirectory);
    QSignalBlocker b2(m_internal->ui.scanRange);

    m_internal->setCommand(
      map.value("pyxrf_utils_command", "pyxrf-utils").toString());
    m_internal->setWorkingDirectory(wd);
    m_internal->setScanRange(map.value("scan_range").toString());
    m_internal->setSkipDownloads(map.value("skip_downloads").toBool());
    m_internal->setRedownloadSuccessful(
      map.value("redownload_successful").toBool());
    m_internal->setParametersFile(map.value("parameters_file").toString());
    m_internal->setSkipProcessed(
      map.value("skip_processed", true).toBool());
    m_internal->setRotateDatasets(
      map.value("rotate_datasets", true).toBool());
    m_internal->setCsvOutput(map.value("csv_output").toString());
  }

  m_internal->populateScanTable();

  auto skipJson = map.value("skip_scan_ids", "[]").toString();
  auto skipArr = QJsonDocument::fromJson(skipJson.toUtf8()).array();
  QSet<int> skipIds;
  for (const auto& v : skipArr) {
    skipIds.insert(v.toInt());
  }
  for (auto& entry : m_internal->scanEntries) {
    if (skipIds.contains(entry.scanId)) {
      entry.use = false;
    }
  }
  m_internal->rebuildTableUI();

  m_internal->setupComboBoxes();
  m_internal->setIcName(map.value("ic_name", "sclr1_ch4").toString());

  auto uiStateJson = map.value("ui_state").toString();
  if (!uiStateJson.isEmpty()) {
    auto uiState = QJsonDocument::fromJson(uiStateJson.toUtf8()).object();
    m_internal->ui.filterSidsString->setText(
      uiState.value("filter_sids_string").toString());
  }
}

void PyXRFWidget::writeSettings()
{
  m_internal->writeSettings();
}

} // namespace tomviz
