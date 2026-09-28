/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqOutputWidget.h>
#include <pqPluginDockWidgetsBehavior.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkPVRenderView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSettings.h>
#include <vtkSMViewProxy.h>
#include <vtkMolecule.h>
#include <vtkSmartPointer.h>
#include <vtkVector.h>

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include "AboutDialog.h"
#include "AcquisitionWidget.h"
#include "ActiveObjects.h"
#include "AddAlignReaction.h"
#include "AddPythonTransformReaction.h"
#include "CustomOperatorEditDialog.h"
#include "CustomOperatorManagerDialog.h"
#include "AnimationHelperDialog.h"
#include "animations/AnimationSceneGuard.h"
#include "animations/RecordedAnimations.h"
#include "AxesReaction.h"
#include "Behaviors.h"
#include "CameraReaction.h"
#include "ColorMap.h"
#include "Connection.h"
#include "DataBroker.h"
#include "DataBrokerLoadReaction.h"
#include "DataBrokerSaveReaction.h"
#include "DataTransformMenu.h"
#include "FileFormatManager.h"
#include "LoadDataReaction.h"
#include "LoadPaletteReaction.h"
#include "LoadStackReaction.h"
#include "LoadTimeSeriesReaction.h"
#include "PipelineModuleMenu.h"
#include "pipeline/AutoExecuteController.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineExecutor.h"
#include "pipeline/ThreadedExecutor.h"
#include "pipeline/PipelineControlsWidget.h"
#include "pipeline/PipelineStripWidget.h"
#include "pipeline/Node.h"
#include "pipeline/TransformNode.h"
#include "pipeline/OutputPort.h"
#include "pipeline/PassthroughOutputPort.h"
#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/PortData.h"
#include "pipeline/PortDataMetadata.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/NodeEditDialog.h"
#include "pipeline/NodePropertiesPanel.h"
#include "pipeline/LinkPropertiesWidget.h"
#include "pipeline/SinkGroupPropertiesWidget.h"
#include "pipeline/SourceNode.h"
#include "pipeline/SinkNode.h"
#include "pipeline/VolumePropertiesWidget.h"
#include "MoleculeProperties.h"
#include "MovieExportDialog.h"
#include "CentralWidget.h"
#include "OperatorSearchDialog.h"
#include "ProgressDialogManager.h"
#include "PtychoRunner.h"
#include "PyXRFRunner.h"
#include "AddPythonSourceReaction.h"
#include "PythonUtilities.h"
#include "RecentFilesMenu.h"
#include "ReconstructionReaction.h"
#include "ResetReaction.h"
#include "SaveDataDialog.h"
#include "SaveDataReaction.h"
#include "SaveLoadStateReaction.h"
#include "SaveLoadTemplateReaction.h"
#include "SaveScreenshotReaction.h"
#include "SaveWebReaction.h"
#include "SetDataTypeReaction.h"
#include "SetTiltAnglesReaction.h"
#include "Utilities.h"
#include "ViewMenuManager.h"
#include "WelcomeDialog.h"
#include "tomvizConfig.h"

#include <QAction>
#include <QCloseEvent>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QMessageBox>
#include <QSet>
#include <QShortcut>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSet>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

namespace {
QString getAutosaveFile()
{
  // workaround to get user config location
  QString dataPath;
  dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dataDir(dataPath);
  if (!dataDir.exists()) {
    dataDir.mkpath(dataPath);
  }
  return dataDir.absoluteFilePath(".tomviz_autosave.tvsm");
}

/// Append the trailing "Save Data" section shared by the node and port
/// context menus: a separator, then the action, greyed out when the
/// dialog would have nothing to offer. @a target is whatever
/// SaveDataDialog can be restricted to — a Node or an OutputPort.
template <typename Target>
void addSaveDataAction(QMenu& menu, Target* target, QWidget* parent,
                       bool enabled)
{
  using tomviz::SaveDataDialog;

  if (!menu.isEmpty()) {
    menu.addSeparator();
  }

  auto* action = menu.addAction("Save Data", [target, parent]() {
    SaveDataDialog dialog(target, parent);
    if (dialog.exec() == QDialog::Accepted) {
      SaveDataDialog::writeEntries(dialog.selectedEntries(), parent);
    }
  });
  // Also when only released ports are left, so the dialog can say why
  // they cannot be saved
  const auto scope = SaveDataDialog::Scope::AllPorts;
  action->setEnabled(
    enabled &&
    (!SaveDataDialog::candidatePorts(target, scope).isEmpty() ||
     !SaveDataDialog::releasedPorts(target, scope).isEmpty()));
}
} // namespace
class Connection;

namespace tomviz {

MainWindow* MainWindow::instance()
{
  for (auto* w : QApplication::topLevelWidgets()) {
    auto* mw = qobject_cast<MainWindow*>(w);
    if (mw)
      return mw;
  }
  return nullptr;
}

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
  : QMainWindow(parent, flags), m_ui(new Ui::MainWindow)
{

  // Update back light azimuth default on view.
  connect(pqApplicationCore::instance()->getServerManagerModel(),
          &pqServerManagerModel::viewAdded, [](pqView* view) {
            if (view && view->getProxy()->IsA("vtkSMRenderViewProxy")) {
              vtkSMPropertyHelper helper(view->getProxy(), "BackLightAzimuth");
              // See https://github.com/OpenChemistry/tomviz/issues/1525
              helper.Set(60);
            }
          });

  // checkOpenGL();
  m_ui->setupUi(this);
  // Allow docks in the same area to be split in both directions (e.g. the
  // Pipelines and Properties docks side by side with a horizontal split, not
  // only stacked vertically or tabbed).
  setDockNestingEnabled(true);
  // Give the dock splitter a visible 1px line (most styles draw it nearly
  // invisible). The pipeline scroll area rules reassert its white background:
  // setting any stylesheet on the main window otherwise stops the palette-set
  // background from being honored, turning the area grey. Target the scroll
  // area, its content container and the strip by name; a blanket
  // "#pipelineScroll QWidget" rule would also match the context menus
  // parented to the strip widget, replacing their native rendering with a
  // flat white one. The strip has to be listed explicitly because it is
  // custom-painted and never fills its own background — leave it out and it
  // alone goes grey.
  setStyleSheet(styleSheet() +
                QStringLiteral(
                  "QMainWindow::separator { background: palette(mid);"
                  " width: 1px; height: 1px; }"
                  "#pipelineScroll, #pipelineScrollContainer, #pipelineStrip"
                  " { background: white; }"));
  setAcceptDrops(true);
  // Several child widgets accept drops by default (line edits, the
  // message/python text docks, item views) and claim a file drag only
  // to refuse it, leaving dead zones where drag-and-drop silently does
  // nothing. Filter file drags application-wide so dropping data works
  // anywhere over this window.
  qApp->installEventFilter(this);
  // Force full messages to be shown
  m_ui->outputWidget->showFullMessages(true);
  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &MainWindow::autosave);
  m_timer->start(5 /*minutes*/ * 60 /*seconds per minute*/ *
                 1000 /*msec per second*/);

  qSetMessagePattern("[%{type}] %{message}");

  QString version(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0)
    version.append("-").append(TOMVIZ_VERSION_EXTRA);
  setWindowTitle("tomviz " + version);

  QIcon icon(":/icons/tomviz.png");
  setWindowIcon(icon);

  // tabify output messages widget.
  tabifyDockWidget(m_ui->dockWidgetAnimation, m_ui->dockWidgetMessages);
  tabifyDockWidget(m_ui->dockWidgetAnimation, m_ui->dockWidgetPythonConsole);

  // don't think tomviz should import ParaView modules by default in Python
  // shell.
  pqPythonShell::setPreamble(QStringList());

  // Hide these dock widgets when tomviz is first opened. If they are later
  // opened and remain open while tomviz is shut down, their visibility and
  // geometry state will be saved out to the settings file. The dock widgets
  // will then be restored when the Behaviors (in particular the
  // pqPersistentMainWindowStateBehavior) are instantiated further down.
  m_ui->dockWidgetMessages->hide();
  m_ui->dockWidgetPythonConsole->hide();
  m_ui->dockWidgetAnimation->hide();
  m_ui->dockWidgetLightsInspector->hide();

  // Tweak the initial sizes of the dock widgets.
  QList<QDockWidget*> docks;
  docks << m_ui->dockWidget << m_ui->dockWidget_5 << m_ui->dockWidgetMessages;
  QList<int> dockSizes;
  dockSizes << 250 << 250 << 250;
  resizeDocks(docks, dockSizes, Qt::Horizontal);
  docks.clear();
  dockSizes.clear();
  docks << m_ui->dockWidgetAnimation;
  dockSizes << 200;
  resizeDocks(docks, dockSizes, Qt::Vertical);

  // raise dockWidgetMessages on error.
  connect(m_ui->outputWidget, &pqOutputWidget::messageDisplayed, this,
          &MainWindow::handleMessage);

  // The color map / histogram updates are now driven by onNodeSelected()
  // and onPortSelected() which call CentralWidget::setActiveSinkNode()
  // and CentralWidget::setActiveVolumeData().

