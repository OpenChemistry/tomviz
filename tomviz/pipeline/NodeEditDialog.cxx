/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeEditDialog.h"

#include "CustomOperatorEditDialog.h"
#include "PythonNodeEditorWidget.h"

#include "EditNodeWidget.h"
#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "sinks/LegacyModuleSink.h"

#include "Utilities.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QScreen>
#include <QCloseEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace tomviz {
namespace pipeline {

NodeEditDialog::NodeEditDialog(Node* node, Pipeline* pipeline, QWidget* parent)
  : QDialog(parent), m_node(node), m_pipeline(pipeline),
    m_isNewInsertion(false)
{
  init();
}

NodeEditDialog::NodeEditDialog(Node* node, Pipeline* pipeline,
                               const DeferredLinkInfo& deferred,
                               QWidget* parent)
  : QDialog(parent), m_node(node), m_pipeline(pipeline),
    m_deferred(deferred), m_isNewInsertion(true)
{
  init();
}

NodeEditDialog::~NodeEditDialog()
{
  saveGeometry();

  // Restore the parametersApplied → execute() auto-wiring that was
  // disconnected in init(), unless the node was removed (cancel in insertion
  // mode).
  if (m_node && m_pipeline && m_pipeline->nodes().contains(m_node)) {
    m_node->setEditing(false);
    m_node->setHeld(false);
    connect(m_node, &Node::parametersApplied, m_pipeline,
            [pip = m_pipeline]() { pip->execute(); });
  }
}

Node* NodeEditDialog::node() const
{
  return m_node;
}

void NodeEditDialog::init()
{
  // Float above the main window so the dialog does not slip behind it on
  // macOS while the user keeps working in the render view.
  floatAboveMainWindow(this);

  m_node->setEditing(true);

  // Suppress the auto-execute wiring so the dialog controls execution.
  QObject::disconnect(m_node, &Node::parametersApplied, m_pipeline, nullptr);
  // That only covers the node's own trigger. A node that has never run
  // (an eagerly spliced insertion, or one the strip linked up and handed
  // to this dialog) is New, so any global execute would still pick it
  // up and run it with default parameters; hold it until Apply commits.
  if (m_isNewInsertion || m_node->state() == NodeState::New) {
    m_node->setHeld(true);
  }

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(5, 5, 5, 5);
  layout->setSpacing(5);

  m_buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
    Qt::Horizontal, this);

  m_buttonBox->button(QDialogButtonBox::Ok)->setDefault(false);

  connect(m_buttonBox, &QDialogButtonBox::accepted, this,
          &NodeEditDialog::onOkay);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this,
          &NodeEditDialog::reject);
  connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
          this, &NodeEditDialog::onApply);

  QPushButton* saveAsButton = nullptr;
  m_editWidget = m_node->createPropertiesWidget(m_pipeline, this);
  if (m_editWidget) {
    layout->addWidget(m_editWidget, 1);
    connect(m_editWidget, &EditNodeWidget::canApplyChanged,
            this, &NodeEditDialog::refreshButtonEnablement);

    QString helpUrl = m_editWidget->helpUrl();
    if (!helpUrl.isEmpty()) {
      auto* helpButton = m_buttonBox->addButton(QDialogButtonBox::Help);
      connect(helpButton, &QPushButton::clicked, this,
              [helpUrl]() { openHelpUrl(helpUrl); });
    }

    // A Python node can become a custom operator in the user's directory,
    // script and description as they stand in the editor.
    if (qobject_cast<PythonNodeEditorWidget*>(m_editWidget)) {
      saveAsButton = new QPushButton(tr("Save as Custom Transform..."), this);
      saveAsButton->setObjectName(
        QStringLiteral("saveAsCustomOperatorButton"));
      connect(saveAsButton, &QPushButton::clicked, this,
              &NodeEditDialog::saveAsCustomOperator);
    }
  }

  // The button box keeps the platform's Apply/OK/Cancel arrangement to
  // itself; the extra action sits at the left end of the same row.
  auto* buttonRow = new QHBoxLayout;
  if (saveAsButton) {
    buttonRow->addWidget(saveAsButton);
  }
  buttonRow->addWidget(m_buttonBox, 1);
  layout->addLayout(buttonRow);

  restoreGeometry();

  connect(m_pipeline, &Pipeline::executionStarted,
          this, &NodeEditDialog::refreshButtonEnablement);
  connect(m_pipeline, &Pipeline::executionFinished,
          this, &NodeEditDialog::refreshButtonEnablement);

  // The dialog is modeless, so the node can be deleted while it is open
  // (delete from the strip, session clear, group removal). Drop our pointer
  // and close so OK/Apply cannot touch the freed node.
  connect(m_pipeline, &Pipeline::nodeRemoved, this, [this](Node* removed) {
    if (removed == m_node) {
      m_node = nullptr;
      close();
    }
  });
  refreshButtonEnablement();
}

