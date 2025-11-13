/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PtychoDialog.h"
#include "ui_PtychoDialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>

namespace tomviz {

class PtychoDialog::Internal : public QObject
{
  Q_OBJECT

signals:
  void needTableDataUpdate();

public:
  Ui::PtychoDialog ui;
  QPointer<PtychoDialog> parent;

  bool ptychoguiIsRunning = false;

  // We're going to assume these files will be small and just
  // load the whole thing into memory...
  QStringList versionList;
  QStringList loadMethods;
  QStringList loadSIDs;
  QMap<int, QString> tableColumns;
  long minSID = 0;
  long maxSID = 0;

  Internal(PtychoDialog* p)
    : parent(p)
  {
    ui.setupUi(p);
    setParent(p);

    setupTable();
    setupConnections();
  }

  void setupConnections()
  {
    connect(ui.startPtychoGUI, &QPushButton::clicked, this,
            &Internal::startPtychoGUI);

    connect(ui.selectPtychoDirectory, &QPushButton::clicked, this,
            &Internal::selectPtychoDirectory);
    connect(ui.selectOutputDirectory, &QPushButton::clicked, this,
            &Internal::selectOutputDirectory);
    connect(ui.ptychoDirectory, &QLineEdit::editingFinished,
            this, &Internal::needTableDataUpdate);

    connect(ui.buttonBox, &QDialogButtonBox::accepted, this,
            &Internal::accepted);
  }