  // Create pipeline controls and strip widgets in the left dock
  m_pipelineControls = new pipeline::PipelineControlsWidget(this);
  m_ui->pipelineContainerLayout->addWidget(m_pipelineControls);
  m_pipelineStrip = new pipeline::PipelineStripWidget(this);
  m_pipelineStrip->setObjectName("pipelineStrip");
  m_pipelineStrip->setSortOrder(pipeline::SortOrder::DepthFirst);
  auto* pipelineScroll = new QScrollArea(this);
  pipelineScroll->setObjectName("pipelineScroll");
  pipelineScroll->setWidgetResizable(true);
  pipelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // pipelineScroll->setFrameShape(QFrame::NoFrame);
  QPalette scrollPal = pipelineScroll->palette();
  scrollPal.setColor(QPalette::Window, Qt::white);
  pipelineScroll->setPalette(scrollPal);
  // Wrap the strip widget in a container with padding so the scroll area
  // provides insets on the top, right, and bottom.
  auto* scrollContainer = new QWidget();
  scrollContainer->setObjectName("pipelineScrollContainer");
  scrollContainer->setAutoFillBackground(true);
  scrollContainer->setPalette(scrollPal);
  auto* scrollLayout = new QVBoxLayout(scrollContainer);
  scrollLayout->setContentsMargins(0, 4, 4, 4);
  scrollLayout->addWidget(m_pipelineStrip);
  pipelineScroll->setWidget(scrollContainer);
  m_ui->pipelineContainerLayout->addWidget(pipelineScroll);
  connect(m_pipelineControls,
          &pipeline::PipelineControlsWidget::dimmingToggled,
          m_pipelineStrip,
          &pipeline::PipelineStripWidget::setDimmingEnabled);
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::nodeSelected,
          this, &MainWindow::onNodeSelected);
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::portSelected,
          this, &MainWindow::onPortSelected);
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::linkSelected,
          this, &MainWindow::onLinkSelected);
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::selectionCleared,
          &ActiveObjects::instance(), &ActiveObjects::clearActiveSelection);
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::deleteNodeRequested,
          this, [this](pipeline::Node* node) {
            auto* p = pipeline();
            if (p && !p->isExecuting()) {
              p->removeNode(node);
            }
          });
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::deleteLinkRequested,
          this, [this](pipeline::Link* link) {
            auto* p = pipeline();
            if (p && !p->isExecuting()) {
              p->removeLink(link);
            }
          });

  // Sync ActiveObjects changes back to the strip widget and properties panel.
  // This ensures programmatic setActiveNode/Port/Link calls are reflected.
  connect(&ActiveObjects::instance(), &ActiveObjects::mouseOverVoxel, this,
          &MainWindow::onMouseOverVoxel);
  connect(&ActiveObjects::instance(), &ActiveObjects::activeNodeChanged,
          this, &MainWindow::onActiveNodeChanged);
  connect(&ActiveObjects::instance(), &ActiveObjects::activePortChanged,
          this, &MainWindow::onActivePortChanged);
  connect(&ActiveObjects::instance(), &ActiveObjects::activeLinkChanged,
          this, &MainWindow::onActiveLinkChanged);

  // Double-click on a node with an editor opens the edit dialog
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::nodeDoubleClicked,
          this, [this](pipeline::Node* node) {
            if (node && node->hasPropertiesWidget()) {
              auto* dlg = new pipeline::NodeEditDialog(
                node, pipeline(), this);
              dlg->setAttribute(Qt::WA_DeleteOnClose);
              dlg->setWindowTitle(
                QString("Edit - %1").arg(node->label()));
              dlg->show();
            }
          });

  // Link validator: type-compatible, different nodes. If the target input
  // already has a link, the existing one will be replaced when the drag
  // completes.
  m_pipelineStrip->setLinkValidator(
    [](pipeline::OutputPort* from, pipeline::InputPort* to) -> bool {
      if (from->node() == to->node()) {
        return false;
      }
      if (!pipeline::isPortTypeCompatible(from->type(),
                                          to->acceptedTypes())) {
        return false;
      }
      if (!from->canAcceptLink(to)) {
        return false;
      }
      return true;
    });

  // Create the link when the user completes a drag between ports.
  // If the destination is a TransformNode with a custom properties UI and all
  // its inputs are now connected, show the dialog before executing.  Cancel
  // removes the newly created link; OK/Apply executes the pipeline.
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::linkRequested,
          this, [this](pipeline::OutputPort* from, pipeline::InputPort* to) {
            auto* p = pipeline();
            if (!p) {
              return;
            }

            // If the target already has a link (e.g. stealing a sink into
            // a group), remove the existing link first.
            if (to->link()) {
              p->removeLink(to->link());
            }

            auto* link = p->createLink(from, to);
            if (!link) {
              return;
            }

            // Check if all inputs on the destination node are connected.
            // Don't execute until they are.
            auto* destNode = to->node();
            bool allConnected = true;
            for (auto* input : destNode->inputPorts()) {
              if (!input->link()) {
                allConnected = false;
                break;
              }
            }

            if (!allConnected) {
              return;
            }

            // Auto-open the edit dialog only for nodes the user just
            // dropped — i.e. NodeState::New. Reconnecting an existing
            // transform (Stale/Current), or wiring up a template-loaded
            // transform whose parameters are already set, should not
            // pop the dialog.
            if (destNode && destNode->hasPropertiesWidget() &&
                destNode->state() == pipeline::NodeState::New) {
              auto* dialog = new pipeline::NodeEditDialog(
                destNode, p, this);
              connect(dialog, &QDialog::rejected, this,
                      [p, link]() { p->removeLink(link); });
              dialog->setAttribute(Qt::WA_DeleteOnClose);
              dialog->show();
              return;
            }

            p->execute();
          });

  // Leave-group: relink the member to the group's upstream port
  connect(m_pipelineStrip, &pipeline::PipelineStripWidget::leaveGroupRequested,
          this, &MainWindow::leaveGroup);

  // Context menu on links: delete action
  m_pipelineStrip->setLinkMenuProvider(
    [this](pipeline::Link* link, QMenu& menu) {
      auto* action = menu.addAction("Delete Link", [link]() {
        auto* p = qobject_cast<pipeline::Pipeline*>(link->parent());
        if (p) {
          p->removeLink(link);
        }
      });
      if (pipeline() && pipeline()->isExecuting()) {
        action->setEnabled(false);
      }
    });

  // Context menu on nodes: type-specific actions
  m_pipelineStrip->setNodeMenuProvider(
    [this](pipeline::Node* node, QMenu& menu) {
      auto* p = pipeline();
      if (!p) {
        return;
      }
      bool locked = p->isExecuting();

      // SinkNode not inside a group: offer "Create Group"
      auto* sink = qobject_cast<pipeline::SinkNode*>(node);
      bool inGroup = false;
      if (sink) {
        for (auto* inPort : sink->inputPorts()) {
          if (inPort->link() &&
              qobject_cast<pipeline::SinkGroupNode*>(
                inPort->link()->from()->node())) {
            inGroup = true;
            break;
          }
        }
      }
      if (sink && !inGroup) {
        auto* action = menu.addAction("Create Group", [p, sink]() {
          // Create a SinkGroupNode with matching port types.
          auto* group = new pipeline::SinkGroupNode();
          for (auto* inPort : sink->inputPorts()) {
            // Use the first accepted type flag as the passthrough type.
            pipeline::PortType pt = pipeline::PortType::ImageData;
            for (auto t : pipeline::kAllPortTypes) {
              if (inPort->acceptedTypes().testFlag(t)) {
                pt = t;
                break;
              }
            }
            group->addPassthrough(inPort->name(), pt);
          }
          p->addNode(group);

          // Relink: route sink through the group. If the sink had an
          // upstream connection, break it and reconnect via the group.
          for (int i = 0; i < sink->inputPorts().size(); ++i) {
            auto* sinkInput = sink->inputPorts()[i];
            if (sinkInput->link() && i < group->inputPorts().size()) {
              auto* upstream = sinkInput->link()->from();
              p->removeLink(sinkInput->link());
              p->createLink(upstream, group->inputPorts()[i]);
            }
            // Always connect the sink to the group's output.
            if (i < group->outputPorts().size()) {
              p->createLink(group->outputPorts()[i], sinkInput);
            }
          }
          p->execute();
        });
        if (locked) {
          action->setEnabled(false);
        }
      }

      // Node inside a group: offer "Leave Group"
      for (auto* inPort : node->inputPorts()) {
        if (!inPort->link()) {
          continue;
        }
        auto* group = qobject_cast<pipeline::SinkGroupNode*>(
          inPort->link()->from()->node());
        if (!group) {
          continue;
        }
        auto* action = menu.addAction("Leave Group", [p, node, group]() {
          for (auto* inp : node->inputPorts()) {
            if (!inp->link()) {
              continue;
            }
            auto* groupPort = inp->link()->from();
            if (groupPort->node() != group) {
              continue;
            }
            int idx = group->outputPorts().indexOf(groupPort);
            pipeline::OutputPort* upstream = nullptr;
            if (idx >= 0 && idx < group->inputPorts().size()) {
              auto* gi = group->inputPorts()[idx];
              if (gi->link()) {
                upstream = gi->link()->from();
              }
            }
            p->removeLink(inp->link());
            if (upstream) {
              p->createLink(upstream, inp);
            }
            break;
          }
          p->execute();
        });
        if (locked) {
          action->setEnabled(false);
        }
        break;
      }

      // Delete node
      auto* deleteAction =
        menu.addAction("Delete", [p, node]() { p->removeNode(node); });
      if (locked) {
        deleteAction->setEnabled(false);
      }

      // Save this node's own persisted ports. Kept last, in its own
      // trailing section, so it reads as a separate kind of action from
      // the graph edits above it.
      if (qobject_cast<pipeline::SourceNode*>(node) ||
          qobject_cast<pipeline::TransformNode*>(node)) {
        addSaveDataAction(menu, node, this, !locked);
      }
    });

  // Context menu on ports: per-port persistence override, grouped
  // under a "Persistency" submenu so the top-level menu has room for
  // upcoming port-level actions.
  m_pipelineStrip->setPortMenuProvider(
    [this](pipeline::OutputPort* port, QMenu& menu) {
      if (!port) {
        return;
      }
      // Passthrough ports (sink-group outputs) forward upstream data
      // and pin isPersistent() to false; a persistence choice here
      // would silently do nothing, so don't offer one.
      if (qobject_cast<pipeline::PassthroughOutputPort*>(port)) {
        return;
      }
      auto* p = pipeline();
      auto* persistencyMenu = menu.addMenu(QStringLiteral("Persistency"));
      auto addModeAction =
        [persistencyMenu, port, p](const QString& iconPath,
                                    const QString& label, bool persistent,
                                    pipeline::PersistenceMode mode) {
          auto* action = persistencyMenu->addAction(
            QIcon(iconPath), label, [port, persistent, mode, p]() {
              // Enabling persistence: set the medium first so the
              // single reconcile triggered by setPersistent runs with
              // the target mode already in place. Disabling: skip the
              // mode entirely (transient ports ignore it, and setting
              // it first would trigger an unnecessary InMemory-branch
              // reload on a port that we're about to evict anyway).
              if (persistent) {
                port->setPersistenceMode(mode);
                port->setPersistent(true);
              } else {
                port->setPersistent(false);
              }
              // If the user just asked to retain this port's data but
              // the data was already deallocated (e.g. previously
              // transient and consumed), the only way to obtain it is
              // to re-run the producer. Trigger that here at the UI
              // layer so the pipeline core stays free of policy.
              if (persistent && p && port->node() &&
                  !port->hasData()) {
                p->execute(port->node());
              }
            });
          action->setCheckable(true);
          // Mark the currently-active mode so the user sees what's set.
          bool isCurrent =
            (port->isPersistent() == persistent) &&
            (!persistent || port->persistenceMode() == mode);
          action->setChecked(isCurrent);
        };
      addModeAction(QStringLiteral(":/pipeline/port_persistent_ram.svg"),
                    QStringLiteral("Persist in Memory"), true,
                    pipeline::PersistenceMode::InMemory);
      addModeAction(QStringLiteral(":/pipeline/port_persistent_disk.svg"),
                    QStringLiteral("Persist on Disk"), true,
                    pipeline::PersistenceMode::OnDisk);
      addModeAction(QStringLiteral(":/pipeline/port_transient.svg"),
                    QStringLiteral("Transient"), false,
                    pipeline::PersistenceMode::InMemory);

      addSaveDataAction(menu, port, this, !(p && p->isExecuting()));
    });

  // Sync tip output port from ActiveObjects to the strip widget and colormap
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeTipOutputPortChanged,
          m_pipelineStrip,
          &pipeline::PipelineStripWidget::setTipOutputPort);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeTipOutputPortChanged,
          this, [this](pipeline::OutputPort* port) {
            disconnect(m_tipDataChangedConn);
            disconnect(m_tipMetadataChangedConn);
            if (port) {
              m_tipDataChangedConn = connect(
                port, &pipeline::OutputPort::dataChanged,
                this, &MainWindow::scheduleColorMapDisplayUpdate);
              m_tipMetadataChangedConn = connect(
                port, &pipeline::OutputPort::metadataChanged,
                this, &MainWindow::scheduleColorMapDisplayUpdate);
            }
            updateColorMapDisplay();
          });

  // Create the single application pipeline
  initPipeline();

  // connect quit.
  connect(m_ui->actionExit, &QAction::triggered, this, &MainWindow::close);

  // Panel switching is now handled by onNodeSelected() / onPortSelected()
  // which are connected to the PipelineStripWidget signals above.

  // Connect the about dialog up too.
  connect(m_ui->actionAbout, &QAction::triggered, this,
          [this]() { openDialog<AboutDialog>(&m_aboutDialog); });

  // And connect the read the docs page
  connect(m_ui->actionReadTheDocs, &QAction::triggered, this,
          &MainWindow::openReadTheDocs);

  // Instantiate tomviz application behavior.
  new Behaviors(this);

  new LoadDataReaction(m_ui->actionOpen);

  new LoadStackReaction(m_ui->actionStack);

  new LoadTimeSeriesReaction(m_ui->actionOpenTimeSeries);

  new DataBrokerLoadReaction(m_ui->actionImportFromDataBroker);

  auto dataBrokerSaveReaction =
    new DataBrokerSaveReaction(m_ui->actionExportToDataBroker, this);

  // Sources menu
  auto pyXRFRunner = new PyXRFRunner(this);
  connect(m_ui->actionPyXRFSource, &QAction::triggered, pyXRFRunner,
          &PyXRFRunner::start);
  auto ptychoRunner = new PtychoRunner(this);
  connect(m_ui->actionPtychoSource, &QAction::triggered, ptychoRunner,
          &PtychoRunner::start);

  // Build Data Transforms menu
  new DataTransformMenu(this, m_ui->menuData, m_ui->menuSegmentation);

  // Create the custom transforms menu
  m_customTransformsMenu = new QMenu("Custom Transforms", this);
  m_ui->menubar->insertMenu(m_ui->menuModules->menuAction(),
                            m_customTransformsMenu);
  connect(m_customTransformsMenu, &QMenu::aboutToShow, this,
          [this]() { registerCustomOperators(findCustomOperators()); });

  // Create the pipeline templates menu
  m_pipelineTemplates = new QMenu("Pipeline templates", this);
  m_ui->menubar->insertMenu(m_ui->menuModules->menuAction(),
                            m_pipelineTemplates);
  // Populate the menu with templates
  findPipelineTemplates();

  // Build Tomography menu
  // ################################################################
  QAction* setVolumeDataTypeAction =
    m_ui->menuTomography->addAction("Set Data Type");
  QAction* setTiltDataTypeAction =
    m_ui->menuTomography->addAction("Set Data Type");
  // QAction* setFibDataTypeAction =
  //   m_ui->menuTomography->addAction("Set Data Type");
  m_ui->menuTomography->addSeparator();

  QAction* setTiltAnglesAction =
    m_ui->menuTomography->addAction("Set Tilt Angles");
  m_ui->menuTomography->addSeparator();

  // === Pre-processing submenu ===
  QMenu* preprocessingMenu = m_ui->menuTomography->addMenu("Pre-processing");
  QAction* downsampleByTwoAction =
    preprocessingMenu->addAction("Bin Tilt Images x2");
  QAction* removeBadPixelsAction =
    preprocessingMenu->addAction("Remove Bad Pixels");
  QAction* gaussianFilterAction =
    preprocessingMenu->addAction("Gaussian Filter");
  QAction* autoSubtractBackgroundAction =
    preprocessingMenu->addAction("Background Subtraction (Auto)");
  QAction* subtractBackgroundAction =
    preprocessingMenu->addAction("Background Subtraction (Manual)");
  QAction* normalizationAction =
    preprocessingMenu->addAction("Normalize Average Image Intensity");
  QAction* gradientMagnitude2DSobelAction =
    preprocessingMenu->addAction("2D Gradient Magnitude");
  QAction* ctfCorrectAction =
    preprocessingMenu->addAction("CTF Correction");

  // === Alignment submenu ===
  QMenu* alignmentMenu = m_ui->menuTomography->addMenu("Alignment");
  QAction* autoAlignCCAction = alignmentMenu->addAction(
    "Image Alignment (Auto: Cross Correlation)");
  QAction* autoAlignCOMAction =
    alignmentMenu->addAction("Image Alignment (Auto: Center of Mass)");
  QAction* autoAlignPyStackRegAction =
    alignmentMenu->addAction("Image Alignment (Auto: PyStackReg)");
  QAction* alignAction =
    alignmentMenu->addAction("Image Alignment (Manual)");
  alignmentMenu->addSeparator();
  QAction* autoRotateAlignAction =
    alignmentMenu->addAction("Tilt Axis Rotation Alignment (Auto)");
  QAction* autoRotateAlignShiftAction =
    alignmentMenu->addAction("Tilt Axis Shift Alignment (Auto)");
  QAction* rotateAlignAction =
    alignmentMenu->addAction("Tilt Axis Alignment (Manual)");
  QAction* shiftRotationCenterAction =
    alignmentMenu->addAction("Shift Rotation Center (Manual)");

  // === Reconstruction submenu ===
  QMenu* reconstructionMenu = m_ui->menuTomography->addMenu("Reconstruction");
  QAction* reconDFMAction =
    reconstructionMenu->addAction("Direct Fourier Method");
  QAction* reconWBPAction =
    reconstructionMenu->addAction("Weighted Back Projection");
  QAction* reconWBP_CAction =
    reconstructionMenu->addAction("Simple Back Projection (C++)");
  QAction* reconARTAction = reconstructionMenu->addAction(
    "Algebraic Reconstruction Technique (ART)");
  QAction* reconSIRTAction = reconstructionMenu->addAction(
    "Simultaneous Iterative Recon. Technique (SIRT)");
  QAction* reconDFMConstraintAction =
    reconstructionMenu->addAction("Constraint-based Direct Fourier Method");
  QAction* reconTVMinimizationAction =
    reconstructionMenu->addAction("TV Minimization Method");
  QAction* reconTomoPyGridRecAction =
    reconstructionMenu->addAction("TomoPy Reconstruction");

  // === Simulation submenu ===
  QMenu* simulationMenu =
    m_ui->menuTomography->addMenu("Simulation && Demonstrations");
  QAction* generateTiltSeriesAction =
    simulationMenu->addAction("Project Tilt Series from Volume");
  QAction* randomShiftsAction =
    simulationMenu->addAction("Shift Tilt Series Randomly");
  QAction* reconRealTimeAction =
    simulationMenu->addAction("Initialize Real-Time Tomography");
  QAction* addPoissonNoiseAction =
    simulationMenu->addAction("Add Poisson Noise");

  // Set up reactions for Tomography Menu
  //#################################################################
  new SetDataTypeReaction(setVolumeDataTypeAction, this,
                          pipeline::PortType::Volume);
  new SetDataTypeReaction(setTiltDataTypeAction, this,
                          pipeline::PortType::TiltSeries);
  new SetTiltAnglesReaction(setTiltAnglesAction, this);

  new AddPythonTransformReaction(
    generateTiltSeriesAction, "Generate Tilt Series",
    readInPythonScript("GenerateTiltSeries"),
    readInJSONDescription("GenerateTiltSeries"));

  new AddAlignReaction(alignAction);
  new AddPythonTransformReaction(downsampleByTwoAction, "Bin Tilt Image x2",
                                 readInPythonScript("BinTiltSeriesByTwo"));
  new AddPythonTransformReaction(
    removeBadPixelsAction, "Remove Bad Pixels",
    readInPythonScript("RemoveBadPixelsTiltSeries"));
  new AddPythonTransformReaction(
    gaussianFilterAction, "Gaussian Filter Tilt Series",
    readInPythonScript("GaussianFilterTiltSeries"),
    readInJSONDescription("GaussianFilterTiltSeries"));
  new AddPythonTransformReaction(
    autoSubtractBackgroundAction, "Background Subtraction (Auto)",
    readInPythonScript("Subtract_TiltSer_Background_Auto"));
  new AddPythonTransformReaction(
    subtractBackgroundAction, "Background Subtraction (Manual)",
    readInPythonScript("Subtract_TiltSer_Background"));
  new AddPythonTransformReaction(normalizationAction, "Normalize Tilt Series",
                                 readInPythonScript("NormalizeTiltSeries"));
  new AddPythonTransformReaction(
    gradientMagnitude2DSobelAction, "Gradient Magnitude 2D",
    readInPythonScript("GradientMagnitude2D_Sobel"));
  new AddPythonTransformReaction(ctfCorrectAction, "CTF Correction",
                                 readInPythonScript("ctf_correct"),
                                 readInJSONDescription("ctf_correct"));
  new AddPythonTransformReaction(
    rotateAlignAction, "Tilt Axis Alignment (manual)",
    readInPythonScript("RotationAlign"),
    readInJSONDescription("RotationAlign"));
  new AddPythonTransformReaction(
    autoRotateAlignAction, "Auto Tilt Axis Align",
    readInPythonScript("AutoTiltAxisRotationAlignment"),
    readInJSONDescription("AutoTiltAxisRotationAlignment"));
  new AddPythonTransformReaction(
    autoRotateAlignShiftAction, "Auto Tilt Axis Shift Align",
    readInPythonScript("AutoTiltAxisShiftAlignment"),
    readInJSONDescription("AutoTiltAxisShiftAlignment"));

  new AddPythonTransformReaction(
    autoAlignCCAction, "Auto Tilt Image Align (XCORR)",
    readInPythonScript("AutoCrossCorrelationTiltImageAlignment"),
    readInJSONDescription("AutoCrossCorrelationTiltImageAlignment"));
  new AddPythonTransformReaction(
    autoAlignCOMAction, "Auto Tilt Image Align (CoM)",
    readInPythonScript("AutoCenterOfMassTiltImageAlignment"),
    readInJSONDescription("AutoCenterOfMassTiltImageAlignment"));
  new AddPythonTransformReaction(
    autoAlignPyStackRegAction, "Auto Tilt Image Align (PyStackReg)",
    readInPythonScript("PyStackRegImageAlignment"),
    readInJSONDescription("PyStackRegImageAlignment"));
  new AddPythonTransformReaction(
    shiftRotationCenterAction, "Shift Rotation Center",
    readInPythonScript("ShiftRotationCenter_tomopy"),
    readInJSONDescription("ShiftRotationCenter_tomopy"));

  new AddPythonTransformReaction(reconDFMAction, "Reconstruct (Direct Fourier)",
                                 readInPythonScript("Recon_DFT"),
                                 readInJSONDescription("Recon_DFT"));
  new AddPythonTransformReaction(reconWBPAction,
                                 "Reconstruct (Back Projection)",
                                 readInPythonScript("Recon_WBP"),
                                 readInJSONDescription("Recon_WBP"));
  new AddPythonTransformReaction(reconARTAction, "Reconstruct (ART)",
                                 readInPythonScript("Recon_ART"),
                                 readInJSONDescription("Recon_ART"));
  new AddPythonTransformReaction(reconSIRTAction, "Reconstruct (SIRT)",
                                 readInPythonScript("Recon_SIRT"),
                                 readInJSONDescription("Recon_SIRT"));
  new AddPythonTransformReaction(
    reconDFMConstraintAction, "Reconstruct (Constraint-based Direct Fourier)",
    readInPythonScript("Recon_DFT_constraint"),
    readInJSONDescription("Recon_DFT_constraint"));
  new AddPythonTransformReaction(
    reconTVMinimizationAction, "Reconstruct (TV Minimization)",
    readInPythonScript("Recon_TV_minimization"),
    readInJSONDescription("Recon_TV_minimization"));
  new AddPythonTransformReaction(
    reconTomoPyGridRecAction, "Reconstruct (TomoPy)",
    readInPythonScript("Recon_tomopy"),
    readInJSONDescription("Recon_tomopy"));

  new ReconstructionReaction(reconWBP_CAction);

  new AddPythonTransformReaction(
    randomShiftsAction, "Shift Tilt Series Randomly",
    readInPythonScript("ShiftTiltSeriesRandomly"),
    readInJSONDescription("ShiftTiltSeriesRandomly"));
  new AddPythonTransformReaction(
    reconRealTimeAction, "Initialize Real-Time Tomography",
    readInPythonScript("Recon_real_time_tomography"),
    readInJSONDescription("Recon_real_time_tomography"));
  new AddPythonTransformReaction(addPoissonNoiseAction, "Add Poisson Noise",
                                 readInPythonScript("AddPoissonNoise"),
                                 readInJSONDescription("AddPoissonNoise"));

  //#################################################################

  // Set up operator search dialog
  m_operatorSearchDialog = new OperatorSearchDialog(this);
  m_operatorSearchDialog->collectActionsFromMenu(m_ui->menuData,
                                                 "Data Transforms");
  m_operatorSearchDialog->collectActionsFromMenu(m_ui->menuSegmentation,
                                                 "Segmentation");
  m_operatorSearchDialog->collectActionsFromMenu(m_ui->menuTomography,
                                                 "Tomography");

  // Add "Search Operators..." to each operator menu (like ParaView does for
  // Sources, Filters, and Extractors)
  auto showSearch = [this]() {
    // Custom operators can change on disk without the menu ever having
    // been opened; a rescan only lists the directories and parses the
    // JSON descriptions, cheap enough here.
    registerCustomOperators(findCustomOperators());
    m_operatorSearchDialog->show();
    m_operatorSearchDialog->raise();
    m_operatorSearchDialog->activateWindow();
  };

  for (auto* menu :
       { m_ui->menuData, m_ui->menuSegmentation, m_ui->menuTomography }) {
    // Show "Ctrl+Space" text on all three, but don't set a real shortcut
    // to avoid ambiguity. The global shortcut below handles the actual key.
    auto* searchAction = new QAction("Search Operators...", menu);
    searchAction->setShortcut(QKeySequence("Ctrl+Space"));
    searchAction->setShortcutContext(Qt::WidgetShortcut);
    connect(searchAction, &QAction::triggered, this, showSearch);
    QAction* firstAction = menu->actions().isEmpty() ? nullptr
                                                     : menu->actions().first();
    menu->insertAction(firstAction, searchAction);
    menu->insertSeparator(firstAction);
  }

  // Global shortcut for Ctrl+Space to open the search dialog
  auto* globalSearchShortcut = new QShortcut(QKeySequence("Ctrl+Space"), this);
  connect(globalSearchShortcut, &QShortcut::activated, this, showSearch);

  new PipelineModuleMenu(m_ui->modulesToolbar, m_ui->menuModules, this);
  new RecentFilesMenu(*m_ui->menuRecentlyOpened, m_ui->menuRecentlyOpened);

  new SaveDataReaction(m_ui->actionSaveData);
  new SaveScreenshotReaction(m_ui->actionSaveScreenshot, this);
  // The same dialog as the Animation Helper's Export Movie button, which
  // can write MP4 (ParaView's own exporter cannot in our packages)
  connect(m_ui->actionSaveMovie, &QAction::triggered, this,
          [this]() { MovieExportDialog::exportMovie(this); });
  // FIXME: staged for removal
  m_ui->actionSaveWeb->setVisible(false);

  new SaveLoadStateReaction(m_ui->actionLoadState, /*load*/ true);
  new SaveLoadStateReaction(m_ui->actionSaveStateAs);
  connect(m_ui->actionSaveState, &QAction::triggered, this,
          &MainWindow::saveState);

  connect(m_ui->actionLoadTemplate, &QAction::triggered, this,
          []() { SaveLoadTemplateReaction::loadTemplateWithDialog(); });
  connect(m_ui->actionSaveTemplateAs, &QAction::triggered, this,
          []() { SaveLoadTemplateReaction::saveTemplateAs(); });

  auto reaction = new ResetReaction(m_ui->actionReset);
  connect(m_ui->menu_File, &QMenu::aboutToShow, reaction,
          &ResetReaction::updateEnableState);

  new ViewMenuManager(this, m_ui->menuView);

  QMenu* sampleDataMenu = new QMenu("Sample Data", this);
  m_ui->menubar->insertMenu(m_ui->menuHelp->menuAction(), sampleDataMenu);
  QAction* userGuideAction = m_ui->menuHelp->addAction("User Guide");
  connect(userGuideAction, &QAction::triggered, this, &MainWindow::openUserGuide);
  QAction* introAction = m_ui->menuHelp->addAction("Intro to 3D Visualization");
  connect(introAction, &QAction::triggered, this, &MainWindow::openVisIntro);
  // Only show the bundled sample data entries if the data files are actually
  // present. The conda-forge package ships without them, while the packaged
  // installers (DMG/MSI) include them, so this lets a single binary work both
  // ways without a compile-time flag.
  QString dataDir =
    QApplication::applicationDirPath() + "/../share/tomviz/Data";
  bool haveRecon =
    QFileInfo(dataDir + "/Recon_NanoParticle_doi_10.1021-nl103400a.emd")
      .exists();
  bool haveTilt =
    QFileInfo(dataDir + "/TiltSeries_NanoParticle_doi_10.1021-nl103400a.emd")
      .exists();
  QAction* liveAcquisitionAction =
    sampleDataMenu->addAction("Simulated Live Acquisition");
  new AddPythonSourceReaction(
    liveAcquisitionAction, readInPythonScript("SimulatedLiveAcquisition"),
    readInJSONDescription("SimulatedLiveAcquisition"));
  sampleDataMenu->addSeparator();
  if (haveRecon) {
    QAction* reconAction =
      sampleDataMenu->addAction("Star Nanoparticle (Reconstruction)");
    connect(reconAction, &QAction::triggered, this, &MainWindow::openRecon);
  }
  if (haveTilt) {
    QAction* tiltAction =
      sampleDataMenu->addAction("Star Nanoparticle (Tilt Series)");
    connect(tiltAction, &QAction::triggered, this, &MainWindow::openTilt);
  }
  if (haveRecon || haveTilt) {
    sampleDataMenu->addSeparator();
  }
  QAction* constantDataAction =
    sampleDataMenu->addAction("Generate Constant Dataset");
  new AddPythonSourceReaction(constantDataAction,
                              readInPythonScript("ConstantDataset"),
                              readInJSONDescription("ConstantDataset"));
  QAction* randomParticlesAction =
    sampleDataMenu->addAction("Generate Random Particles");
  new AddPythonSourceReaction(randomParticlesAction,
                              readInPythonScript("RandomParticles"),
                              readInJSONDescription("RandomParticles"));
  QAction* probeShapeAction =
    sampleDataMenu->addAction("Generate Electron Beam Shape");
  new AddPythonSourceReaction(probeShapeAction,
                              readInPythonScript("STEM_probe"),
                              readInJSONDescription("STEM_probe"));
  sampleDataMenu->addSeparator();
  QAction* sampleDataLinkAction =
    sampleDataMenu->addAction("Download More Datasets");
  connect(sampleDataLinkAction, &QAction::triggered, this, &MainWindow::openDataLink);

  QAction* loadPaletteAction = m_ui->utilitiesToolbar->addAction(
    QIcon(":pqWidgets/Icons/pqPalette.svg"), "LoadPalette");
  new LoadPaletteReaction(loadPaletteAction);

  QToolButton* tb = qobject_cast<QToolButton*>(
    m_ui->utilitiesToolbar->widgetForAction(loadPaletteAction));
  if (tb) {
    tb->setPopupMode(QToolButton::InstantPopup);
  }

  CameraReaction::addAllActionsToToolBar(m_ui->utilitiesToolbar);
  AxesReaction::addAllActionsToToolBar(m_ui->utilitiesToolbar);

  ResetReaction::reset();

  // Add the acquisition client experimentally.
  m_ui->actionAcquisition->setEnabled(false);

  connect(m_ui->actionAcquisition, &QAction::triggered, this,
          [this]() { openDialog<AcquisitionWidget>(&m_acquisitionWidget); });

  connect(m_ui->actionAnimationHelper, &QAction::triggered, this, [this]() {
    openDialog<AnimationHelperDialog>(&m_animationHelperDialog);
  });
  // The Animation menu gathers what the .ui spreads over File, View and
  // a dock: the helper, the panel with the timeline, and the export.
  auto* animationPanel = m_ui->dockWidgetAnimation->toggleViewAction();
  animationPanel->setText(tr("Animation Panel"));
  m_ui->menuAnimation->insertAction(m_ui->actionSaveMovie, animationPanel);
  m_ui->menuAnimation->insertSeparator(m_ui->actionSaveMovie);

  // Prepopulate the previously seen python readers/writers
  // This operation is fast since it fetches the readers description
  // from the settings, without really invoking python
  FileFormatManager::instance().prepopulatePythonReaders();
  FileFormatManager::instance().prepopulatePythonWriters();

  // Initialize python synchronously (splash screen stays up until done)
  auto operators = initPython();

  m_ui->actionAcquisition->setEnabled(true);
  registerCustomOperators(operators);

  auto dataBroker = new DataBroker(this);
  m_ui->actionImportFromDataBroker->setEnabled(dataBroker->installed());
  m_ui->actionExportToDataBroker->setEnabled(
    dataBroker->installed() &&
    ActiveObjects::instance().activeNode() != nullptr);
  dataBrokerSaveReaction->setDataBrokerInstalled(dataBroker->installed());
  dataBroker->deleteLater();

  // The PyXRF and Ptycho python modules are deliberately not imported at
  // startup. Their runners check their requirements when the actions are
  // triggered, so broken optional dependencies cannot break launch.

  // Snapshot existing dock widgets before loading plugin dock widgets
  auto currentDocks = findChildren<QDockWidget*>();
  QSet<QDockWidget*> existingDocks(currentDocks.begin(), currentDocks.end());

  // Add plugin dock widgets when a plugin is loaded.
  new pqPluginDockWidgetsBehavior(this);

  // On first launch (no saved window state), ParaView plugin dock widgets
  // (e.g., Node Editor) appear visible by default. Hide any that were added
  // by plugins. pqPersistentMainWindowStateBehavior will restore their
  // visibility on subsequent launches if the user opened them.
  QTimer::singleShot(0, this, [this, existingDocks]() {
    QSettings* settings = pqApplicationCore::instance()->settings();
    if (!settings->contains("MainWindow/Geometry")) {
      for (auto* dock : findChildren<QDockWidget*>()) {
        if (!existingDocks.contains(dock)) {
          dock->hide();
        }
      }
    }
  });
}

