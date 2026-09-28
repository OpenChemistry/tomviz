/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveDataDialog.h"

#include "pipeline/Node.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SinkNode.h"

#include "ui_SaveDataDialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <tuple>

namespace tomviz {

using pipeline::Node;
using pipeline::OutputPort;
using pipeline::Pipeline;
using pipeline::PortType;

namespace {

const char* kSettingsGroup = "SaveDataDialog";

/// Sinks never produce data of their own, and a sink group's
/// passthrough ports only mirror their upstream port.
bool isSinkLike(Node* node)
{
  return qobject_cast<pipeline::SinkNode*>(node) != nullptr ||
         qobject_cast<pipeline::SinkGroupNode*>(node) != nullptr;
}

/// A leaf is a node whose output only feeds visualizations (or nothing
/// at all) — the tip of a processing branch.
bool isLeafNode(Node* node)
{
  for (auto* downstream : node->downstreamNodes()) {
    if (!isSinkLike(downstream)) {
      return false;
    }
  }
  return true;
}

/// Reduce @a name to characters that are safe in a filename on every
/// platform tomviz runs on. Returns an empty string if nothing survives.
QString sanitize(const QString& name)
{
  static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9._-]+"));
  QString result = name;
  result.replace(unsafe, QStringLiteral("_"));
  while (result.startsWith('_') || result.startsWith('.')) {
    result.remove(0, 1);
  }
  while (result.endsWith('_') || result.endsWith('.')) {
    result.chop(1);
  }
  return result;
}

QString nodeBaseName(Node* node)
{
  QString name = sanitize(node->label());
  return name.isEmpty() ? QStringLiteral("node") : name;
}

/// Filename stem for each node, with a 1-based numeric suffix added to
/// every node sharing a label so the stems stay distinguishable.
QHash<Node*, QString> uniqueNodeNames(const QList<OutputPort*>& ports)
{
  QList<Node*> ordered;
  QHash<QString, int> counts;
  for (auto* port : ports) {
    auto* node = port->node();
    if (!node || ordered.contains(node)) {
      continue;
    }
    ordered.append(node);
    counts[nodeBaseName(node)]++;
  }

  QHash<Node*, QString> result;
  QHash<QString, int> seen;
  for (auto* node : ordered) {
    QString base = nodeBaseName(node);
    if (counts.value(base) > 1) {
      base = QStringLiteral("%1_%2").arg(base).arg(++seen[base]);
    }
    result.insert(node, base);
  }
  return result;
}

/// Take @a stem, or the first `stem_N` variant nothing has claimed yet.
/// Sanitizing can collapse distinct names onto the same stem, so this
/// runs even after node labels have been disambiguated.
QString claimName(const QString& stem, QSet<QString>& used)
{
  QString claimed = stem;
  for (int suffix = 2; used.contains(claimed.toLower()); ++suffix) {
    claimed = QStringLiteral("%1_%2").arg(stem).arg(suffix);
  }
  used.insert(claimed.toLower());
  return claimed;
}

/// The part of @a stem past the shared @a base, for the braced display
/// name. Reads from the claimed stem rather than the array name so a
/// collision suffix shows up in the row the way it will on disk.
QString tailOf(const QString& stem, const QString& base)
{
  QString tail = stem.mid(base.size());
  if (tail.startsWith('_')) {
    tail.remove(0, 1);
  }
  return tail;
}

} // namespace

SaveDataDialog::SaveDataDialog(Pipeline* pipeline, QWidget* parent)
  : QDialog(parent), m_ui(new Ui::SaveDataDialog), m_pipeline(pipeline)
{
  init();
}

SaveDataDialog::SaveDataDialog(Node* node, QWidget* parent)
  : QDialog(parent), m_ui(new Ui::SaveDataDialog), m_node(node)
{
  init();
  restrictScope();
}

SaveDataDialog::SaveDataDialog(OutputPort* port, QWidget* parent)
  : QDialog(parent), m_ui(new Ui::SaveDataDialog), m_port(port)
{
  init();
  restrictScope();
}

void SaveDataDialog::restrictScope()
{
  // "Leaf nodes only" is meaningless once we're down to one node or one
  // port — pin the scope, but leave the group visible so the rule the
  // dialog is following stays on screen.
  m_ui->allPortsRadio->setChecked(true);
  m_ui->scopeGroup->setEnabled(false);

  rebuildPlan();
}