void NodeEditDialog::saveAsCustomOperator()
{
  auto* python = qobject_cast<PythonNodeEditorWidget*>(m_editWidget);
  if (!python) {
    return;
  }
  const QString directory = tomviz::userDataPath();
  if (directory.isEmpty()) {
    return; // userDataPath() has already told the user why
  }

  // The description's "name" is the natural file stem; a node without
  // one is named after its label.
  QString base = QJsonDocument::fromJson(python->definitionText().toUtf8())
                   .object()
                   .value(QStringLiteral("name"))
                   .toString();
  if (base.isEmpty()) {
    base = python->nodeLabel();
  }

  tomviz::CustomOperatorDraft draft;
  draft.directory = directory;
  draft.stem = tomviz::uniqueOperatorStem(directory, base);
  draft.script = python->scriptText();
  draft.description = python->definitionText();

  // Parented to the main window rather than this dialog, so closing the
  // node editor does not take the draft with it.
  auto* dialog =
    new tomviz::CustomOperatorEditDialog(draft, tomviz::mainWidget());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();

  // The draft has everything it needs; this editor is done, the same as
  // a cancel (an insertion still in progress is rolled back).
  reject();
}

void NodeEditDialog::refreshButtonEnablement()
{
  bool canCommit = m_editWidget && m_editWidget->canApply() &&
                   !m_pipeline->isExecuting();
  m_buttonBox->button(QDialogButtonBox::Apply)->setEnabled(canCommit);
  m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(canCommit);
}

void NodeEditDialog::onApply()
{
  if (!m_node || m_applying) {
    return;
  }

  m_applying = true;
  if (m_editWidget) {
    m_editWidget->applyChangesToOperator();
  }
  m_applying = false;

  if (!m_node) {
    // The node was removed while apply was blocked in a nested event
    // loop; the deferred close was refused, so close now.
    close();
    return;
  }

  if (m_isNewInsertion && !m_insertionCompleted) {
    completeInsertion();
  }

  m_node->setHeld(false);
  m_node->markStale();
  m_pipeline->execute();
}

void NodeEditDialog::onOkay()
{
  if (!m_node || m_applying) {
    return;
  }

  m_applying = true;
  if (m_editWidget) {
    m_editWidget->applyChangesToOperator();
  }
  m_applying = false;

  if (!m_node) {
    close();
    return;
  }

  if (m_isNewInsertion && !m_insertionCompleted) {
    completeInsertion();
  }

  m_node->setHeld(false);
  m_node->markStale();
  m_pipeline->execute();
  accept();
}

void NodeEditDialog::closeEvent(QCloseEvent* event)
{
  if (m_applying) {
    event->ignore();
    return;
  }
  QDialog::closeEvent(event);
}

