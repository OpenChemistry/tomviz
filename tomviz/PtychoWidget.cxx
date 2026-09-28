/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PtychoWidget.h"
#include "ui_PtychoWidget.h"

#include "PythonUtilities.h"
#include "Utilities.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QComboBox>
#include <QBrush>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextStream>
#include <QtConcurrent>

#include <atomic>
#include <functional>
#include <memory>

namespace tomviz {

// Emits results from the background ptycho directory scan. The worker
// thread emits; the widget receives via queued connections.
class PtychoScanCall : public QObject
{
  Q_OBJECT

signals:
  void listed(int total);
  void sidScanned(qlonglong sid, QStringList versions, QVariantList angles,
                  QStringList errors, int index, int total);
  void finished(bool ok);
};

class PtychoWidget::Internal : public QObject
{
  Q_OBJECT

public:
  Ui::PtychoWidget ui;
  QPointer<PtychoWidget> parent;

  bool ptychoguiIsRunning = false;

  // Key is SID
  QMap<long, QStringList> versionOptions;
  // First key for these is the SID. Second key is the version.
  QMap<long, QMap<QString, double>> angleOptions;
  QMap<long, QMap<QString, QString>> allErrorLists;

  QList<long> sidList;
  QList<double> angleList;
  QStringList versionList;
  QList<bool> useList;
  QStringList errorReasonList;

  QList<long> filteredSidList;

  QMap<int, QString> tableColumns;

  Python::Module ptychoModule;

  // Background directory scan state. The generation counter invalidates
  // results from a superseded scan; the flag cancels its worker loop.
  std::shared_ptr<std::atomic<bool>> scanCancelFlag;
  int scanGeneration = 0;
  bool scanInProgress = false;
  // Runs after the next scan that completes; carried across a
  // superseded scan so a pending selection restore is not lost.
  std::function<void()> pendingScanDone;

  Internal(PtychoWidget* p)
    : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    ui.scanProgressBar->hide();

    importModule();

    setupTable();
    setupConnections();
  }

  ~Internal()
  {
    if (scanCancelFlag) {
      scanCancelFlag->store(true);
    }
  }

signals:
  void scanFinished();

public:
  void setupConnections()
  {
    connect(ui.startPtychoGUI, &QPushButton::clicked, this,
            &Internal::startPtychoGUI);

    connect(ui.ptychoDirectory, &QLineEdit::editingFinished,
            this, &Internal::ptychoDirEdited);
    connect(ui.selectPtychoDirectory, &QPushButton::clicked, this,
            &Internal::selectPtychoDirectory);

    connect(ui.loadFromCSVFile, &QLineEdit::editingFinished,
            this, &Internal::setUseAndVersionsFromCSV);
    connect(ui.selectLoadFromCSVFile, &QPushButton::clicked, this,
            &Internal::selectLoadFromCSV);

    connect(ui.filterSIDsString, &QLineEdit::editingFinished,
            this, &Internal::updateFilteredSidList);
    connect(ui.loadSidsFromTxt, &QPushButton::clicked, this,
            &Internal::onLoadSidsFromTxtClicked);

    // Write the table out without running the operator, so a scan list
    // can be prepared up front and shared with the PyXRF workflow.
    auto* saveScanListButton = new QPushButton("Save Scan List...", parent);
    saveScanListButton->setToolTip(
      "Save the listed scans as a CSV (Scan ID, Theta, Use, Version) that "
      "this dialog and the PyXRF dialog can load back in.");
    ui.horizontalLayout->addWidget(saveScanListButton);
    connect(saveScanListButton, &QPushButton::clicked, this,
            &Internal::saveScanList);

    connect(ui.selectOutputInfoFile, &QPushButton::clicked, this,
            &Internal::selectOutputInfoFile);
  }