MainWindow::~MainWindow()
{
  QString autosaveFile = getAutosaveFile();
  if (QFile::exists(autosaveFile) && !QFile::remove(autosaveFile)) {
    std::cerr << "Failed to remove autosave file." << std::endl;
  }
}

std::vector<OperatorDescription> MainWindow::initPython()
{
  Python::initialize();
  Connection::registerType();
  auto operators = findCustomOperators();
  FileFormatManager::instance().registerPythonReaders();
  FileFormatManager::instance().registerPythonWriters();

  return operators;
}

template <class T>
void MainWindow::openDialog(QWidget** dialog)
{
  if (*dialog == nullptr) {
    *dialog = new T(this);
  }
  (*dialog)->show();
}

void MainWindow::openFiles(int argc, char** argv)
{
  if (argc < 2) {
    return;
  }

  QString path(argv[argc - 1]);
  QFileInfo info(path);
  if (!info.exists()) {
    return;
  }

  if (info.isFile()) {
    if (info.suffix() == "tvsm" || info.suffix() == "tvh5") {
      SaveLoadStateReaction::loadState(info.canonicalFilePath());
    } else {
      LoadDataReaction::loadData(info.canonicalFilePath());
    }
  } else if (info.isDir()) {
    LoadStackReaction::loadData(info.canonicalFilePath());
  }
}