  void setupTable()
  {
    auto* table = ui.createSIDsTable;
    auto& columns = tableColumns;

    columns.clear();
    columns[0] = "Version";
    columns[1] = "Method";
    columns[2] = "SIDs";

    table->setColumnCount(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
      auto* header = new QTableWidgetItem(columns[i]);
      table->setHorizontalHeaderItem(i, header);
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

  bool validate(QString& reason)
  {
    // Validate settings
    if (ptychoDirectory().isEmpty() || !QDir(ptychoDirectory()).exists()) {
      reason = "Ptycho directory does not exist: " + ptychoDirectory();
      return false;
    }

    if (versionList.isEmpty()) {
      reason = "No versions found in ptycho directory: " + ptychoDirectory();
      return false;
    }

    bool anyContents = false;
    for (auto s: loadSIDs) {
      if (!s.isEmpty()) {
        anyContents = true;
        break;
      }
    }

    if (!anyContents) {
      reason = "No SIDs were selected for any version";
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

    return true;
  }

  void populateSIDSettingsIfNeeded()
  {
    for (int i = 0; i < versionList.size(); ++i) {
      if (loadMethods.size() <= i) {
        loadMethods.append("from_array");
      }

      if (loadSIDs.size() <= i) {
        QString value = "";
        if (i == 0 && minSID != maxSID && loadMethods[i] != "from_file") {
          value = QString("%1 : %2").arg(minSID).arg(maxSID);
        }
        loadSIDs.append(value);
      }
    }
  }

  void updateTable()
  {
    populateSIDSettingsIfNeeded();

    auto* table = ui.createSIDsTable;
    table->clearContents();

    table->setRowCount(versionList.size());
    for (int i = 0; i < versionList.size(); ++i) {
      for (auto j : tableColumns.keys()) {
        auto column = tableColumns[j];
        auto value = tableValue(i, column);
        if (column == "Method") {
          auto* cb = createMethodComboBox(i, value);
          table->setCellWidget(i, j, cb);
          continue;
        } else if (column == "SIDs") {
          auto* w = createSIDsLineEdit(i, value);
          table->setCellWidget(i, j, w);
          continue;
        }

        auto* item = new QTableWidgetItem(value);
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, j, item);
      }
    }
  }

  QWidget* createMethodComboBox(int row, QString value)
  {
    auto cb = new QComboBox(parent);
    cb->addItem("From Array");
    cb->addItem("From File");
    cb->setCurrentIndex(value == "from_array" ? 0 : 1);

    connect(cb, &QComboBox::currentIndexChanged, this, [this, row, cb]() {
      QString newValue = cb->currentText() == "From Array" ? "from_array" : "from_file";
      // Do a check just in case...
      if (row < loadMethods.size() && row < loadSIDs.size()) {
        loadMethods[row] = newValue;
        // Clear the row as well
        loadSIDs[row] = "";
        updateTable();
      }
    });

    return createTableWidget(cb);
  }

  QWidget* createSIDsLineEdit(int row, QString value)
  {
    auto w = new QLineEdit(value, parent);
    connect(w, &QLineEdit::editingFinished, this, [this, row, w]() {
      if (row < loadSIDs.size()) {
        loadSIDs[row] = w->text();
      }
    });

    auto* tw = new QWidget(ui.createSIDsTable);
    auto* layout = new QHBoxLayout(tw);
    layout->addWidget(w);

    if (row < loadMethods.size() && loadMethods[row] == "from_file") {
      // Also add a button for selecting the directory.
      auto pb = new QPushButton("Select", parent);
      connect(pb, &QPushButton::clicked, w, [this, row, w]() {
        QString caption = "Select SID List File";
        QString filter = "*txt *.csv";
        auto file =
          QFileDialog::getOpenFileName(parent.data(), caption, w->text(), filter);
        if (file.isEmpty()) {
          return;
        }

        if (row < loadSIDs.size()) {
          loadSIDs[row] = file;
          w->setText(file);
        }
      });
      layout->addWidget(pb);
    }

    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return tw;
  }

  QWidget* createTableWidget(QWidget* w)
  {
    // This is required to center the widget
    auto* tw = new QWidget(ui.createSIDsTable);
    auto* layout = new QHBoxLayout(tw);
    layout->addWidget(w);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return tw;
  }

  QString tableValue(int row, QString column)
  {
    if (column == "Version") {
      return versionList[row];
    } else if (column == "Method") {
      return loadMethods[row];
    } else if (column == "SIDs") {
      return loadSIDs[row];
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
    setOutputDirectory(
      settings->value("outputDirectory", defaultOutputDirectory()).toString());
    setRotateDatasets(
      settings->value("rotateDatasets", true).toBool());

    if (!ptychoDirectory().isEmpty() && QDir(ptychoDirectory()).exists()) {
      // Also read the table settings
      versionList = settings->value("versionList", QStringList()).toStringList();
      loadMethods = settings->value("loadMethods", QStringList()).toStringList();
      loadSIDs = settings->value("loadSIDs", QStringList()).toStringList();
      updateTable();
    }

    settings->endGroup();
    settings->endGroup();
  }

  void writeSettings()
  {
    auto settings = pqApplicationCore::instance()->settings();
    settings->beginGroup("ptycho");
    settings->beginGroup("process");

    // Save general settings
    settings->setValue("ptychoGUICommand", ptychoGUICommand());
    settings->setValue("ptychoDirectory", ptychoDirectory());
    settings->setValue("outputDirectory", outputDirectory());
    settings->setValue("rotateDatasets", rotateDatasets());

    // Save table content settings
    settings->setValue("versionList", versionList);
    settings->setValue("loadMethods", loadMethods);
    settings->setValue("loadSIDs", loadSIDs);

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
      emit needTableDataUpdate();
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
    emit needTableDataUpdate();
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
    versionList.clear();
    loadMethods.clear();
    loadSIDs.clear();
    minSID = 0;
    maxSID = 0;
    updateTable();
  }

  void updateTableData(long _minSID, long _maxSID, QStringList _versionList)
  {
    versionList = _versionList;
    loadMethods.clear();
    loadSIDs.clear();
    minSID = _minSID;
    maxSID = _maxSID;
    updateTable();
  }

  QMap<QString, QStringList> loadSIDSettings() const
  {
    QMap<QString, QStringList> ret;
    ret["version_list"] = versionList;
    ret["load_methods"] = loadMethods;
    ret["load_sids"] = loadSIDs;
    return ret;
  }

  QString ptychoGUICommand() const { return ui.ptychoGUICommand->text(); }

  void setPtychoGUICommand(QString s) { ui.ptychoGUICommand->setText(s); }

  QString ptychoDirectory() const { return ui.ptychoDirectory->text(); }

  void setPtychoDirectory(QString s) { ui.ptychoDirectory->setText(s); }

  QString outputDirectory() const { return ui.outputDirectory->text(); }

  void setOutputDirectory(QString s) { ui.outputDirectory->setText(s); }

  bool rotateDatasets() const { return ui.rotateDatasets->isChecked(); }

  void setRotateDatasets(bool b) { ui.rotateDatasets->setChecked(b); }
};

PtychoDialog::PtychoDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
  connect(m_internal.data(), &Internal::needTableDataUpdate,
          this, &PtychoDialog::needTableDataUpdate);
}

PtychoDialog::~PtychoDialog() = default;

void PtychoDialog::show()
{
  m_internal->readSettings();
  QDialog::show();
}

void PtychoDialog::clearTable()
{
  m_internal->clearTable();
}

void PtychoDialog::updateTableData(long minSID, long maxSID,
                                   QStringList versionList)
{
  m_internal->updateTableData(minSID, maxSID, versionList);
}

QString PtychoDialog::ptychoDirectory() const
{
  return m_internal->ptychoDirectory();
}

QMap<QString, QStringList> PtychoDialog::loadSIDSettings() const
{
  return m_internal->loadSIDSettings();
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