void SaveDataDialog::init()
{
  m_ui->setupUi(this);

  const QList<QPair<QComboBox*, PortType>> combos = {
    { m_ui->imageDataCombo, PortType::ImageData },
    { m_ui->tableCombo, PortType::Table },
    { m_ui->moleculeCombo, PortType::Molecule }
  };
  for (const auto& [combo, type] : combos) {
    for (const auto& format : PortDataWriter::formats(type)) {
      combo->addItem(format.label(), format.id);
    }
    connect(combo, &QComboBox::currentIndexChanged, this,
            &SaveDataDialog::rebuildPlan);
  }

  m_ui->fileTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

  connect(m_ui->browseButton, &QPushButton::clicked, this,
          &SaveDataDialog::browseForDirectory);
  connect(m_ui->directoryEdit, &QLineEdit::textChanged, this,
          &SaveDataDialog::rebuildPlan);
  connect(m_ui->leafOnlyRadio, &QRadioButton::toggled, this,
          &SaveDataDialog::rebuildPlan);
  connect(m_ui->selectAllButton, &QPushButton::clicked, this,
          [this]() { setAllChecked(true); });
  connect(m_ui->deselectAllButton, &QPushButton::clicked, this,
          [this]() { setAllChecked(false); });
  connect(m_ui->fileTree, &QTreeWidget::itemChanged, this, [this]() {
    rememberCheckState();
    updateSummary();
  });
  connect(this, &QDialog::accepted, this, &SaveDataDialog::saveSettings);

  restoreSettings();
  rebuildPlan();
}

SaveDataDialog::~SaveDataDialog() = default;

namespace {

// The output ports @a scope covers within @a node, before any test of
// what they hold
QList<OutputPort*> portsInScope(Node* node, SaveDataDialog::Scope scope)
{
  if (!node || isSinkLike(node)) {
    return {};
  }
  if (scope == SaveDataDialog::Scope::LeafNodes && !isLeafNode(node)) {
    return {};
  }
  return node->outputPorts();
}

bool writable(OutputPort* port)
{
  return port && !PortDataWriter::formats(port->type()).isEmpty();
}

// Transient, and its data has already been let go
bool released(OutputPort* port)
{
  return writable(port) && !port->hasData() && !port->isPersistent();
}

} // namespace

QList<OutputPort*> SaveDataDialog::candidatePorts(OutputPort* port, Scope)
{
  if (!writable(port) || !port->hasData()) {
    return {};
  }
  return { port };
}

QList<OutputPort*> SaveDataDialog::candidatePorts(Node* node, Scope scope)
{
  QList<OutputPort*> result;
  for (auto* port : portsInScope(node, scope)) {
    result.append(candidatePorts(port, scope));
  }
  return result;
}

QList<OutputPort*> SaveDataDialog::candidatePorts(Pipeline* pipeline,
                                                  Scope scope)
{
  QList<OutputPort*> result;
  if (!pipeline) {
    return result;
  }
  for (auto* node : pipeline->nodes()) {
    result.append(candidatePorts(node, scope));
  }
  return result;
}

QList<OutputPort*> SaveDataDialog::releasedPorts(OutputPort* port, Scope)
{
  if (!released(port)) {
    return {};
  }
  return { port };
}

QList<OutputPort*> SaveDataDialog::releasedPorts(Node* node, Scope scope)
{
  QList<OutputPort*> result;
  for (auto* port : portsInScope(node, scope)) {
    result.append(releasedPorts(port, scope));
  }
  return result;
}

QList<OutputPort*> SaveDataDialog::releasedPorts(Pipeline* pipeline,
                                                 Scope scope)
{
  QList<OutputPort*> result;
  if (!pipeline) {
    return result;
  }
  for (auto* node : pipeline->nodes()) {
    result.append(releasedPorts(node, scope));
  }
  return result;
}