void MainWindow::openTilt()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/TiltSeries_NanoParticle_doi_10.1021-nl103400a.emd";
  QFileInfo info(path);
  if (info.exists()) {
    LoadDataReaction::loadData(info.canonicalFilePath());
  } else {
    QMessageBox::warning(
      this, "Sample Data not found",
      QString("The data file \"%1\" was not found.").arg(path));
  }
}

void MainWindow::openRecon()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/Data";
  path += "/Recon_NanoParticle_doi_10.1021-nl103400a.emd";
  QFileInfo info(path);
  if (info.exists()) {
    LoadDataReaction::loadData(info.canonicalFilePath());
  } else {
    QMessageBox::warning(
      this, "Sample Data not found",
      QString("The data file \"%1\" was not found.").arg(path));
  }
}

void MainWindow::openDataLink()
{
  QString link = "http://www.nature.com/articles/sdata201641";
  openUrl(link);
}

void MainWindow::openReadTheDocs()
{
  openHelpUrl();
}

void MainWindow::openUserGuide()
{
  QString path = QApplication::applicationDirPath() +
                 "/../share/tomviz/docs/TomvizBasicUserGuide.pdf";
  QFileInfo info(path);
  if (info.exists()) {
    QUrl userGuideUrl = QUrl::fromLocalFile(path);
    openUrl(userGuideUrl);
  } else {
    QMessageBox::warning(
      this, "User Guide not found",
      QString("The user guide \"%1\" was not found.").arg(path));
  }
}

