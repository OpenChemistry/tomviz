/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PtychoDialog.h"
#include "ui_PtychoDialog.h"

#include "PythonUtilities.h"
#include "Utilities.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QComboBox>
#include <QBrush>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScrollBar>

namespace tomviz {

class PtychoDialog::Internal : public QObject
{
  Q_OBJECT

public:
  Ui::PtychoDialog ui;
  QPointer<PtychoDialog> parent;

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

  Internal(PtychoDialog* p)
    : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    importModule();

    setupTable();
    setupConnections();
  }

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
    connect(ui.selectOutputDirectory, &QPushButton::clicked, this,
            &Internal::selectOutputDirectory);

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
            &Internal::accepted);
    connect(ui.buttonBox, &QDialogButtonBox::helpRequested, this,
            [](){ openHelpUrl("https://tomviz.readthedocs.io/en/latest/workflows_ptycho.html"); });
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

  void accepted()
  {
    QString reason;
    if (!validate(reason)) {
      QString title = "Invalid Settings";
      QMessageBox::critical(parent.data(), title, reason);
      parent->show();
      return;
    }

    writeSettings();
    parent->accept();
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

  QList<long> invalidSidsSelected()
  {
    QList<long> invalid;
    for (auto sid : selectedSids()) {
      auto idx = sidList.indexOf(sid);
      if (!errorReasonList[idx].isEmpty()) {
        invalid.append(sid);
      }
    }
    return invalid;
  }

  bool validate(QString& reason)
  {
    // Validate settings
    if (ptychoDirectory().isEmpty() || !QDir(ptychoDirectory()).exists()) {
      reason = "Ptycho directory does not exist: " + ptychoDirectory();
      return false;
    }

    if (sidList.isEmpty()) {
      reason = "No SIDs found in ptycho directory: " + ptychoDirectory();
      return false;
    }

    auto invalid = invalidSidsSelected();
    if (!invalid.isEmpty()) {
      QString title = "Invalid SID and version combinations selected";
      QString text = "Invalid SIDs were selected. ";
      text += "Do you wish to automatically deselect them and continue?";
      if (QMessageBox::question(parent, title, text) == QMessageBox::No) {
        reason = "Invalid SIDs were selected";
        return false;
      }

      for (auto sid : invalid) {
        auto idx = sidList.indexOf(sid);
        useList[idx] = false;
        updateTable();
      }
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

    return true;
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
      auto sid = filteredSidList[i];
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

    if (scrollbar) {
      scrollbar->setValue(scrollbarPosition);
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

  QString defaultOutputDirectory() { return QDir::home().filePath("ptycho_output"); }

  void readSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ptycho");
    settings->beginGroup("process");

    setPtychoGUICommand(
      settings->value("ptychoGUICommand", "run-ptycho").toString());

    setPtychoDirectory(settings->value("ptychoDirectory", "").toString());
    setCsvFile(settings->value("loadFromCSVFile", "").toString());
    setFilterSIDsString(settings->value("filterSIDsString", "").toString());

    setOutputDirectory(
      settings->value("outputDirectory", defaultOutputDirectory()).toString());
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
      // Trigger a load
      loadPtychoDir();

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

    settings->setValue("outputDirectory", outputDirectory());
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

    setPtychoDirectory(file);
    ptychoDirEdited();
  }

  void ptychoDirEdited()
  {
    // Whenever this is called, make sure we clear the CSV file and SID filters
    setCsvFile("");
    setFilterSIDsString("");

    loadPtychoDir();
  }

  void loadPtychoDir()
  {
    clearTable();

    Python python;

    auto func = ptychoModule.findFunction("gather_ptycho_info");
    if (!func.isValid()) {
      QString msg = "Failed to import \"tomviz.ptycho.gather_ptycho_info\"";
      qCritical() << msg;
      return;
    }

    Python::Dict kwargs;
    kwargs.set("ptycho_dir", ptychoDirectory());
    auto result = func.call(kwargs);

    if (!result.isValid() || !result.isDict()) {
      QString msg = "Error calling \"tomviz.ptycho.gather_ptycho_info\"";
      qCritical() << msg;
      return;
    }

    auto resultDict = result.toDict();

    auto sidListV = resultDict["sid_list"].toVariant().toList();
    auto versionDictV = resultDict["version_list"].toVariant().toList();
    auto angleDictV = resultDict["angle_list"].toVariant().toList();
    auto errorDictV = resultDict["error_list"].toVariant().toList();

    sidList.clear();
    versionOptions.clear();
    angleOptions.clear();
    allErrorLists.clear();
    for (size_t i = 0; i < sidListV.size(); ++i) {
      auto sid = sidListV[i].toLong();
      sidList.append(sid);

      auto versionOptionsV = versionDictV[i].toList();
      auto theseAnglesV = angleDictV[i].toList();
      auto theseErrorsV = errorDictV[i].toList();

      QStringList versions;
      QMap<QString, double> angles;
      QMap<QString, QString> errors;
      for (size_t j = 0; j < versionOptionsV.size(); ++j) {
        auto version = QString::fromStdString(versionOptionsV[j].toString());
        versions.append(version);
        angles[version] = theseAnglesV[j].toDouble();
        errors[version] = QString::fromStdString(theseErrorsV[j].toString());
      }
      versionOptions[sid] = versions;
      angleOptions[sid] = angles;
      allErrorLists[sid] = errors;
    }

    resetSelectedVersionsAndUseList();
    updateFilteredSidList();
  }

  void resetSelectedVersionsAndUseList()
  {
    versionList.clear();
    useList.clear();

    for (auto sid: sidList) {
      bool set = false;
      for (auto& version: versionOptions[sid]) {
        if (allErrorLists[sid][version].isEmpty()) {
          // This one is valid.
          versionList.append(version);
          useList.append(true);
          set = true;
          break;
        }
      }
      if (!set) {
        // Do the first one and don't set it to be used.
        versionList.append(versionOptions[sid][0]);
        useList.append(false);
      }
    }

    onSelectedVersionsChanged();
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

    for (int i = 0; i < sids.size(); ++i) {
      auto sid = sids[i];
      auto idx = sidList.indexOf(sid);
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

    updateTable();
  }

  void selectOutputDirectory()
  {
    QString caption = "Select output directory";
    auto dir = QFileDialog::getExistingDirectory(parent.data(), caption,
                                                 outputDirectory());
    if (dir.isEmpty()) {
      return;
    }

    setOutputDirectory(dir);
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

  void setFilterSIDsString(QString s) const { ui.filterSIDsString->setText(s); }

  QString outputDirectory() const { return ui.outputDirectory->text(); }

  void setOutputDirectory(QString s) { ui.outputDirectory->setText(s); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }

  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }
};

PtychoDialog::PtychoDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

PtychoDialog::~PtychoDialog() = default;

void PtychoDialog::show()
{
  m_internal->readSettings();
  QDialog::show();
}

QString PtychoDialog::ptychoDirectory() const
{
  return m_internal->ptychoDirectory();
}

QList<long> PtychoDialog::selectedSids() const
{
  return m_internal->selectedSids();
}

QStringList PtychoDialog::selectedVersions() const
{
  return m_internal->selectedVersions();
}

QList<double> PtychoDialog::selectedAngles() const
{
  return m_internal->selectedAngles();
}

QString PtychoDialog::outputDirectory() const
{
  return m_internal->outputDirectory();
}

bool PtychoDialog::rotateDatasets() const
{
  return m_internal->rotateDatasets();
}

} // namespace tomviz

#include "PtychoDialog.moc"