  void setupTable()
  {
    auto* table = ui.table;
    auto& columns = tableColumns;

    columns.clear();
    columns[0] = "SID";
    columns[1] = "Angle";
    columns[2] = "Version";
    columns[3] = "Use";
    columns[4] = "Error Reason";

    table->setColumnCount(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
      auto* header = new QTableWidgetItem(columns[i]);
      table->setHorizontalHeaderItem(i, header);
    }

    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QWidget::customContextMenuRequested, this,
            &Internal::showTableContextMenu);
  }

  void showTableContextMenu(const QPoint& pos)
  {
    auto* table = ui.table;
    auto selectedRows = table->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
      return;
    }

    QSet<QString> commonVersions;
    bool first = true;
    for (auto& index : selectedRows) {
      int row = index.row();
      auto sid = filteredSidList[row];
      QSet<QString> versions(versionOptions[sid].begin(),
                             versionOptions[sid].end());
      if (first) {
        commonVersions = versions;
        first = false;
      } else {
        commonVersions &= versions;
      }
    }

    QMenu menu(table);
    auto* setVersionAction = menu.addAction("Set Version...");
    if (commonVersions.isEmpty()) {
      setVersionAction->setEnabled(false);
      setVersionAction->setText("Set Version... (no shared versions)");
    }
    auto* chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (chosen != setVersionAction) {
      return;
    }

    QStringList sortedVersions = commonVersions.values();
    sortedVersions.sort();

    int defaultIndex = 0;
    QSet<QString> currentVersions;
    for (auto& index : selectedRows) {
      int row = index.row();
      auto sid = filteredSidList[row];
      auto idx = sidList.indexOf(sid);
      currentVersions.insert(versionList[idx]);
    }
    if (currentVersions.size() == 1) {
      int idx = sortedVersions.indexOf(*currentVersions.begin());
      if (idx >= 0) {
        defaultIndex = idx;
      }
    }

    bool ok = false;
    auto version = QInputDialog::getItem(
      parent, "Set Version", "Version:", sortedVersions, defaultIndex,
      false, &ok);
    if (!ok) {
      return;
    }

    for (auto& index : selectedRows) {
      int row = index.row();
      auto sid = filteredSidList[row];
      auto idx = sidList.indexOf(sid);
      versionList[idx] = version;
    }

    onSelectedVersionsChanged();
    updateTable();
  }

  void importModule()
  {
    Python python;

    if (ptychoModule.isValid()) {
      return;
    }

    ptychoModule = python.import("tomviz.ptycho");
    if (!ptychoModule.isValid()) {
      qCritical() << "Failed to import \"tomviz.ptycho\" module";
    }
  }

  QList<long> selectedSids()
  {
    // Only include the filtered ones
    QList<long> ret;
    for (auto& sid : filteredSidList) {
      auto idx = sidList.indexOf(sid);
      auto use = useList[idx];
      if (use) {
        ret.append(sid);
      }
    }

    return ret;
  }

  QStringList selectedVersions()
  {
    QStringList versions;
    for (auto sid : selectedSids()) {
      auto idx = sidList.indexOf(sid);
      versions.append(versionList[idx]);
    }
    return versions;
  }

  QList<double> selectedAngles()
  {
    QList<double> angles;
    for (auto sid : selectedSids()) {
      auto idx = sidList.indexOf(sid);
      angles.append(angleList[idx]);
    }
    return angles;
  }

  void updateTable()
  {
    auto* table = ui.table;

    int scrollbarPosition = 0;
    auto scrollbar = table->verticalScrollBar();
    if (scrollbar) {
      scrollbarPosition = scrollbar->value();
    }

    table->clearContents();

    table->setRowCount(filteredSidList.size());
    for (int i = 0; i < filteredSidList.size(); ++i) {
      fillTableRow(i, filteredSidList[i]);
    }

    if (scrollbar) {
      scrollbar->setValue(scrollbarPosition);
    }
  }

  void fillTableRow(int i, long sid)
  {
    auto* table = ui.table;

    bool invalid = false;
    for (auto j : tableColumns.keys()) {
      auto column = tableColumns[j];
      auto value = tableValue(sid, column);
      if (column == "Version") {
        auto* cb = createVersionComboBox(sid, value);
        table->setCellWidget(i, j, cb);
        continue;
      } else if (column == "Use") {
        auto* w = createUseCheckBox(sid, value);
        table->setCellWidget(i, j, w);
        continue;
      } else if (column == "Error Reason") {
        invalid = !value.isEmpty();
      }

      auto* item = new QTableWidgetItem(value);
      item->setTextAlignment(Qt::AlignCenter);
      table->setItem(i, j, item);
    }

    if (invalid) {
      // Make every item have a red background
      for (int j = 0; j < tableColumns.size(); ++j) {
        auto* item = table->item(i, j);
        if (item) {
          item->setBackground(QBrush(Qt::red));
        } else {
          auto* cw = table->cellWidget(i, j);
          if (cw) {
            cw->setStyleSheet("background-color: red");
          }
        }
      }
    }
  }

  QWidget* createVersionComboBox(long sid, QString value)
  {
    if (versionOptions[sid].size() < 2) {
      // If there aren't any options, the item will just be a label
      QString text = "None";
      if (versionOptions[sid].size() == 1) {
        text = versionOptions[sid][0];
      }
      return createTableWidget(new QLabel(text, parent));
    }

    auto cb = new QComboBox(parent);
    for (auto& option: versionOptions[sid]) {
      cb->addItem(option);
    }
    cb->setCurrentText(value);

    connect(cb, &QComboBox::currentIndexChanged, this, [this, sid, cb]() {
      auto idx = sidList.indexOf(sid);
      versionList[idx] = cb->currentText();
      onSelectedVersionsChanged();
      // Update the table, because the angle and error reason likely changed
      updateTable();
    });

    return createTableWidget(cb);
  }

  QWidget* createUseCheckBox(long sid, QString value)
  {
    auto cb = new QCheckBox(parent);
    cb->setChecked(value == "x" || value == "1");
    connect(cb, &QCheckBox::toggled, this, [this, sid](bool b) {
      auto idx = sidList.indexOf(sid);
      useList[idx] = b;
    });

    return createTableWidget(cb);
  }

  QWidget* createTableWidget(QWidget* w)
  {
    // This is required to center the widget
    auto* tw = new QWidget(ui.table);
    auto* layout = new QHBoxLayout(tw);
    layout->addWidget(w);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return tw;
  }

  QString tableValue(long sid, QString column)
  {
    auto idx = sidList.indexOf(sid);
    if (column == "SID") {
      return QString::number(sidList[idx]);
    } else if (column == "Version") {
      return versionList[idx];
    } else if (column == "Angle") {
      return QString::number(angleList[idx]);
    } else if (column == "Use") {
      return useList[idx] ? "x" : "";
    } else if (column == "Error Reason") {
      return errorReasonList[idx];
    }

    qCritical() << "Unknown table column: " << column;
    return "";
  }

  QString defaultOutputInfoFile() { return ""; }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ptycho");
    settings->beginGroup("process");

    setPtychoGUICommand(
      settings->value("ptychoGUICommand", "run-ptycho").toString());

    auto savedPtychoDir = settings->value("ptychoDirectory", "").toString();
    if (!savedPtychoDir.isEmpty() && !QDir(savedPtychoDir).exists()) {
      savedPtychoDir = "";
    }
    setPtychoDirectory(savedPtychoDir);
    setCsvFile(settings->value("loadFromCSVFile", "").toString());
    setFilterSIDsString(settings->value("filterSIDsString", "").toString());

    setOutputInfoFile(
      settings->value("outputInfoFile", defaultOutputInfoFile()).toString());
    setRotateDatasets(
      settings->value("rotateDatasets", true).toBool());

    QVariantList sidListV = settings->value("sidListV").toList();
    QVariantList versionListV = settings->value("versionListV").toList();
    QVariantList useListV = settings->value("useListV").toList();

    QList<long> savedSidList;
    for (const auto& var : sidListV) {
      savedSidList.append(var.value<long>());
    }

    QStringList savedVersionList;
    for (const auto& var : versionListV) {
      savedVersionList.append(var.value<QString>());
    }

    QList<bool> savedUseList;
    for (const auto& var : useListV) {
      savedUseList.append(var.value<bool>());
    }

    settings->endGroup();
    settings->endGroup();

    if (!ptychoDirectory().isEmpty()) {
      // Trigger a load; the rest applies after the scan completes
      loadPtychoDir([this, savedSidList, savedVersionList, savedUseList]() {
        if (!csvFile().isEmpty()) {
          // Trigger applying the CSV file
          setUseAndVersionsFromCSV();
        }

        if (!filterSIDsString().isEmpty()) {
          // Trigger an update via the filters
          updateFilteredSidList();
        }

        if (savedSidList == sidList) {
          // If the saved SID list matches, we can also load the settings
          // for "use" and "version"
          versionList = savedVersionList;
          useList = savedUseList;
          onSelectedVersionsChanged();
          updateTable();
        }
      });
    }
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ptycho");
    settings->beginGroup("process");

    // Save general settings
    settings->setValue("ptychoGUICommand", ptychoGUICommand());
    settings->setValue("ptychoDirectory", ptychoDirectory());
    settings->setValue("loadFromCSVFile", csvFile());

    settings->setValue("filterSIDsString", filterSIDsString());

    settings->setValue("outputInfoFile", outputInfoFile());
    settings->setValue("rotateDatasets", rotateDatasets());

    // Save out our lists
    QVariantList sidListV;
    for (auto v: sidList) {
      sidListV.append(QVariant::fromValue(v));
    }

    QVariantList versionListV;
    for (auto v: versionList) {
      versionListV.append(QVariant::fromValue(v));
    }

    QVariantList useListV;
    for (auto b: useList) {
      useListV.append(QVariant::fromValue(b));
    }

    settings->setValue("sidListV", sidListV);
    settings->setValue("versionListV", versionListV);
    settings->setValue("useListV", useListV);

    settings->endGroup();
    settings->endGroup();
  }

  void startPtychoGUI()
  {
    if (ptychoguiIsRunning) {
      // It's already running. Just return.
      return;
    }

    QString program = ptychoGUICommand();
    QStringList args;

    auto* process = new QProcess(this);

    auto processEnv = QProcessEnvironment::systemEnvironment();

    // Remove variables related to python environment
    processEnv.remove("PYTHONHOME");
    processEnv.remove("PYTHONPATH");

    process->setProcessEnvironment(processEnv);

    // Forward stdout/stderr to this process
    process->setProcessChannelMode(QProcess::ForwardedChannels);

    process->start(program, args);

    ptychoguiIsRunning = true;

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this]() {
      ptychoguiIsRunning = false;
      loadPtychoDir();
    });

    connect(
      process, &QProcess::errorOccurred, this,
      [this, process](QProcess::ProcessError err) {
        ptychoguiIsRunning = false;

        QString title;
        QString msg;

        if (err == QProcess::FailedToStart) {
          title = "Ptycho GUI failed to start";
          msg = QString("The program \"%1\" failed to start.\n\n")
                  .arg(process->program());
        } else {
          QString output = process->readAllStandardOutput();
          QString error = process->readAllStandardError();
          title = "Ptycho GUI exited with an error";
          msg =
            QString("stdout: \"%1\"\n\nstderr: \"%2\"").arg(output).arg(error);
        }
        QMessageBox::critical(parent.data(), title, msg);
      });
  }

  void selectPtychoDirectory()
  {
    QString caption = "Select Ptycho GUI Directory";
    auto file =
      QFileDialog::getExistingDirectory(parent.data(), caption,
                                        ptychoDirectory());
    if (file.isEmpty()) {
      return;
    }

    // If "recon_result" exists underneath the selected directory,
    // it means the parent directory was selected.
    // We should automatically select the child one.
    auto possibleChildPath = QDir(file).filePath("recon_result");
    if (QFile::exists(possibleChildPath)) {
      file = possibleChildPath;
    }

    setPtychoDirectory(file);
    ptychoDirEdited();
  }

  void ptychoDirEdited()
  {
    auto dir = ptychoDirectory();
    if (!dir.isEmpty() && !QDir(dir).exists()) {
      QMessageBox::critical(parent.data(), "Directory Not Found",
                            "Ptycho directory does not exist: " + dir);
      setPtychoDirectory("");
      setCsvFile("");
      setFilterSIDsString("");
      return;
    }

    // Whenever this is called, make sure we clear the CSV file and SID filters
    setCsvFile("");
    setFilterSIDsString("");

    loadPtychoDir();
  }

  void cancelScan()
  {
    if (scanCancelFlag) {
      scanCancelFlag->store(true);
    }
    ++scanGeneration;
    if (scanInProgress) {
      scanInProgress = false;
      ui.scanProgressBar->hide();
      emit scanFinished();
    }
  }

  // Blocks (event loop running) until no scan is in progress, so an
  // Apply during a scan waits for complete results.
  void waitForScan()
  {
    // The dialog refuses to close during an apply; if this object is
    // destroyed under the nested loop anyway, quit rather than spin.
    QPointer<Internal> self(this);
    while (self && self->scanInProgress) {
      QEventLoop loop;
      connect(this, &Internal::scanFinished, &loop, &QEventLoop::quit);
      connect(this, &QObject::destroyed, &loop, &QEventLoop::quit);
      loop.exec();
    }
  }

  // Scans the ptycho directory in a background thread, streaming SIDs
  // into the table as they are found. onDone runs after the next scan
  // that completes, carrying over if this scan is superseded.
  void loadPtychoDir(std::function<void()> onDone = {})
  {
    cancelScan();
    clearTable();
    updateTable();

    if (onDone) {
      pendingScanDone = std::move(onDone);
    }

    scanInProgress = true;
    scanCancelFlag = std::make_shared<std::atomic<bool>>(false);

    int gen = scanGeneration;
    auto cancel = scanCancelFlag;
    auto dir = ptychoDirectory();

    // Busy indicator until the SID list arrives
    ui.scanProgressBar->setRange(0, 0);
    ui.scanProgressBar->show();

    auto* call = new PtychoScanCall;
    connect(call, &PtychoScanCall::listed, this, [this, gen](int total) {
      if (gen != scanGeneration) {
        return;
      }
      ui.scanProgressBar->setRange(0, total);
      ui.scanProgressBar->setValue(0);
    });
    connect(call, &PtychoScanCall::sidScanned, this,
            [this, gen](qlonglong sid, QStringList versions,
                        QVariantList angles, QStringList errors, int index,
                        int total) {
      Q_UNUSED(total)
      if (gen != scanGeneration) {
        return;
      }
      appendScannedSid(static_cast<long>(sid), versions, angles, errors);
      ui.scanProgressBar->setValue(index + 1);
    });
    connect(call, &PtychoScanCall::finished, this,
            [this, gen](bool ok) {
      if (gen != scanGeneration) {
        return;
      }
      scanInProgress = false;
      ui.scanProgressBar->hide();
      if (!ok) {
        qCritical() << "The ptycho directory scan failed";
      }
      updateFilteredSidList();
      auto done = std::move(pendingScanDone);
      pendingScanDone = nullptr;
      if (done) {
        done();
      }
      emit scanFinished();
    });
    connect(call, &PtychoScanCall::finished, call, &QObject::deleteLater);

    auto future = QtConcurrent::run([call, cancel, dir]() {
      QList<qlonglong> sids;
      Python::Module module;
      {
        Python python;
        module = python.import("tomviz.ptycho");
        Python::Function listFunc;
        if (module.isValid()) {
          listFunc = module.findFunction("list_ptycho_sids");
        }
        if (!listFunc.isValid()) {
          qCritical()
            << "Failed to find \"tomviz.ptycho.list_ptycho_sids\"";
          emit call->finished(false);
          return;
        }

        Python::Dict kwargs;
        kwargs.set("ptycho_dir", dir);
        auto res = listFunc.call(kwargs);
        if (!res.isValid() || !res.isList()) {
          qCritical() << "Error calling \"tomviz.ptycho.list_ptycho_sids\"";
          emit call->finished(false);
          return;
        }

        auto resList = res.toList();
        for (int i = 0; i < resList.length(); ++i) {
          sids.append(resList[i].toLong());
        }
      }

      int total = sids.size();
      emit call->listed(total);

      for (int i = 0; i < total; ++i) {
        if (cancel->load()) {
          emit call->finished(false);
          return;
        }

        auto sid = sids[i];
        QStringList versions, errors;
        QVariantList angles;
        {
          // Scoped so the GIL is released between SIDs
          Python python;
          auto func = module.findFunction("gather_sid_info");
          if (!func.isValid()) {
            qCritical() << "Failed to find \"tomviz.ptycho.gather_sid_info\"";
            emit call->finished(false);
            return;
          }

          Python::Dict kwargs;
          kwargs.set("ptycho_dir", dir);
          kwargs.set("sid", Variant(static_cast<long>(sid)));
          auto res = func.call(kwargs);
          if (!res.isValid() || !res.isDict()) {
            // Skip this SID, but keep scanning the rest
            qCritical() << "Error gathering ptycho info for SID" << sid;
            continue;
          }

          auto resDict = res.toDict();
          for (auto& v : resDict["versions"].toVariant().toList()) {
            versions.append(QString::fromStdString(v.toString()));
          }
          for (auto& a : resDict["angles"].toVariant().toList()) {
            angles.append(a.toDouble());
          }
          for (auto& e : resDict["errors"].toVariant().toList()) {
            errors.append(QString::fromStdString(e.toString()));
          }
        }
        emit call->sidScanned(sid, versions, angles, errors, i, total);
      }

      emit call->finished(!cancel->load());
    });
    Q_UNUSED(future)
  }

  // Record one scanned SID with its default selection (first valid
  // version, used) and stream its row in when no SID filter is active.
  void appendScannedSid(long sid, const QStringList& versions,
                        const QVariantList& angles, const QStringList& errors)
  {
    if (sidList.contains(sid)) {
      return;
    }

    QMap<QString, double> angleMap;
    QMap<QString, QString> errorMap;
    for (int j = 0; j < versions.size(); ++j) {
      angleMap[versions[j]] = angles.value(j).toDouble();
      errorMap[versions[j]] = errors.value(j);
    }

    sidList.append(sid);
    versionOptions[sid] = versions;
    angleOptions[sid] = angleMap;
    allErrorLists[sid] = errorMap;

    QString chosen = versions.isEmpty() ? QString("t1") : versions[0];
    bool use = false;
    for (auto& version : versions) {
      if (errorMap[version].isEmpty()) {
        // This one is valid.
        chosen = version;
        use = true;
        break;
      }
    }
    versionList.append(chosen);
    useList.append(use);
    angleList.append(angleMap[chosen]);
    errorReasonList.append(errorMap[chosen]);

    if (filterSIDsString().isEmpty()) {
      filteredSidList.append(sid);
      int row = ui.table->rowCount();
      ui.table->setRowCount(row + 1);
      fillTableRow(row, sid);
    }
  }

  void onSelectedVersionsChanged()
  {
    angleList.clear();
    errorReasonList.clear();

    for (int i = 0; i < sidList.size(); ++i) {
      auto sid = sidList[i];
      auto version = versionList[i];
      angleList.append(angleOptions[sid][version]);
      errorReasonList.append(allErrorLists[sid][version]);
    }
  }

  void updateFilteredSidList()
  {
    auto filterString = filterSIDsString();

    Python python;

    auto func = ptychoModule.findFunction("filter_sid_list");
    if (!func.isValid()) {
      qCritical() << "Failed to find function \"filter_sid_list\"";
      return;
    }

    Python::Dict kwargs;
    kwargs.set("sid_list", sidList);
    kwargs.set("filter_string", filterString);
    auto result = func.call(kwargs);
    if (!result.isValid() || !result.isList()) {
      qCritical() << "Failed to call function \"filter_sid_list\"";
      return;
    }

    filteredSidList.clear();
    auto resultList = result.toList();
    for (int i = 0; i < resultList.length(); ++i) {
      filteredSidList.append(resultList[i].toLong());
    }

    updateTable();
  }

  void onLoadSidsFromTxtClicked()
  {
    QString caption = "Select txt file";
    QString filter = "*.txt";
    auto startPath = ptychoDirectory();
    auto filePath =
      QFileDialog::getOpenFileName(parent.data(), caption, startPath, filter);

    if (filePath.isEmpty()) {
      return;
    }

    QFile file(filePath);
    if (!file.exists()) {
      qCritical() << QString("Txt file does not exist: %1").arg(filePath);
      return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
      qCritical()
        << QString("Failed to open file \"%1\" with error: ").arg(filePath)
        << file.errorString();
      return;
    }

    QTextStream reader(&file);
    auto sids = readSidsFromText(reader);
    ui.filterSIDsString->setText(sids.join(", "));

    updateFilteredSidList();
  }

  void selectLoadFromCSV()
  {
    QString caption = "Select CSV file to load Use and Version settings";
    auto startPath = !csvFile().isEmpty() ? csvFile() : ptychoDirectory();
    auto file =
      QFileDialog::getOpenFileName(parent.data(), caption, startPath);
    if (file.isEmpty()) {
      return;
    }
    ui.loadFromCSVFile->setText(file);

    setUseAndVersionsFromCSV();
  }

  void setUseAndVersionsFromCSV()
  {
    // Apply against the complete SID list, not a partial scan
    waitForScan();

    Python python;

    auto func = ptychoModule.findFunction("get_use_and_versions_from_csv");
    if (!func.isValid()) {
      qCritical() << "Failed to find function \"get_use_and_versions_from_csv\"";
      return;
    }

    Python::Dict kwargs;
    kwargs.set("csv_path", csvFile());
    auto result = func.call(kwargs);
    if (!result.isValid() || !result.isDict()) {
      qCritical() << "Failed to call function \"get_use_and_versions_from_csv\"";
      return;
    }

    auto resultDict = result.toDict();

    QList<long> sids;
    QList<bool> use;
    QStringList versions;

    auto sidsPy = resultDict["sids"].toList();
    auto usePy = resultDict["use"].toList();
    auto versionsPy = resultDict["versions"].toList();

    for (auto i = 0; i < sidsPy.length(); ++i) {
      sids.append(sidsPy[i].toLong());
      if (i < usePy.length()) {
        use.append(usePy[i].toBool());
      }
      if (i < versionsPy.length()) {
        versions.append(versionsPy[i].toString());
      }
    }

    if (sids.size() == 0) {
      qCritical() << "No SIDs found in CSV file. Aborting";
      return;
    }

    if (use.size() != 0) {
      // Set the "Use" for every current one to "false";
      for (int i = 0; i < useList.size(); ++i) {
        useList[i] = false;
      }
    }

    QList<long> missing;
    for (int i = 0; i < sids.size(); ++i) {
      auto sid = sids[i];
      auto idx = sidList.indexOf(sid);
      if (idx < 0) {
        // The file lists a SID that is not in the ptycho directory
        missing.append(sid);
        continue;
      }
      if (i < use.size()) {
        useList[idx] = use[i];
      }

      if (i < versions.size()) {
        // Verify it is a valid version
        auto newVersion = versions[i];
        if (!versionOptions[sid].contains(newVersion)) {
          qCritical() << "SID \"" << sid << "\" from CSV file "
                      << "indicated a version of " << newVersion << ", "
                      << "but that did not match the available versions "
                      << "found within the ptycho directory for that SID. "
                      << "Skipping...";
        } else {
          versionList[idx] = newVersion;
        }
      }
    }

    if (!missing.isEmpty()) {
      qWarning() << missing.size() << "SIDs from the file are not in the"
                 << "ptycho directory and were skipped:" << missing;
    }

    updateTable();
  }

  void saveScanList()
  {
    if (filteredSidList.isEmpty()) {
      QMessageBox::information(parent.data(), "Save Scan List",
                               "There are no scans to save.");
      return;
    }
    auto startPath =
      outputInfoFile().isEmpty() ? ptychoDirectory() : outputInfoFile();
    auto path = QFileDialog::getSaveFileName(parent.data(), "Save scan list",
                                             startPath, "CSV files (*.csv)");
    if (path.isEmpty()) {
      return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::warning(parent.data(), "Save Scan List",
                           QString("Could not write %1").arg(path));
      return;
    }
    // Same columns as the CSV the operator writes for its output info file
    QTextStream out(&file);
    out << "Scan ID,Theta,Use,Version\n";
    for (auto sid : filteredSidList) {
      int idx = sidList.indexOf(sid);
      if (idx < 0) {
        continue;
      }
      out << sid << ',' << QString::number(angleList[idx], 'f', 3) << ','
          << (useList[idx] ? 1 : 0) << ',' << versionList[idx] << '\n';
    }
  }

  void selectOutputInfoFile()
  {
    QString caption = "Select output info file";
    auto startPath = outputInfoFile();
    if (startPath.isEmpty()) {
      startPath = ptychoDirectory();
    }
    auto file = QFileDialog::getSaveFileName(
      parent.data(), caption, startPath,
      "CSV files (*.csv);;Text files (*.txt)");
    if (file.isEmpty()) {
      return;
    }

    setOutputInfoFile(file);
  }

  void clearTable()
  {
    versionOptions.clear();
    angleOptions.clear();
    allErrorLists.clear();

    sidList.clear();
    angleList.clear();
    versionList.clear();
    useList.clear();
    errorReasonList.clear();

    filteredSidList.clear();
  }

  QString ptychoGUICommand() const { return ui.ptychoGUICommand->text(); }

  void setPtychoGUICommand(QString s) { ui.ptychoGUICommand->setText(s); }

  QString ptychoDirectory() const { return ui.ptychoDirectory->text(); }

  void setPtychoDirectory(QString s) { ui.ptychoDirectory->setText(s); }

  QString csvFile() const { return ui.loadFromCSVFile->text(); }

  void setCsvFile(QString s) { ui.loadFromCSVFile->setText(s); }

  QString filterSIDsString() const { return ui.filterSIDsString->text().trimmed(); }

  void setFilterSIDsString(QString s) { ui.filterSIDsString->setText(s); }

  QString outputInfoFile() const { return ui.outputInfoFile->text(); }

  void setOutputInfoFile(QString s) { ui.outputInfoFile->setText(s); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }

  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }
};