QList<SaveDataDialog::PortPlan> SaveDataDialog::planPorts(
  const QList<OutputPort*>& ports,
  const QHash<OutputPort*, QStringList>& arrayNames, const QString& directory,
  const QHash<PortType, PortFormat>& formats)
{
  auto nodeNames = uniqueNodeNames(ports);
  QDir destination(directory);

  QList<PortPlan> plans;
  QSet<QString> usedNames;
  for (auto* port : ports) {
    auto format = formats.value(PortDataWriter::formatGroup(port->type()));
    if (format.extension.isEmpty()) {
      continue;
    }

    QStringList base{ nodeNames.value(port->node()), sanitize(port->name()) };
    base.removeAll(QString());

    PortPlan plan;
    plan.port = port;

    auto arrays = arrayNames.value(port);
    if (format.multiArray) {
      QString stem = claimName(base.join('_'), usedNames);
      plan.displayName = QStringLiteral("%1.%2").arg(stem, format.extension);
      plan.entries.append(
        { port, arrays, destination.filePath(plan.displayName) });
    } else {
      // One file per array: the stems differ only in their tail, so the
      // row can show them braced rather than one row per file.
      QStringList tails;
      for (const auto& arrayName : arrays) {
        QStringList parts = base;
        parts.append(sanitize(arrayName));
        parts.removeAll(QString());

        QString stem = claimName(parts.join('_'), usedNames);
        QString fileName =
          QStringLiteral("%1.%2").arg(stem, format.extension);
        tails.append(tailOf(stem, base.join('_')));
        plan.entries.append(
          { port, { arrayName }, destination.filePath(fileName) });
      }

      plan.displayName =
        plan.entries.size() == 1
          ? QFileInfo(plan.entries.first().path).fileName()
          : QStringLiteral("%1_{%2}.%3")
              .arg(base.join('_'), tails.join('|'), format.extension);
    }

    if (!plan.entries.isEmpty()) {
      plans.append(plan);
    }
  }

  return plans;
}

SaveDataDialog::Scope SaveDataDialog::currentScope() const
{
  if (m_port || m_node) {
    return Scope::AllPorts;
  }
  return m_ui->leafOnlyRadio->isChecked() ? Scope::LeafNodes
                                          : Scope::AllPorts;
}

QList<OutputPort*> SaveDataDialog::currentCandidates() const
{
  auto scope = currentScope();
  if (m_port) {
    return candidatePorts(m_port, scope);
  }
  if (m_node) {
    return candidatePorts(m_node, scope);
  }
  return candidatePorts(m_pipeline, scope);
}

QList<OutputPort*> SaveDataDialog::currentReleased() const
{
  auto scope = currentScope();
  if (m_port) {
    return releasedPorts(m_port, scope);
  }
  if (m_node) {
    return releasedPorts(m_node, scope);
  }
  return releasedPorts(m_pipeline, scope);
}

PortFormat SaveDataDialog::formatFor(PortType type) const
{
  QComboBox* combo = nullptr;
  switch (PortDataWriter::formatGroup(type)) {
    case PortType::ImageData:
      combo = m_ui->imageDataCombo;
      break;
    case PortType::Table:
      combo = m_ui->tableCombo;
      break;
    case PortType::Molecule:
      combo = m_ui->moleculeCombo;
      break;
    default:
      return PortFormat();
  }

  return PortDataWriter::formatById(type, combo->currentData().toString());
}

void SaveDataDialog::browseForDirectory()
{
  // Mirrors the "directory" parameter widget in ParameterInterfaceBuilder
  // so the two browse affordances behave identically.
  QString browseDir;
  if (!m_ui->directoryEdit->text().isEmpty()) {
    QDir dir = QFileInfo(m_ui->directoryEdit->text()).dir();
    if (dir.exists()) {
      browseDir = dir.absolutePath();
    }
  }

  QString path =
    QFileDialog::getExistingDirectory(window(), "Select Directory", browseDir);
  if (!path.isNull()) {
    m_ui->directoryEdit->setText(path);
  }
}