void NodeEditDialog::reject()
{
  if (m_applying) {
    return;
  }

  if (m_isNewInsertion && !m_insertionCompleted) {
    // The insertion was applied eagerly when the dialog opened.  Undo it so
    // that cancel is a true no-op.  Removing the new node also drops its own
    // input/output links (for sources there are none).  Clear m_node first so
    // the nodeRemoved handler above does not close() and re-enter reject().
    Node* node = m_node;
    m_node = nullptr;
    m_pipeline->removeNode(node);

    // Recreate the original links that were broken to insert the node.
    for (const auto& ep : m_deferred.linksToRestore) {
      if (ep.from && ep.to) {
        auto* link = m_pipeline->createLink(ep.from, ep.to);
        // Breaking the link to insert the node hid any downstream module: a
        // direct module sink via onInputDisconnected, or modules behind a
        // SinkGroupNode via the group's resetVisualization() fan-out. Re-link
        // alone does not re-show them and we deliberately do not re-execute, so
        // ask the downstream node to restore its presentation explicitly. The
        // VTK objects still hold the last data, so this is presentation-only --
        // no pipeline run and no dependence on upstream PortData (which a
        // transient source releases on disconnect). Done only on this cancel
        // path so the insertion preview keeps its current behavior (the moved
        // module stays hidden until the not-yet-run transform produces data).
        if (link && ep.to->node()) {
          ep.to->node()->restorePresentation();
        }
      }
    }

    // createLink() above marks the affected downstream subtree stale.
    // Restore the states captured before the insertion so nothing is left
    // spuriously stale (which would otherwise force a needless re-run).
    for (const auto& ps : m_deferred.portStaleStates) {
      if (ps.port) {
        ps.port->setStale(ps.stale);
      }
    }
    for (const auto& ns : m_deferred.nodeStates) {
      if (ns.node) {
        ns.node->setStateNoCascade(ns.state);
      }
    }

    emit insertionCanceled();
  }

  QDialog::reject();
}

void NodeEditDialog::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);

  auto* mainWin = tomviz::mainWidget();
  if (!mainWin) {
    return;
  }

  auto* screen = mainWin->screen();
  auto screenGeom = screen ? screen->availableGeometry()
                           : QRect(0, 0, 1920, 1080);

  auto mainCenter = mainWin->frameGeometry().center();
  auto dlgSize = frameGeometry().size();

  int x = mainCenter.x() - dlgSize.width() / 2;
  int y = mainCenter.y() - dlgSize.height() / 2;

  x = qBound(screenGeom.left(), x,
              screenGeom.right() - dlgSize.width());
  y = qBound(screenGeom.top(), y,
              screenGeom.bottom() - dlgSize.height());

  move(x, y);
  raise();
  activateWindow();
}

void NodeEditDialog::saveGeometry()
{
  // No application core in test harnesses: nothing to remember into.
  auto* core = pqApplicationCore::instance();
  if (!m_node || !core) {
    return;
  }
  QSettings* settings = core->settings();
  QString key =
    QString("Edit%1NodeDialogGeometry").arg(m_node->label());
  settings->setValue(key, QVariant(geometry()));
}

void NodeEditDialog::restoreGeometry()
{
  if (!m_node) {
    return;
  }
  auto* core = pqApplicationCore::instance();
  QVariant saved = core ? core->settings()->value(
                            QString("Edit%1NodeDialogGeometry")
                              .arg(m_node->label()))
                        : QVariant();
  if (!saved.isNull()) {
    resize(saved.toRect().size());
  } else {
    resize(900, 700);
  }
}

void NodeEditDialog::completeInsertion()
{
  // The insertion (node + link rewiring) was performed eagerly when the
  // dialog opened, so the pipeline already has its final topology.  There is
  // nothing left to rewire here -- just mark the insertion committed so the
  // cancel path will not try to roll it back.
  m_insertionCompleted = true;
  m_isNewInsertion = false;
  emit insertionCompleted(m_node);
}

} // namespace pipeline
} // namespace tomviz
