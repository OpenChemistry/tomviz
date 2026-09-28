/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineModuleMenu.h"

#include "ActiveObjects.h"
#include "MainWindow.h"

#include "pipeline/Link.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineUtils.h"
#include "pipeline/OutputPort.h"
#include "pipeline/InputPort.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/sinks/LegacyModuleSink.h"
#include "pipeline/data/LabelMapData.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/VolumeSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/OutlineSink.h"
#include "pipeline/sinks/ThresholdSink.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/RulerSink.h"
#include "pipeline/sinks/ScaleCubeSink.h"
#include "pipeline/sinks/MoleculeSink.h"
#include "pipeline/sinks/PlotSink.h"

#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqServer.h>
#include <pqServerManagerModel.h>
#include <pqView.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMContextViewProxy.h>
#include <vtkSMViewLayoutProxy.h>
#include <vtkSMViewProxy.h>

#include <QApplication>
#include <QEvent>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>

namespace tomviz {

static vtkSMViewProxy* resolveViewForSink(const QString& sinkType)
{
  bool needsChart = (sinkType == "Plot");
  QString viewTypeName = needsChart ? "XYChartView" : "RenderView";

  auto* activeView = ActiveObjects::instance().activeView();
  if (activeView) {
    bool activeIsChart = vtkSMContextViewProxy::SafeDownCast(activeView);
    bool activeIsRender = vtkSMRenderViewProxy::SafeDownCast(activeView);
    if ((needsChart && activeIsChart) || (!needsChart && activeIsRender)) {
      return activeView;
    }
  }

  auto* smModel = pqApplicationCore::instance()->getServerManagerModel();
  QList<pqView*> allViews = smModel->findItems<pqView*>();

  QList<pqView*> matching;
  for (auto* v : allViews) {
    auto* proxy = v->getViewProxy();
    bool isChart = vtkSMContextViewProxy::SafeDownCast(proxy);
    bool isRender = vtkSMRenderViewProxy::SafeDownCast(proxy);
    if ((needsChart && isChart) || (!needsChart && isRender)) {
      matching.append(v);
    }
  }

  if (matching.isEmpty()) {
    if (!needsChart) {
      ActiveObjects::instance().createRenderViewIfNeeded();
      return ActiveObjects::instance().activeView();
    }

    // No chart view exists — split the layout and create one.
    int emptyCell = -1;
    vtkSMViewLayoutProxy* layout = nullptr;
    if (activeView) {
      layout = vtkSMViewLayoutProxy::FindLayout(activeView);
      if (layout) {
        int location = layout->GetViewLocation(activeView);
        int leftChild = layout->Split(
          location, vtkSMViewLayoutProxy::HORIZONTAL, 0.5);
        if (leftChild >= 0) {
          emptyCell = leftChild + 1;
        }
      }
    }

    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    auto* server = pqApplicationCore::instance()->getActiveServer();
    auto* newView = builder->createView(viewTypeName, server);
    if (!newView) {
      return nullptr;
    }
    auto* proxy = newView->getViewProxy();

    if (layout && emptyCell >= 0) {
      layout->AssignView(emptyCell, proxy);
    }

    ActiveObjects::instance().setActiveView(proxy);
    return proxy;
  }

  if (matching.size() == 1) {
    auto* proxy = matching.first()->getViewProxy();
    ActiveObjects::instance().setActiveView(proxy);
    return proxy;
  }

  QStringList names;
  for (auto* v : matching) {
    names << v->getSMName();
  }
  bool ok = false;
  QString chosen = QInputDialog::getItem(
    nullptr, "Select View",
    QString("Multiple views available. Select one:"),
    names, 0, false, &ok);
  if (!ok) {
    return nullptr;
  }
  int idx = names.indexOf(chosen);
  auto* proxy = matching[idx]->getViewProxy();
  ActiveObjects::instance().setActiveView(proxy);
  return proxy;
}

PipelineModuleMenu::PipelineModuleMenu(QToolBar* toolBar, QMenu* menu,
                                       QObject* parentObject)
  : QObject(parentObject), m_menu(menu), m_toolBar(toolBar)
{
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);
  connect(menu, &QMenu::triggered, this, &PipelineModuleMenu::triggered);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activeTipOutputPortChanged,
          this, &PipelineModuleMenu::updateEnableState);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::activePipelineChanged,
          this, &PipelineModuleMenu::updateEnableState);
  qApp->installEventFilter(this);
  updateActions();
}

PipelineModuleMenu::~PipelineModuleMenu() = default;

QList<QString> PipelineModuleMenu::sinkTypes()
{
  return { "Volume", "Label Map", "Outline", "Slice", "Contour",
           "Threshold", "Clip", "Ruler", "Scale Cube", "Molecule",
           "Plot" };
}

QIcon PipelineModuleMenu::sinkIcon(const QString& type)
{
  // Icon paths must match the legacy Module::icon() implementations
  static QMap<QString, QString> iconMap = {
    { "Volume", ":/icons/pqVolumeData.png" },
    { "Label Map", ":/pipeline/port_labelmap.svg" },
    { "Outline", ":/pqWidgets/Icons/pqProbeLocation.svg" },
    { "Slice", ":/icons/orthoslice.svg" },
    { "Contour", ":pqWidgets/Icons/pqIsosurface.svg" },
    { "Threshold", ":/pqWidgets/Icons/pqThreshold.svg" },
    { "Clip", ":/pqWidgets/Icons/pqClip.svg" },
    { "Ruler", ":/pqWidgets/Icons/pqRuler.svg" },
    { "Scale Cube", ":/icons/pqMeasurementCube.png" },
    { "Molecule", ":/pqWidgets/Icons/pqGroup.svg" },
    { "Plot", ":/pqWidgets/Icons/pqLineChart16.png" },
  };
  auto it = iconMap.find(type);
  return (it != iconMap.end()) ? QIcon(it.value()) : QIcon();
}