void SaveDataDialog::rebuildPlan()
{
  auto ports = currentCandidates();

  // Reading array names requires the payload, which for an OnDisk port
  // means a load from the cache file. Cache the result per port so
  // flipping formats doesn't pay that cost again.
  QApplication::setOverrideCursor(Qt::WaitCursor);
  for (auto* port : ports) {
    if (m_arrayNames.contains(port)) {
      continue;
    }
    auto handle = port->materialize();
    m_arrayNames.insert(port, handle ? PortDataWriter::arrayNames(*handle)
                                     : QStringList());
  }
  QApplication::restoreOverrideCursor();

  QHash<PortType, PortFormat> formats;
  for (auto group :
       { PortType::ImageData, PortType::Table, PortType::Molecule }) {
    formats.insert(group, formatFor(group));
  }

  m_plans =
    planPorts(ports, m_arrayNames, m_ui->directoryEdit->text(), formats);

  QSignalBlocker blocker(m_ui->fileTree);
  m_ui->fileTree->clear();
  for (const auto& plan : m_plans) {
    auto* item = new QTreeWidgetItem(m_ui->fileTree);
    item->setText(0, plan.port->node()->label());
    item->setText(1, plan.port->name());
    item->setText(2, pipeline::portTypeToString(plan.port->type()));
    item->setText(3, plan.displayName);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, m_unchecked.contains(plan.port) ? Qt::Unchecked
                                                           : Qt::Checked);
  }
  blocker.unblock();

  // Grey out format rows for types that aren't in scope.
  QSet<PortType> groups;
  for (auto* port : ports) {
    groups.insert(PortDataWriter::formatGroup(port->type()));
  }
  const std::initializer_list<std::tuple<QWidget*, QWidget*, PortType>> rows = {
    { m_ui->imageDataLabel, m_ui->imageDataCombo, PortType::ImageData },
    { m_ui->tableLabel, m_ui->tableCombo, PortType::Table },
    { m_ui->moleculeLabel, m_ui->moleculeCombo, PortType::Molecule }
  };
  for (const auto& [label, combo, type] : rows) {
    label->setEnabled(groups.contains(type));
    combo->setEnabled(groups.contains(type));
  }

  updateSummary();
}

void SaveDataDialog::rememberCheckState()
{
  for (int i = 0; i < m_ui->fileTree->topLevelItemCount(); ++i) {
    auto* port = m_plans[i].port;
    if (m_ui->fileTree->topLevelItem(i)->checkState(0) == Qt::Checked) {
      m_unchecked.remove(port);
    } else {
      m_unchecked.insert(port);
    }
  }
}

void SaveDataDialog::setAllChecked(bool checked)
{
  QSignalBlocker blocker(m_ui->fileTree);
  for (int i = 0; i < m_ui->fileTree->topLevelItemCount(); ++i) {
    m_ui->fileTree->topLevelItem(i)->setCheckState(
      0, checked ? Qt::Checked : Qt::Unchecked);
  }
  blocker.unblock();

  rememberCheckState();
  updateSummary();
}

void SaveDataDialog::updateSummary()
{
  int total = 0;
  for (const auto& plan : m_plans) {
    total += plan.entries.size();
  }

  int count = selectedEntries().size();
  if (m_plans.isEmpty()) {
    m_ui->summaryLabel->setText("No data available to save.");
  } else {
    m_ui->summaryLabel->setText(
      QStringLiteral("%1 of %2 files selected.").arg(count).arg(total));
  }

  bool hasDirectory = !m_ui->directoryEdit->text().trimmed().isEmpty();
  m_ui->buttonBox->button(QDialogButtonBox::Save)
    ->setEnabled(count > 0 && hasDirectory);

  // Say which outputs are missing and why, rather than leave them to be
  // wondered about
  const auto gone = currentReleased();
  if (gone.isEmpty()) {
    m_ui->releasedNote->hide();
    return;
  }
  QStringList names;
  for (auto* port : gone.mid(0, 3)) {
    names << QStringLiteral("%1 (%2)").arg(port->node()->label(),
                                          port->name());
  }
  if (gone.size() > 3) {
    names << tr("%1 more").arg(gone.size() - 3);
  }
  const QString joined = names.join(QStringLiteral(", "));
  m_ui->releasedNote->setText(
    gone.size() == 1
      ? tr("%1 cannot be saved because it is transient and its data is no "
           "longer in memory. To save it, right-click its port and choose "
           "Persist in Memory; tomviz runs that step again.")
          .arg(joined)
      : tr("%1 outputs cannot be saved because they are transient and their "
           "data is no longer in memory: %2. To save one, right-click its "
           "port and choose Persist in Memory; tomviz runs that step again.")
          .arg(gone.size())
          .arg(joined));
  m_ui->releasedNote->show();
}