void MainWindow::openVisIntro()
{
  QString link = "https://www.cambridge.org/core/journals/microscopy-today/"
                 "article/"
                 "tutorial-on-the-visualization-of-volumetric-data-using-"
                 "tomviz/55B58F40A16E96CDEB644202D9FD08BB";
  openUrl(link);
}

// Panel switching is now handled by onNodeSelected() / onPortSelected()

void MainWindow::showEvent(QShowEvent* e)
{
  QMainWindow::showEvent(e);
  if (m_isFirstShow) {
    m_isFirstShow = false;
    QTimer::singleShot(1, this, &MainWindow::onFirstWindowShow);
  }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
  if (e->mimeData()->hasUrls()) {
    e->acceptProposedAction();
  }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
  // Route local-file drags over any child of this window to the data
  // loading handlers above. Without this, whichever drop-accepting
  // child sits under the cursor claims the drag and refuses the files.
  // Other windows (e.g. the image stack dialog, which has its own drop
  // handling) are left alone, as are drags carrying no local files.
  auto type = event->type();
  if (type == QEvent::DragEnter || type == QEvent::DragMove ||
      type == QEvent::Drop) {
    auto* widget = qobject_cast<QWidget*>(watched);
    if (widget && widget != this && widget->window() == this) {
      auto* dropEvt = static_cast<QDropEvent*>(event);
      const auto* mime = dropEvt->mimeData();
      bool hasLocalFile = false;
      if (mime && mime->hasUrls()) {
        const auto urls = mime->urls();
        for (const auto& url : urls) {
          if (url.isLocalFile()) {
            hasLocalFile = true;
            break;
          }
        }
      }
      if (hasLocalFile) {
        if (type == QEvent::DragEnter) {
          dragEnterEvent(static_cast<QDragEnterEvent*>(event));
        } else if (type == QEvent::DragMove) {
          dropEvt->acceptProposedAction();
        } else {
          dropEvent(dropEvt);
        }
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::dropEvent(QDropEvent* e)
{
  for (const auto& url : e->mimeData()->urls()) {
    if (!url.isLocalFile()) {
      continue;
    }
    QString path = url.toLocalFile();
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "tvsm" || suffix == "tvh5") {
      SaveLoadStateReaction::loadState(path);
    } else {
      LoadDataReaction::loadData(path);
    }
  }
  e->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
  auto* activePipeline = ActiveObjects::instance().pipeline();
  if (activePipeline && activePipeline->isExecuting()) {
    QMessageBox::StandardButton response =
      QMessageBox::question(this, "Close tomviz?",
                            "You have transforms that are not completed "
                            "running in the background. These may not exit "
                            "cleanly. Are "
                            "you sure you want to try exiting anyway?");
    if (response == QMessageBox::No) {
      e->ignore();
      return;
    }
  } else if (activePipeline && !activePipeline->nodes().isEmpty()) {
    QMessageBox::StandardButton response =
      QMessageBox::question(this, "Close?", "Are you sure you want to exit?");
    if (response == QMessageBox::No) {
      e->ignore();
      return;
    }
  }
  // This is a little hackish, but we must ensure all PV proxy unregister calls
  // happen early enough in application destruction that the ParaView proxy
  // management code can still run without segfaulting.
  if (activePipeline) {
    activePipeline->clear();
  }
  e->accept();
}

bool MainWindow::checkOpenGL()
{
  // Check for required OpenGL version, and pop up a warning dialog if needed.
  QSurfaceFormat format;
  format.setVersion(3, 2);
  QOffscreenSurface offScreen;
  offScreen.setFormat(format);
  offScreen.create();
  QOpenGLContext context;
  context.setFormat(format);
  context.create();
  context.makeCurrent(&offScreen);
  if (!context.isValid() || context.format().version() < qMakePair(3, 2)) {
    QMessageBox::critical(this, "Tomviz",
                          "This system does not support OpenGL 3.2 or higher. "
                          "The application is unlikely to function correctly.");
    return false;
  }
  return true;
}

void MainWindow::onFirstWindowShow()
{
  QFile file(getAutosaveFile());
  if (!file.exists()) {
    QSettings* settings = pqApplicationCore::instance()->settings();
    bool showWelcome =
      settings->value("GeneralSettings.ShowWelcomeDialog", true).toBool();
    if (showWelcome) {
      QString path =
        QApplication::applicationDirPath() + "/../share/tomviz/Data";
      path += "/Recon_NanoParticle_doi_10.1021-nl103400a.emd";
      QFileInfo info(path);
      if (info.exists()) {
        WelcomeDialog welcomeDialog(this);
        welcomeDialog.setModal(true);
        welcomeDialog.exec();
      }
    }
    return;
  }
  QMessageBox::StandardButton response =
    QMessageBox::question(this, "Load autosave?",
                          "There is an autosave file present. Load it?",
                          QMessageBox::Yes | QMessageBox::No);
  if (response == QMessageBox::Yes) {
    SaveLoadStateReaction::loadState(getAutosaveFile());
  }
}

void MainWindow::autosave()
{
  SaveLoadStateReaction::saveState(getAutosaveFile(), false);
}

QString MainWindow::mostRecentStateFile() const
{
  return m_mostRecentStateFile;
}

void MainWindow::setMostRecentStateFile(const QString& fileName)
{
  if (m_mostRecentStateFile == fileName) {
    return;
  }
  m_mostRecentStateFile = fileName;
  updateSaveStateEnableState();
}

void MainWindow::updateSaveStateEnableState()
{
  m_ui->actionSaveState->setEnabled(!mostRecentStateFile().isEmpty());
}

void MainWindow::saveState()
{
  QString mostRecentFile = mostRecentStateFile();
  // Make sure there is a recent state file
  if (mostRecentFile.isEmpty()) {
    QString msg = "No recent state file to save to";
    qCritical() << msg;
    QMessageBox::critical(this, "Tomviz", msg);
    return;
  }

  // Write to a temporary file, or directly to the recent state
  // file if it does not exist for some reason.
  QString writeFile = mostRecentFile;
  if (QFile::exists(writeFile)) {
    // This will almost certainly be true
    // We need to keep the extension
    QFileInfo info(writeFile);
    QString ext = info.suffix();
    writeFile.chop(ext.size());
    writeFile += "tmp." + ext;
  }

  // Write the state file
  if (!SaveLoadStateReaction::saveState(writeFile, false)) {
    // Clean-up and return
    if (QFile::exists(writeFile))
      QFile::remove(writeFile);

    QString msg = "Failed to save the state file";
    qCritical() << msg;
    QMessageBox::critical(this, "Tomviz", msg);
    return;
  }

  if (writeFile == mostRecentFile) {
    // We wrote directly to the state file. We are done!
    return;
  }

  // Otherwise, move the most recent state file to back it up,
  // then move the tmp file into its place before removing it.
  QString oldFile = mostRecentFile;
  QFileInfo info(mostRecentFile);
  QString ext = info.suffix();
  oldFile.chop(ext.size());
  oldFile += "old." + ext;
  if (!QFile::rename(mostRecentFile, oldFile)) {
    QString msg = "Failed to move " + mostRecentFile + " to " + oldFile;
    qCritical() << msg;
    QMessageBox::critical(this, "Tomviz", msg);
    return;
  }

  if (!QFile::rename(writeFile, mostRecentFile)) {
    QString msg = "Failed to move " + writeFile + " to " + mostRecentFile;
    qCritical() << msg;
    QMessageBox::critical(this, "Tomviz", msg);
    return;
  }

  if (!QFile::remove(oldFile)) {
    QString msg = "Failed to remove " + oldFile;
    qCritical() << msg;
    QMessageBox::critical(this, "Tomviz", msg);
    return;
  }
}

void MainWindow::handleMessage(const QString&, int type)
{
  QDockWidget* dock = m_ui->dockWidgetMessages;
  if (!dock->isVisible() && (type == QtCriticalMsg || type == QtWarningMsg)) {
    // if dock is not visible, we always pop it up as a floating dialog. This
    // avoids causing re-renders which may cause more errors and more confusion.
    QRect rectApp = geometry();

    QRect rectDock(QPoint(0, 0), QSize(static_cast<int>(rectApp.width() * 0.4),
                                       dock->sizeHint().height()));
    rectDock.moveCenter(QPoint(
      rectApp.center().x(), rectApp.bottom() - dock->sizeHint().height() / 2));
    dock->setFloating(true);
    dock->setGeometry(rectDock);
    dock->show();
  }
  if (dock->isVisible()) {
    dock->raise();
  }
}

void MainWindow::registerCustomOperators(
  std::vector<OperatorDescription> operators)
{
  m_customTransformsMenu->clear();
  // The search dialog keeps its own list; the actions it knew are about
  // to be deleted, so it gets the new set below.
  const QString searchCategory = m_customTransformsMenu->title();
  if (m_operatorSearchDialog) {
    m_operatorSearchDialog->removeCategory(searchCategory);
  }

  auto* createAction = m_customTransformsMenu->addAction(tr("Create New..."));
  connect(createAction, &QAction::triggered, this,
          &MainWindow::createCustomOperator);
  auto* manageAction = m_customTransformsMenu->addAction(tr("Manage..."));
  connect(manageAction, &QAction::triggered, this,
          &MainWindow::manageCustomOperators);

  std::vector<const OperatorDescription*> sources;
  std::vector<const OperatorDescription*> transforms;
  for (const auto& op : operators) {
    if (op.type == OperatorDescription::Type::Source) {
      sources.push_back(&op);
    } else {
      transforms.push_back(&op);
    }
  }

  auto addEntry = [this, &searchCategory](const OperatorDescription& op) {
    // Plain actions, like every other menu: editing, deleting and cloning
    // live in the Manage dialog. A broken definition is listed disabled
    // so it can be found and fixed there.
    QAction* action = m_customTransformsMenu->addAction(op.label);
    action->setEnabled(op.valid);
    if (m_operatorSearchDialog) {
      // Registered before the files are read: the search shows the
      // action's live status tip, set below once the description is in.
      m_operatorSearchDialog->addOperatorAction(action, searchCategory);
    }
    if (!op.valid) {
      action->setStatusTip(
        tr("This definition is broken; fix it under %1 > Manage...")
          .arg(searchCategory));
    }
    if (!op.loadError.isNull()) {
      qWarning().noquote()
        << QString("An error occurred trying to load an operator from '%1':")
             .arg(op.pythonPath);
      qWarning().noquote() << op.loadError;
      return;
    } else if (!op.valid) {
      qWarning().noquote()
        << QString("'%1' doesn't contain a valid operator definition.")
             .arg(op.pythonPath);
      return;
    }

    QString source;
    QString json;
    QFile pythonFile(op.pythonPath);
    if (pythonFile.open(QIODevice::ReadOnly)) {
      source = pythonFile.readAll();
    } else {
      qCritical() << QString("Unable to read '%1'.").arg(op.pythonPath);
    }
    if (!op.jsonPath.isNull()) {
      QFile jsonFile(op.jsonPath);
      if (jsonFile.open(QIODevice::ReadOnly)) {
        json = jsonFile.readAll();
      } else {
        qCritical() << QString("Unable to read '%1'.").arg(op.jsonPath);
      }
    }

    if (op.type == OperatorDescription::Type::Source) {
      new AddPythonSourceReaction(action, source, json);
    } else {
      new AddPythonTransformReaction(action, op.label, source, json);
    }
    // The transform reaction sets the status tip from the description;
    // give sources the same so the search can show it.
    if (action->statusTip().isEmpty()) {
      action->setStatusTip(QJsonDocument::fromJson(json.toUtf8())
                             .object()
                             .value(QStringLiteral("description"))
                             .toString());
    }
  };

  if (!sources.empty()) {
    m_customTransformsMenu->addSection("Sources");
    for (const auto* op : sources) {
      addEntry(*op);
    }
  }
  if (!transforms.empty()) {
    m_customTransformsMenu->addSection("Transforms");
    for (const auto* op : transforms) {
      addEntry(*op);
    }
  }
}

std::vector<OperatorDescription> MainWindow::findCustomOperators()
{
  std::vector<OperatorDescription> operators;
  QSet<QString> scanned;
  auto scan = [&operators, &scanned](const QString& path, bool userOwned) {
    // The same directory can be named more than once (a symlink, or the
    // user directory repeated in the environment); scan it once.
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty() || scanned.contains(canonical)) {
      return;
    }
    scanned.insert(canonical);
    std::vector<OperatorDescription> ops = tomviz::findCustomOperators(path);
    for (auto& op : ops) {
      op.userOwned = userOwned;
    }
    operators.insert(operators.end(), ops.begin(), ops.end());
  };

  // The user's own directory is always scanned, whatever the environment
  // says: it is the one place tomviz edits and deletes operators in.
  const QString userDir = userDataPath();
  if (!userDir.isEmpty()) {
    scan(userDir, /*userOwned=*/true);
  }
  for (const QString& path : customOperatorSearchPaths()) {
    scan(path, /*userOwned=*/false);
  }

  // Sort so we get a consistent order each time we load
  std::sort(operators.begin(), operators.end(),
            [](const OperatorDescription& op1, const OperatorDescription& op2) {
              return op1.label < op2.label;
            });

  return operators;
}

void MainWindow::deleteCustomOperator(const OperatorDescription& op)
{
  QStringList files{ op.pythonPath };
  if (!op.jsonPath.isEmpty()) {
    files.append(op.jsonPath);
  }

  const auto answer = QMessageBox::question(
    this, tr("Delete Custom Node"),
    tr("Delete \"%1\"?\n\nThese files will be removed from disk:\n%2")
      .arg(op.label, files.join('\n')),
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer != QMessageBox::Yes) {
    return;
  }

  // Nodes already in a pipeline hold their own copy of the script and
  // description, so they are unaffected by the files going away.
  QStringList failed;
  for (const QString& file : files) {
    if (QFile::exists(file) && !QFile::remove(file)) {
      failed.append(file);
    }
  }
  if (!failed.isEmpty()) {
    QMessageBox::warning(
      this, tr("Delete Custom Node"),
      tr("These files could not be removed:\n%1").arg(failed.join('\n')));
  }
  // The menu rescans the directories the next time it opens; the
  // manager shows the change right away.
  if (m_customOperatorManager) {
    m_customOperatorManager->refresh();
  }
}

void MainWindow::showCustomOperatorDialog(CustomOperatorEditDialog* dialog)
{
  if (!dialog->loadError().isEmpty()) {
    QMessageBox::critical(this, tr("Custom Node"), dialog->loadError());
    delete dialog;
    return;
  }
  // The manager, when open, lists what is on disk; a save changes that.
  connect(dialog, &CustomOperatorEditDialog::saved, this, [this]() {
    if (m_customOperatorManager) {
      m_customOperatorManager->refresh();
    }
  });
  // Modeless, like the node editor: the pipeline stays usable while a
  // file is being edited. The menu rescans the files on its next open.
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::editCustomOperator(const OperatorDescription& op)
{
  showCustomOperatorDialog(
    new CustomOperatorEditDialog(op.label, op.pythonPath, op.jsonPath, this));
}

void MainWindow::manageCustomOperators()
{
  if (!m_customOperatorManager) {
    auto* dialog = new CustomOperatorManagerDialog(
      []() { return findCustomOperators(); }, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &CustomOperatorManagerDialog::createRequested, this,
            &MainWindow::createCustomOperator);
    connect(dialog, &CustomOperatorManagerDialog::cloneRequested, this,
            &MainWindow::cloneCustomOperator);
    connect(dialog, &CustomOperatorManagerDialog::editRequested, this,
            &MainWindow::editCustomOperator);
    connect(dialog, &CustomOperatorManagerDialog::deleteRequested, this,
            &MainWindow::deleteCustomOperator);
    connect(dialog, &CustomOperatorManagerDialog::openDirectoryRequested,
            this, &MainWindow::openCustomOperatorDirectory);
    m_customOperatorManager = dialog;
  } else {
    m_customOperatorManager->refresh();
  }
  m_customOperatorManager->show();
  m_customOperatorManager->raise();
  m_customOperatorManager->activateWindow();
}

void MainWindow::openCustomOperatorDirectory(const OperatorDescription& op)
{
  QDesktopServices::openUrl(
    QUrl::fromLocalFile(QFileInfo(op.pythonPath).absolutePath()));
}

void MainWindow::createCustomOperator()
{
  const QString directory = userDataPath();
  if (directory.isEmpty()) {
    return; // userDataPath() has already told the user why
  }
  // The shipped template, named so it doesn't collide with an earlier
  // "new" operator still sitting under its default name.
  int number = 1;
  const QString stem = uniqueOperatorStem(
    directory, QStringLiteral("NewCustomTransform"), &number);
  const QString label = number > 1
                          ? tr("New Custom Transform %1").arg(number)
                          : tr("New Custom Transform");
  CustomOperatorDraft draft;
  draft.directory = directory;
  draft.stem = stem;
  draft.script = readInPythonScript("NewCustomTransform");
  draft.description = withDescriptionIdentity(
    readInJSONDescription("NewCustomTransform"), stem, label);
  showCustomOperatorDialog(new CustomOperatorEditDialog(draft, this));
}

void MainWindow::cloneCustomOperator(const OperatorDescription& op)
{
  const QString directory = userDataPath();
  if (directory.isEmpty()) {
    return; // userDataPath() has already told the user why
  }

  auto read = [this](const QString& path, QString* text) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::critical(this, tr("Clone Custom Node"),
                            tr("Could not read \"%1\":\n%2")
                              .arg(QDir::toNativeSeparators(path),
                                   file.errorString()));
      return false;
    }
    *text = QString::fromUtf8(file.readAll());
    return true;
  };

  CustomOperatorDraft draft;
  draft.directory = directory;
  if (!read(op.pythonPath, &draft.script)) {
    return;
  }
  if (!op.jsonPath.isEmpty() && !read(op.jsonPath, &draft.description)) {
    return;
  }
  // The copy is told apart from its original in both the file name and
  // the menu, and from earlier copies by a number.
  int number = 1;
  draft.stem = uniqueOperatorStem(
    directory,
    QFileInfo(op.pythonPath).completeBaseName() + QStringLiteral("_copy"),
    &number);
  draft.description = markDescriptionAsCopy(draft.description, number);
  showCustomOperatorDialog(new CustomOperatorEditDialog(draft, this));
}

void MainWindow::setEnabledPythonConsole(bool enabled)
{
  m_ui->dockWidgetPythonConsole->setEnabled(enabled);
}

void MainWindow::onMouseOverVoxel(const vtkVector3i& ijk, double v)
{

  statusBar()->showMessage(
    QString("(%1, %2, %3) : %4").arg(ijk[0]).arg(ijk[1]).arg(ijk[2]).arg(v),
    5000);
}

void MainWindow::findPipelineTemplates() {
  m_pipelineTemplates->clear();

  // Always include the bundled 'share' directory.
  QList<QDir> locations;
  locations.append(QDir(QApplication::applicationDirPath() +
                        "/../share/tomviz/templates/"));

  QByteArray envOverride = qgetenv("TOMVIZ_PIPELINE_TEMPLATES_PATH");
  if (!envOverride.isEmpty()) {
    for (const QString& path : QString::fromLocal8Bit(envOverride)
                                 .split(QDir::listSeparator(),
                                        Qt::SkipEmptyParts)) {
      if (QFileInfo(path).isDir()) {
        locations.append(QDir(path));
      }
    }
  } else {
    locations.append(QDir(tomviz::userTemplatesPath()));
  }

  foreach (QDir dir, locations) {
    foreach (QFileInfo file, dir.entryInfoList()) {
      if (file.isFile() && file.suffix() == "tvsm") {
        QString menuName = file.completeBaseName().replace("_", " ");
        QAction* action = m_pipelineTemplates->addAction(menuName);
        new SaveLoadTemplateReaction(action, true, file.absoluteFilePath());
      }
    }
  }
  m_pipelineTemplates->addSeparator();
  QAction* actionSaveTemplate = m_pipelineTemplates->addAction("Save Template");
  new SaveLoadTemplateReaction(actionSaveTemplate);
  connect(actionSaveTemplate, &QAction::triggered, this, &MainWindow::findPipelineTemplates);
}

void MainWindow::initPipeline()
{
  auto* p = new pipeline::Pipeline(this);
  p->setExecutor(new pipeline::ThreadedExecutor(p));
  // Owns the per-node periodic-execution timers; parented to the pipeline
  // so its lifetime (and its worker thread's) tracks it exactly.
  new pipeline::AutoExecuteController(p, p);
  m_pipeline = p;
  m_pipelineStrip->setPipeline(p);
  m_pipelineControls->setPipeline(p);
  ActiveObjects::instance().setPipeline(p);
  // The animations recorded with the camera viewpoints follow the
  // viewpoints and the authored animations from here on
  RecordedAnimations::instance().install();
  // Rewinds a finished animation before it plays again; see the class.
  new AnimationSceneGuard(this);
  m_progressDialogManager = new ProgressDialogManager(this);
  m_progressDialogManager->setPipeline(p);

  // Wire renderNeeded() → pqView::render() for all sink nodes.
  // pqView::render() coalesces multiple calls via an internal timer
  // (pqView.h: "Multiple calls are collapsed into one."), matching the old
  // Module → ModuleManager::render() → pqView::render() pattern.
  auto connectSinkRender = [](pipeline::Node* node) {
    auto* sink = dynamic_cast<pipeline::LegacyModuleSink*>(node);
    if (sink && sink->view()) {
      auto* pqview = tomviz::convert<pqView*>(sink->view());
      if (pqview) {
        connect(sink, &pipeline::LegacyModuleSink::renderNeeded,
                pqview, &pqView::render);
      }
    }
  };
  connect(p, &pipeline::Pipeline::nodeAdded, this, connectSinkRender);

  // Render all views when a port pushes intermediate data (live updates).
  connect(p, &pipeline::Pipeline::nodeAdded, this,
          [](pipeline::Node* node) {
            for (auto* port : node->outputPorts()) {
              connect(port, &pipeline::OutputPort::intermediateDataApplied,
                      []() { pqApplicationCore::instance()->render(); });
            }
          });

  // Initialize/rescale color maps on intermediate updates so the
  // volume renders correctly during the first execution and across
  // re-runs. Safe under setIntermediateData's BQC — the worker is
  // paused while we mutate SM proxies. Sink-side rebinding is in
  // LegacyModuleSink::postConsume.
  connect(p, &pipeline::Pipeline::nodeAdded, this,
          [this](pipeline::Node* node) {
            for (auto* port : node->outputPorts()) {
              if (!pipeline::isVolumeType(port->type())) {
                continue;
              }
              connect(port, &pipeline::OutputPort::intermediateDataApplied,
                      this, [this, node, port]() {
                        if (ensureColorMapForPort(node, port)) {
                          updateColorMapDisplay();
                        }
                      });
            }
          });

  // Select newly added nodes so the tip output port updates
  connect(p, &pipeline::Pipeline::nodeAdded, this,
          &MainWindow::onNodeSelected);

  // Per-node rescale of existing color maps. rescaleColorMap() pushes
  // SM proxy state through the ParaView session, which is not
  // thread-safe against the worker thread's node execution. The
  // ThreadedExecutor parks its worker at a per-node barrier until this
  // handler returns (see ExecutionWorker::run), so no operator runs
  // concurrently with the rescale below — this previously crashed with
  // a SIGBUS inside vtkSMProxy::UpdateVTKObjects.
  connect(p->executor(), &pipeline::PipelineExecutor::nodeExecutionFinished,
          this, [](pipeline::Node* node, bool success) {
    if (!success) {
      return;
    }
    for (auto* port : node->outputPorts()) {
      if (!pipeline::isVolumeType(port->type()) || !port->hasData()) {
        continue;
      }
      // A LabelMap's transfer functions carry one band per label, in
      // data coordinates. Rescaling them to the data range is at best a
      // no-op and, once the label set shrinks, actively wrong: the
      // bands would be stretched onto values no label holds.
      if (port->type() == pipeline::PortType::LabelMap) {
        continue;
      }
      pipeline::VolumeDataPtr vol;
      try {
        vol = port->data().value<pipeline::VolumeDataPtr>();
      } catch (const std::bad_any_cast&) {
        continue;
      }
      if (vol && vol->hasColorMap()) {
        vol->rescaleColorMap();
      }
    }
  });

  // Backstop for the per-port intermediate path: covers nodes whose
  // first intermediate never fires (no progress.data) and the first
  // run of a freshly loaded pipeline. Topological order makes sure
  // upstream color maps exist before downstream's copyColorMapFrom.
  connect(p, &pipeline::Pipeline::executionFinished, this, [this, p]() {
    bool anyNewColorMaps = false;
    for (auto* node : p->topologicalSort()) {
      for (auto* port : node->outputPorts()) {
        if (ensureColorMapForPort(node, port)) {
          anyNewColorMaps = true;
        }
      }
    }

    // Refresh sinks that skipped updateColorMap during execution
    // (proxy didn't exist yet) plus the central histogram widget.
    if (anyNewColorMaps) {
      for (auto* node : p->nodes()) {
        auto* sink = dynamic_cast<pipeline::LegacyModuleSink*>(node);
        if (sink && sink->isColorMapNeeded()) {
          sink->updateColorMap();
        }
      }
      updateColorMapDisplay();
    }
  });

  // Lock all mutation UI while the pipeline is executing.
  connect(p, &pipeline::Pipeline::executionStarted,
          this, [this]() { setPipelineMutationEnabled(false); });
  connect(p, &pipeline::Pipeline::executionFinished,
          this, [this]() { setPipelineMutationEnabled(true); });
}

void MainWindow::setPipelineMutationEnabled(bool enabled)
{
  m_pipelineStrip->setInteractionLocked(!enabled);
  m_ui->menuData->setEnabled(enabled);
  m_ui->menuTomography->setEnabled(enabled);
  m_ui->menuSegmentation->setEnabled(enabled);
  m_ui->menuModules->setEnabled(enabled);
  if (m_customTransformsMenu) {
    m_customTransformsMenu->setEnabled(enabled);
  }
  if (m_pipelineTemplates) {
    m_pipelineTemplates->setEnabled(enabled);
  }
}

pipeline::Pipeline* MainWindow::pipeline() const
{
  return m_pipeline;
}

void MainWindow::onNodeSelected(pipeline::Node* node)
{
  // Forward to ActiveObjects; mutual exclusion of node/port/link is
  // enforced there. Properties panel, strip sync, and colormap are
  // handled by onActiveNodeChanged / onActivePortChanged.
  ActiveObjects::instance().setActiveNode(node);
}

void MainWindow::onPortSelected(pipeline::OutputPort* port)
{
  ActiveObjects::instance().setActivePort(port);
}

void MainWindow::onLinkSelected(pipeline::Link* link)
{
  ActiveObjects::instance().setActiveLink(link);
}

void MainWindow::clearDynamicPropertiesWidget()
{
  if (m_dynamicPropertiesWidget) {
    auto* w = m_dynamicPropertiesWidget.data();
    m_dynamicPropertiesWidget = nullptr;
    m_ui->propertiesPanelStackedWidget->removeWidget(w);
    w->deleteLater();
  }
}


void MainWindow::showPropertiesPanel(QWidget* content, const QString& title)
{
  auto* container = new QWidget(m_ui->propertiesPanelStackedWidget);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* titleLabel = new QLabel(title, container);
  QFont f = titleLabel->font();
  f.setBold(true);
  titleLabel->setFont(f);
  titleLabel->setContentsMargins(8, 6, 8, 6);
  layout->addWidget(titleLabel);

  auto* separator = new QFrame(container);
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  layout->addWidget(separator);

  // A node with no editable properties (e.g. some sources) still gets a
  // title — just no body.
  if (content) {
    // Wrap in a scroll area so the dock can be dragged down small, the
    // way the pipeline dock can. Added directly, the content widget's
    // minimum height becomes the stacked widget's, which becomes the
    // dock's floor — properties widgets are tall, so the splitter stops
    // well before the pipeline's does.
    //
    // Content that already scrolls itself is added as-is. Wrapping it
    // again would pull its pinned rows into a scrolling viewport —
    // NodePropertiesPanel deliberately keeps Apply below its inner
    // scroll area, and that has to stay put as the body scrolls.
    bool scrollsItself =
      qobject_cast<QScrollArea*>(content) != nullptr ||
      content->property("providesOwnScrolling").toBool();
    if (scrollsItself) {
      content->setParent(container);
      layout->addWidget(content, 1);
    } else {
      auto* scroll = new QScrollArea(container);
      scroll->setObjectName("propertiesScroll");
      scroll->setWidgetResizable(true);
      scroll->setFrameShape(QFrame::NoFrame);
      content->setParent(scroll);
      scroll->setWidget(content);
      // setWidget() turns autoFillBackground on, which would make these
      // panels paint their own palette background where they used to
      // inherit one. Has to follow setWidget() rather than precede it.
      content->setAutoFillBackground(false);
      layout->addWidget(scroll, 1);
    }
  } else {
    layout->addStretch(1);
  }

  m_dynamicPropertiesWidget = container;
  m_ui->propertiesPanelStackedWidget->addWidget(container);
  m_ui->propertiesPanelStackedWidget->setCurrentWidget(container);
}

void MainWindow::leaveGroup(pipeline::Node* member,
                            pipeline::SinkGroupNode* group)
{
  auto* p = pipeline();
  if (!p) {
    return;
  }
  for (auto* inPort : member->inputPorts()) {
    if (!inPort->link()) {
      continue;
    }
    auto* groupPort = inPort->link()->from();
    if (groupPort->node() != group) {
      continue;
    }
    // Find the upstream port feeding the group's matching input.
    int idx = group->outputPorts().indexOf(groupPort);
    pipeline::OutputPort* upstream = nullptr;
    if (idx >= 0 && idx < group->inputPorts().size()) {
      auto* groupInput = group->inputPorts()[idx];
      if (groupInput->link()) {
        upstream = groupInput->link()->from();
      }
    }
    p->removeLink(inPort->link());
    if (upstream) {
      p->createLink(upstream, inPort);
    }
    break;
  }
  p->execute();
}

void MainWindow::onActiveNodeChanged(pipeline::Node* node)
{
  m_pipelineStrip->setSelectedNode(node);
  disconnect(m_sinkColorMapChangedConn);
  disconnect(m_editingChangedConn);
  clearDynamicPropertiesWidget();

  if (!node) {
    m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
    return;
  }

  // Hide the properties panel while an edit dialog is open for the active
  // node, and restore it when the dialog is dismissed.
  //
  // Both NodeEditDialog and NodePropertiesPanel manage the
  // parametersApplied → execute() auto-wiring.  To avoid them stepping on
  // each other we must guarantee ordering:
  //   editing=true  → destroy the panel *immediately* so its destructor
  //                    restores auto-wiring before the dialog's constructor
  //                    re-disconnects it.
  //   editing=false → *defer* panel creation so the dialog's destructor
  //                    restores auto-wiring before the new panel suppresses it.
  m_editingChangedConn = connect(
    node, &pipeline::Node::editingChanged, this, [this](bool editing) {
      if (editing) {
        if (m_dynamicPropertiesWidget) {
          auto* w = m_dynamicPropertiesWidget.data();
          m_dynamicPropertiesWidget = nullptr;
          m_ui->propertiesPanelStackedWidget->removeWidget(w);
          delete w;
        }
        m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
      } else {
        QTimer::singleShot(0, this, [this]() {
          onActiveNodeChanged(ActiveObjects::instance().activeNode());
        });
      }
    });

  // Title header declaring the type of the selected node.
  QString title;
  if (qobject_cast<pipeline::SinkGroupNode*>(node)) {
    title = tr("Visualizations");
  } else if (qobject_cast<pipeline::SourceNode*>(node)) {
    title = tr("Source Node");
  } else if (qobject_cast<pipeline::TransformNode*>(node)) {
    title = tr("Transform Node");
  } else if (qobject_cast<pipeline::SinkNode*>(node)) {
    title = tr("Sink Node");
  } else {
    title = tr("Node");
  }

  QWidget* propsWidget = nullptr;

  if (auto* group = qobject_cast<pipeline::SinkGroupNode*>(node)) {
    auto* w = new pipeline::SinkGroupPropertiesWidget(
      group, pipeline(), m_ui->propertiesPanelStackedWidget);
    connect(w, &pipeline::SinkGroupPropertiesWidget::leaveGroupRequested, this,
            [this, group](pipeline::SinkNode* member) {
              leaveGroup(member, group);
            });
    connect(w, &pipeline::SinkGroupPropertiesWidget::deleteRequested, this,
            [this](pipeline::SinkNode* member) {
              auto* p = pipeline();
              if (p && !p->isExecuting()) {
                p->removeNode(member);
              }
            });
    propsWidget = w;
  } else if (auto* sink = dynamic_cast<pipeline::LegacyModuleSink*>(node)) {
    if (!node->isEditing()) {
      propsWidget = sink->createSinkPropertiesWidget(
        m_ui->propertiesPanelStackedWidget);
    }
    // React to detached colormap toggling while this sink is selected
    m_sinkColorMapChangedConn = connect(
      sink, &pipeline::LegacyModuleSink::colorMapChanged,
      this, [this, sink]() {
        if (sink->useDetachedColorMap()) {
          m_ui->centralWidget->setActiveSinkNode(sink);
        } else {
          updateColorMapDisplay();
        }
      });
  } else if (node && node->hasPropertiesWidget() && !node->isEditing()) {
    propsWidget = new pipeline::NodePropertiesPanel(
      node, pipeline(), m_ui->propertiesPanelStackedWidget);
  }

  // Always show the title for a selected node, even when it has no
  // editable properties (title-only panel).
  showPropertiesPanel(propsWidget, title);

  // Sink with detached colormap: display it; otherwise the tip port
  // drives the colormap via updateColorMapDisplay().
  if (auto* sink = dynamic_cast<pipeline::LegacyModuleSink*>(node)) {
    if (sink->useDetachedColorMap()) {
      m_ui->centralWidget->setActiveSinkNode(sink);
      return;
    }
  }
  updateColorMapDisplay();
}

void MainWindow::onActivePortChanged(pipeline::OutputPort* port)
{
  m_pipelineStrip->setSelectedPort(port);
  clearDynamicPropertiesWidget();
  if (!port) {
    m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
    return;
  }

  if (pipeline::isVolumeType(port->type())) {
    auto* propsWidget =
      new pipeline::VolumePropertiesWidget(nullptr);
    propsWidget->setOutputPort(port);
    connect(propsWidget->showTimeSeriesLabelCheckBox(), &QCheckBox::toggled,
            &ActiveObjects::instance(),
            &ActiveObjects::setShowTimeSeriesLabel);
    connect(&ActiveObjects::instance(),
            &ActiveObjects::showTimeSeriesLabelChanged,
            propsWidget->showTimeSeriesLabelCheckBox(),
            &QCheckBox::setChecked);
    propsWidget->setupInteractiveTransform();
    auto* scrollArea = new QScrollArea(m_ui->propertiesPanelStackedWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(propsWidget);
    showPropertiesPanel(scrollArea, tr("Output Port"));
  } else if (port->type() == pipeline::PortType::Molecule && port->hasData()) {
    try {
      auto molecule =
        port->data().value<vtkSmartPointer<vtkMolecule>>();
      if (molecule) {
        auto* propsWidget = new MoleculeProperties(
          molecule, m_ui->propertiesPanelStackedWidget);
        showPropertiesPanel(propsWidget, tr("Output Port"));
      } else {
        m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
      }
    } catch (const std::bad_any_cast&) {
      m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
    }
  } else {
    m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
  }
}

void MainWindow::onActiveLinkChanged(pipeline::Link* link)
{
  m_pipelineStrip->setSelectedLink(link);
  if (!link) {
    // A null link also arrives as a side effect of selecting a node/port,
    // whose own handler repopulates the panel. Only tear the panel down
    // here if it is actually showing a link's properties (e.g. on a full
    // clearActiveSelection while a link was the active object). The tracked
    // widget is the title wrapper, so look for the inner widget.
    if (m_dynamicPropertiesWidget &&
        m_dynamicPropertiesWidget->findChild<pipeline::LinkPropertiesWidget*>()) {
      clearDynamicPropertiesWidget();
      m_ui->propertiesPanelStackedWidget->setCurrentWidget(m_ui->empty);
    }
    return;
  }
  clearDynamicPropertiesWidget();

  auto* propsWidget =
    new pipeline::LinkPropertiesWidget(link, m_ui->propertiesPanelStackedWidget);
  connect(propsWidget, &pipeline::LinkPropertiesWidget::deleteRequested, this,
          [this](pipeline::Link* l) {
            auto* p = pipeline();
            if (p && !p->isExecuting()) {
              p->removeLink(l);
            }
            ActiveObjects::instance().clearActiveSelection();
          });
  showPropertiesPanel(propsWidget, tr("Link"));
}

bool MainWindow::ensureColorMapForPort(pipeline::Node* node,
                                       pipeline::OutputPort* port)
{
  if (!port || !port->hasData() ||
      !pipeline::isVolumeType(port->type())) {
    return false;
  }
  pipeline::VolumeDataPtr vol;
  try {
    vol = port->data().value<pipeline::VolumeDataPtr>();
  } catch (const std::bad_any_cast&) {
    return false;
  }
  if (!vol) {
    return false;
  }

  // LabelMap ports never inherit a colormap: their colors come from
  // their own label table, reconciled against the label set this
  // execution produced. Skip rescale too — the per-label node positions
  // are already in data-coordinate space and rescaling would shift them.
  if (port->type() == pipeline::PortType::LabelMap) {
    bool created = !vol->hasColorMap();
    pipeline::applyLabelMapColors(vol);
    return created;
  }

  pipeline::VolumeDataPtr upstream;
  if (node) {
    for (auto* in : node->inputPorts()) {
      if (in->hasData() && pipeline::isVolumeType(in->data().type())) {
        try {
          upstream = in->data().value<pipeline::VolumeDataPtr>();
        } catch (const std::bad_any_cast&) {
        }
        if (upstream) {
          break;
        }
      }
    }
  }
  // Pass-through node: shared color map, nothing to do.
  if (vol == upstream) {
    return false;
  }

  bool created = false;
  if (!vol->hasColorMap()) {
    vol->initColorMap();
    // The same choice as the execution path (inheritOutputMetadata): the
    // primary input's color map, never a label map's
    const auto inputs =
      node ? node->collectInputs() : QMap<QString, pipeline::PortData>();
    if (auto source = pipeline::colorMapSource(node, inputs)) {
      vol->copyColorMapFrom(*source);
    } else {
      // Nothing to inherit: tomviz's default preset, as loaded data gets,
      // not ParaView's
      ColorMap::instance().applyPreset(vol->colorMap());
    }
    created = true;
  }
  // A time-series step switch replaces the image in place and announces
  // it with the same signal new data uses. The range that matters spans
  // the whole series and cannot move with the step, so rescaling here
  // would only re-stretch the user's window on every frame of playback.
  if (created || !vol->hasTimeSteps()) {
    vol->rescaleColorMap();
  }
  return created;
}

void MainWindow::scheduleColorMapDisplayUpdate()
{
  // Coalesce — drop if a refresh is already scheduled. Live operator
  // updates fire dataChanged at multi-Hz rates; without this each
  // would re-bind the color-map / histogram machinery.
  if (m_colorMapUpdatePending) {
    return;
  }
  m_colorMapUpdatePending = true;
  static constexpr int kColorMapUpdateThrottleMs = 200;
  QTimer::singleShot(kColorMapUpdateThrottleMs, this, [this]() {
    m_colorMapUpdatePending = false;
    updateColorMapDisplay();
  });
}

void MainWindow::updateColorMapDisplay()
{
  auto* tipPort = ActiveObjects::instance().activeTipOutputPort();
  if (!tipPort || !pipeline::isVolumeType(tipPort->type()) ||
      !tipPort->hasData()) {
    // Nothing valid to display — clear the histogram + gradient
    // opacity widgets so a Reset (or any state-clear) doesn't leave
    // stale curves from the previous pipeline on screen.
    m_ui->centralWidget->setActiveVolumeData(nullptr);
    return;
  }
  pipeline::VolumeDataPtr vol;
  try {
    vol = tipPort->data().value<pipeline::VolumeDataPtr>();
  } catch (const std::bad_any_cast&) {
    m_ui->centralWidget->setActiveVolumeData(nullptr);
    return;
  }
  m_ui->centralWidget->setActiveVolumeData(vol);
}

} // namespace tomviz