PtychoWidget::PtychoWidget(
  const QMap<QString, pipeline::PortData>& /*inputs*/, QWidget* p)
  : pipeline::CustomPythonNodeWidget(p), m_internal(new Internal(this))
{
}

PtychoWidget::~PtychoWidget() = default;

void PtychoWidget::getValues(QMap<QString, QVariant>& map)
{
  // If a directory scan is still running, wait for its results so we
  // never hand back a partially-populated selection.
  m_internal->waitForScan();

  auto sids = m_internal->selectedSids();
  auto versions = m_internal->selectedVersions();
  auto angles = m_internal->selectedAngles();

  QJsonArray sidArray, versionArray, angleArray;
  for (auto sid : sids) {
    sidArray.append(static_cast<qint64>(sid));
  }
  for (const auto& v : versions) {
    versionArray.append(v);
  }
  for (auto angle : angles) {
    angleArray.append(angle);
  }

  map.insert("ptycho_dir", m_internal->ptychoDirectory());
  map.insert("output_info_file", m_internal->outputInfoFile());
  map.insert("rotate_datasets", m_internal->rotateDatasets());
  map.insert("sid_list", QString::fromUtf8(
    QJsonDocument(sidArray).toJson(QJsonDocument::Compact)));
  map.insert("version_list", QString::fromUtf8(
    QJsonDocument(versionArray).toJson(QJsonDocument::Compact)));
  map.insert("angle_list", QString::fromUtf8(
    QJsonDocument(angleArray).toJson(QJsonDocument::Compact)));

  QJsonObject uiState;
  uiState["filter_sids_string"] = m_internal->filterSIDsString();
  uiState["csv_file"] = m_internal->csvFile();

  QJsonArray fullSidList, fullVersionList, fullUseList;
  for (auto sid : m_internal->sidList) {
    fullSidList.append(static_cast<qint64>(sid));
  }
  for (const auto& v : m_internal->versionList) {
    fullVersionList.append(v);
  }
  for (auto u : m_internal->useList) {
    fullUseList.append(u);
  }
  uiState["full_sid_list"] = fullSidList;
  uiState["full_version_list"] = fullVersionList;
  uiState["full_use_list"] = fullUseList;

  map.insert("ui_state", QString::fromUtf8(
    QJsonDocument(uiState).toJson(QJsonDocument::Compact)));
}