pipeline::PortTypes PipelineModuleMenu::sinkAcceptedTypes(const QString& type)
{
  if (type == "Plot")
    return pipeline::PortType::Table;
  if (type == "Molecule")
    return pipeline::PortType::Molecule;
  if (type == "Label Map")
    return pipeline::PortType::LabelMap;
  return pipeline::PortType::ImageData;
}

bool PipelineModuleMenu::sinkSuitsPort(const QString& type,
                                       pipeline::OutputPort* port)
{
  if (!port) {
    return false;
  }

  const auto portType = port->type();

  // Label Map is worth offering for any volume whose values read as
  // labels, not only for a port already typed as one. A segmentation
  // loaded from a TIFF or an EMD arrives as an ordinary volume, since
  // the reader has no way to know the numbers are labels, and there
  // would otherwise be no way to say so.
  if (type == "Label Map" && portType != pipeline::PortType::LabelMap) {
    if (!pipeline::isVolumeType(portType) || !port->hasData()) {
      return false;
    }
    return pipeline::canInterpretAsLabelMap(
      port->data().value<pipeline::VolumeDataPtr>());
  }

  if (!pipeline::isPortTypeCompatible(portType, sinkAcceptedTypes(type))) {
    return false;
  }
  // Volume accepts ImageData, and LabelMap is one of its subtypes, so
  // the compatibility check alone would keep offering the plain volume
  // rendering for a segmentation. Label Map is the one to reach for
  // there: it renders the same way and adds the per-label controls.
  //
  // Only for a port actually typed as a label map: a plain volume that
  // merely could be read as one is far more often just a volume.
  if (type == "Volume" && portType == pipeline::PortType::LabelMap) {
    return false;
  }
  return true;
}

bool PipelineModuleMenu::eventFilter(QObject* obj, QEvent* event)
{
  if (event->type() == QEvent::KeyPress ||
      event->type() == QEvent::KeyRelease) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Control) {
      m_ctrlHeld = (event->type() == QEvent::KeyPress);
      updateEnableState();
    }
  }
  return QObject::eventFilter(obj, event);
}

void PipelineModuleMenu::updateEnableState()
{
  auto* tipPort = ActiveObjects::instance().activeTipOutputPort();

  for (auto* action : m_menu->actions()) {
    auto type = action->data().toString();
    if (type.isEmpty())
      continue;

    if (m_ctrlHeld || !tipPort) {
      // Ctrl held: enable all (user will link manually).
      // No tip port: enable all and let triggered() handle the error.
      action->setEnabled(m_ctrlHeld);
      continue;
    }
    action->setEnabled(sinkSuitsPort(type, tipPort));
  }
}

pipeline::LegacyModuleSink* PipelineModuleMenu::createSink(const QString& type)
{
  if (type == "Volume") {
    return new pipeline::VolumeSink();
  } else if (type == "Label Map") {
    return new pipeline::LabelMapSink();
  } else if (type == "Outline") {
    return new pipeline::OutlineSink();
  } else if (type == "Slice") {
    return new pipeline::SliceSink();
  } else if (type == "Contour") {
    return new pipeline::ContourSink();
  } else if (type == "Threshold") {
    return new pipeline::ThresholdSink();
  } else if (type == "Clip") {
    return new pipeline::ClipSink();
  } else if (type == "Ruler") {
    return new pipeline::RulerSink();
  } else if (type == "Scale Cube") {
    return new pipeline::ScaleCubeSink();
  } else if (type == "Molecule") {
    return new pipeline::MoleculeSink();
  } else if (type == "Plot") {
    return new pipeline::PlotSink();
  }
  return nullptr;
}

void PipelineModuleMenu::updateActions()
{
  QMenu* menu = m_menu;
  QToolBar* toolBar = m_toolBar;
  Q_ASSERT(menu);
  Q_ASSERT(toolBar);

  menu->clear();
  toolBar->clear();

  for (const QString& type : sinkTypes()) {
    auto actn = menu->addAction(sinkIcon(type), type);
    actn->setData(type);
    toolBar->addAction(actn);
  }
  updateEnableState();
}

void PipelineModuleMenu::triggered(QAction* maction)
{
  auto type = maction->data().toString();
  if (type.isEmpty()) {
    return;
  }

  auto* mainWindow = MainWindow::instance();
  auto* pip = mainWindow ? mainWindow->pipeline() : nullptr;
  if (!pip) {
    qCritical("No active pipeline. Load data first.");
    return;
  }

  auto* view = resolveViewForSink(type);
  if (!view) {
    return;
  }

  auto* targetPort = ActiveObjects::instance().activeTipOutputPort();
  if (!targetPort) {
    qCritical("No output port available. Load data first.");
    return;
  }

  auto* sink = createSink(type);
  if (!sink) {
    qCritical("Failed to create sink for type: %s", qPrintable(type));
    return;
  }

  sink->setLabel(type);
  sink->initialize(view);
  pip->addNode(sink);

  // Ctrl held: add the node unconnected (user will link manually)
  if (!(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
    auto* input = sink->inputPorts()[0];
    if (!pipeline::isPortTypeCompatible(targetPort->type(),
                                        input->acceptedTypes())) {
      qCritical("Incompatible port types: sink input does not accept "
                "the tip output port type.");
      return;
    }

    // Hang the sink off the tip port's SinkGroupNode, creating one when
    // the port has none yet.
    pip->createLink(pipeline::sinkAttachPort(pip, targetPort, input),
                    input);
  }
  pip->executeWhenIdle();
}

} // namespace tomviz