QList<SaveDataDialog::Entry> SaveDataDialog::selectedEntries() const
{
  QList<Entry> selected;
  for (int i = 0; i < m_ui->fileTree->topLevelItemCount(); ++i) {
    if (m_ui->fileTree->topLevelItem(i)->checkState(0) == Qt::Checked) {
      selected.append(m_plans[i].entries);
    }
  }
  return selected;
}

int SaveDataDialog::writeEntries(const QList<Entry>& entries, QWidget* parent)
{
  if (entries.isEmpty()) {
    return 0;
  }

  QStringList existing;
  for (const auto& entry : entries) {
    if (QFileInfo::exists(entry.path)) {
      existing << QFileInfo(entry.path).fileName();
    }
  }
  if (!existing.isEmpty()) {
    auto answer = QMessageBox::question(
      parent, "Overwrite Files?",
      QStringLiteral("%1 of the %2 files already exist and will be "
                     "overwritten:\n\n%3\n\nContinue?")
        .arg(existing.size())
        .arg(entries.size())
        .arg(existing.mid(0, 10).join('\n') +
             (existing.size() > 10 ? "\n..." : "")));
    if (answer != QMessageBox::Yes) {
      return 0;
    }
  }

  // Every entry shares one destination, so the first is representative.
  QDir().mkpath(QFileInfo(entries.first().path).absolutePath());

  QProgressDialog progress("Saving data...", "Cancel", 0, entries.size(),
                           parent);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);

  QStringList failures;
  int written = 0;
  for (int i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    QString name = QFileInfo(entry.path).fileName();

    progress.setValue(i);
    progress.setLabelText(QStringLiteral("Saving %1...").arg(name));
    QApplication::processEvents();
    if (progress.wasCanceled()) {
      break;
    }

    auto handle = entry.port->materialize();
    if (!handle ||
        !PortDataWriter::write(*handle, entry.arrayNames, entry.path)) {
      failures << name;
      continue;
    }
    ++written;
  }
  progress.setValue(entries.size());

  if (!failures.isEmpty()) {
    QMessageBox::warning(
      parent, "Save Data",
      QStringLiteral("Failed to write %1 of %2 files:\n\n%3")
        .arg(failures.size())
        .arg(entries.size())
        .arg(failures.mid(0, 10).join('\n') +
             (failures.size() > 10 ? "\n..." : "")));
  }

  return written;
}

void SaveDataDialog::restoreSettings()
{
  auto* core = pqApplicationCore::instance();
  if (!core) {
    return;
  }

  auto* settings = core->settings();
  settings->beginGroup(kSettingsGroup);

  // The destination is deliberately not restored — writing a pile of
  // files is destructive enough that the user should name the directory
  // every time rather than inherit one from a previous session.

  const QList<QPair<QComboBox*, QString>> combos = {
    { m_ui->imageDataCombo, "imageDataFormat" },
    { m_ui->tableCombo, "tableFormat" },
    { m_ui->moleculeCombo, "moleculeFormat" }
  };
  for (const auto& [combo, key] : combos) {
    int index = combo->findData(settings->value(key).toString());
    if (index >= 0) {
      combo->setCurrentIndex(index);
    }
  }

  bool allPersisted = settings->value("allPersisted", true).toBool();
  m_ui->allPortsRadio->setChecked(allPersisted);
  m_ui->leafOnlyRadio->setChecked(!allPersisted);

  settings->endGroup();
}

void SaveDataDialog::saveSettings() const
{
  auto* core = pqApplicationCore::instance();
  if (!core) {
    return;
  }

  auto* settings = core->settings();
  settings->beginGroup(kSettingsGroup);
  settings->setValue("imageDataFormat", m_ui->imageDataCombo->currentData());
  settings->setValue("tableFormat", m_ui->tableCombo->currentData());
  settings->setValue("moleculeFormat", m_ui->moleculeCombo->currentData());
  if (!m_node && !m_port) {
    // A restricted save has no scope choice to remember; persisting its
    // pinned value would silently retarget the next unrestricted one.
    settings->setValue("allPersisted", m_ui->allPortsRadio->isChecked());
  }
  settings->endGroup();
}

} // namespace tomviz