void PtychoWidget::setValues(const QMap<QString, QVariant>& map)
{
  auto dir = map.value("ptycho_dir").toString();
  if (dir.isEmpty()) {
    m_internal->readSettings();
    return;
  }

  {
    QSignalBlocker b1(m_internal->ui.ptychoDirectory);
    m_internal->setPtychoDirectory(dir);
    m_internal->setOutputInfoFile(
      map.value("output_info_file").toString());
    m_internal->setRotateDatasets(
      map.value("rotate_datasets", true).toBool());
  }

  // Restore the selections after the directory scan completes
  m_internal->loadPtychoDir([this, map]() {
    auto uiStateJson = map.value("ui_state").toString();
    if (!uiStateJson.isEmpty()) {
      auto uiState = QJsonDocument::fromJson(uiStateJson.toUtf8()).object();
      m_internal->setCsvFile(uiState.value("csv_file").toString());
      m_internal->setFilterSIDsString(
        uiState.value("filter_sids_string").toString());

      auto fullSidArr = uiState.value("full_sid_list").toArray();
      auto fullVersionArr = uiState.value("full_version_list").toArray();
      auto fullUseArr = uiState.value("full_use_list").toArray();

      QList<long> savedSidList;
      for (const auto& v : fullSidArr) {
        savedSidList.append(static_cast<long>(v.toInteger()));
      }
      QStringList savedVersionList;
      for (const auto& v : fullVersionArr) {
        savedVersionList.append(v.toString());
      }
      QList<bool> savedUseList;
      for (const auto& v : fullUseArr) {
        savedUseList.append(v.toBool());
      }

      if (savedSidList == m_internal->sidList) {
        m_internal->versionList = savedVersionList;
        m_internal->useList = savedUseList;
        m_internal->onSelectedVersionsChanged();
      } else if (!m_internal->csvFile().isEmpty()) {
        m_internal->setUseAndVersionsFromCSV();
      }

      m_internal->updateFilteredSidList();
    } else {
      // No ui_state -- use sid_list/version_list to restore selections
      auto sidJson = map.value("sid_list", "[]").toString();
      auto versionJson = map.value("version_list", "[]").toString();
      auto sidArr = QJsonDocument::fromJson(sidJson.toUtf8()).array();
      auto verArr = QJsonDocument::fromJson(versionJson.toUtf8()).array();

      QSet<long> selectedSids;
      QMap<long, QString> selectedVersions;
      for (int i = 0; i < sidArr.size(); ++i) {
        long sid = static_cast<long>(sidArr[i].toInteger());
        selectedSids.insert(sid);
        if (i < verArr.size()) {
          selectedVersions[sid] = verArr[i].toString();
        }
      }

      for (int i = 0; i < m_internal->sidList.size(); ++i) {
        auto sid = m_internal->sidList[i];
        m_internal->useList[i] = selectedSids.contains(sid);
        if (selectedVersions.contains(sid)) {
          m_internal->versionList[i] = selectedVersions[sid];
        }
      }
      m_internal->onSelectedVersionsChanged();
      m_internal->updateFilteredSidList();
    }
  });
}

void PtychoWidget::writeSettings()
{
  m_internal->writeSettings();
}

} // namespace tomviz

#include "PtychoWidget.moc"
