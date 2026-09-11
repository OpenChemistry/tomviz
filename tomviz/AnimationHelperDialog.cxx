/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationHelperDialog.h"
#include "ui_AnimationHelperDialog.h"

#include "ActiveObjects.h"
#include "CameraAnimation.h"
#include "CameraViewpoints.h"
#include "ClipAnimation.h"
#include "ContourAnimation.h"
#include "ModuleAnimations.h"
#include "MovieExportDialog.h"
#include "OpacityAnimation.h"
#include "ScalarOpacityAnimation.h"
#include "SceneSnapshot.h"
#include "SliceAnimation.h"
#include "Utilities.h"

#include "pipeline/Pipeline.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/SourceNode.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <pqAnimationCue.h>
#include <pqImageUtil.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqPVApplicationCore.h>
#include <pqPropertyLinks.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>
#include <pqServerManagerModel.h>

#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderer.h>
#include <vtkSMAnimationScene.h>
#include <vtkSMProxy.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <QBuffer>
#include <QCheckBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QImage>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

namespace tomviz {

namespace {

pipeline::SourceNode* findUpstreamSource(pipeline::Node* node)
{
  if (!node) {
    return nullptr;
  }
  if (auto* src = qobject_cast<pipeline::SourceNode*>(node)) {
    return src;
  }
  for (auto* up : node->upstreamNodes()) {
    if (auto* src = findUpstreamSource(up)) {
      return src;
    }
  }
  return nullptr;
}

// Big enough to tell two framings of the same volume apart, small
// enough that a dozen of them in a state file is not worth noticing.
const QSize thumbnailSize(128, 96);

// A sparkline of an opacity curve. Two captured curves are hard to tell
// apart from their point counts, and the whole animation is the difference
// between them, so draw the shape.
QPixmap curvePreview(vtkPiecewiseFunction* curve, const double range[2],
                     const QSize& size, const QPalette& palette)
{
  QPixmap preview(size);
  preview.fill(Qt::transparent);
  if (!curve || curve->GetSize() == 0 || !(range[1] > range[0])) {
    return preview;
  }

  QPainterPath path;
  for (int x = 0; x < size.width(); ++x) {
    const double value =
      range[0] + (range[1] - range[0]) * x / (size.width() - 1.0);
    const double opacity = qBound(0.0, curve->GetValue(value), 1.0);
    const QPointF point(x, (1.0 - opacity) * (size.height() - 3) + 1.5);
    if (x == 0) {
      path.moveTo(point);
    } else {
      path.lineTo(point);
    }
  }

  QPainter painter(&preview);
  painter.setRenderHint(QPainter::Antialiasing);
  // Text color rather than highlight: the sparkline has to stay visible
  // when its row is selected and painted with the highlight color.
  painter.setPen(QPen(palette.windowText().color(), 1.5));
  painter.drawPath(path);
  return preview;
}

// A PNG of what the view currently shows, or empty if it cannot be read.
QByteArray captureThumbnail(pqRenderView* renderView)
{
  auto* proxy = renderView ? renderView->getRenderViewProxy() : nullptr;
  if (!proxy) {
    return {};
  }

  // CaptureWindow hands back a reference that is ours to release.
  vtkSmartPointer<vtkImageData> image;
  image.TakeReference(proxy->CaptureWindow(1));
  if (!image) {
    return {};
  }

  QImage captured;
  if (!pqImageUtil::fromImageData(image, captured) || captured.isNull()) {
    return {};
  }

  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  captured.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    .save(&buffer, "PNG");
  return png;
}

} // anonymous namespace

class AnimationHelperDialog::Internal : public QObject
{
public:
  Ui::AnimationHelperDialog ui;
  // Whether Add/Update Viewpoint also records the module state
  QCheckBox* recordScene = nullptr;
  // Caption shown in the view while the path is at the selected viewpoint
  QLineEdit* viewpointLabel = nullptr;
  pqPropertyLinks pqLinks;
  QPointer<AnimationHelperDialog> parent;
  vtkWeakPointer<vtkSMProxy> linkedScene;

  // Opacity curves captured but not yet added, keyed by the volume they
  // were captured for so switching modules never shows another volume's
  // captures. Entries are dropped when their node leaves the pipeline.
  QHash<pipeline::Node*,
        QMap<int, vtkSmartPointer<vtkPiecewiseFunction>>> stagedCurves;

  // What the range page is currently configured for, so pipeline
  // executions only rebuild it (and clobber user-entered values) when the
  // data-derived bounds actually moved.
  QPointer<pipeline::Node> configuredNode;
  QString configuredProperty;
  double configuredLo = 0.0;
  double configuredHi = 0.0;
  bool configuredClipOrtho = true;

  Internal(AnimationHelperDialog* p) : QObject(p), parent(p)
  {
    ui.setupUi(p);

    auto* labelRow = new QHBoxLayout;
    auto* labelTitle = new QLabel("Label:", parent);
    viewpointLabel = new QLineEdit(parent);
    viewpointLabel->setPlaceholderText("Caption shown while at this viewpoint");
    viewpointLabel->setToolTip(
      "Text drawn in the corner of the 3D view, and in exported movies, "
      "from this viewpoint until the next one.");
    labelTitle->setBuddy(viewpointLabel);
    labelRow->addWidget(labelTitle);
    labelRow->addWidget(viewpointLabel, 1);
    ui.cameraLayout->insertLayout(
      ui.cameraLayout->indexOf(ui.segmentLayout) + 1, labelRow);
    connect(viewpointLabel, &QLineEdit::editingFinished, this,
            &Internal::labelChanged);

    recordScene = new QCheckBox("Record module state with viewpoints", parent);
    recordScene->setToolTip(
      "Also save which modules are visible, their opacity, and any volume "
      "cut-out with each viewpoint, and move between those states while "
      "flying the path.");
    recordScene->setChecked(true);
    ui.cameraLayout->addWidget(recordScene);

    ui.viewpointList->setIconSize(QSize(96, 72));
    ui.viewpointList->setDragDropMode(QAbstractItemView::InternalMove);
    ui.viewpointList->setDefaultDropAction(Qt::MoveAction);
    ui.viewpointList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui.viewpointList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.viewpointList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui.keyframeList->setIconSize(QSize(96, 28));

    updateGui();
    setupConnections();
  }

  void setupConnections()
  {
    // Camera animation mode. clicked rather than toggled: reflecting the
    // actual state back into the radios must not re-trigger the actions.
    connect(ui.radioCameraNone, &QRadioButton::clicked, this,
            &Internal::cameraModeChanged);
    connect(ui.radioCameraOrbit, &QRadioButton::clicked, this,
            &Internal::cameraModeChanged);
    connect(ui.radioCameraPath, &QRadioButton::clicked, this,
            &Internal::cameraModeChanged);

    // Camera viewpoints
    connect(ui.addViewpoint, &QPushButton::clicked, this,
            &Internal::addViewpoint);
    connect(ui.updateViewpoint, &QPushButton::clicked, this,
            &Internal::updateViewpoint);
    connect(ui.removeViewpoint, &QPushButton::clicked, this,
            &Internal::removeViewpoint);
    connect(ui.viewpointList, &QListWidget::currentRowChanged, this,
            [this]() {
              updateSegmentControls();
              updateEnableStates();
            });
    connect(ui.viewpointList, &QListWidget::itemDoubleClicked, this,
            &Internal::goToViewpoint);
    connect(ui.viewpointList, &QListWidget::customContextMenuRequested, this,
            &Internal::showViewpointMenu);
    connect(ui.viewpointList, &QListWidget::itemChanged, this,
            &Internal::commitViewpointRename);
    // Drag-and-drop reorder. The list has already moved its row when this
    // fires; defer the model sync so the rebuild it triggers does not run
    // inside the view's own drop handling.
    connect(ui.viewpointList->model(), &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex&, int start, int, const QModelIndex&,
                   int destination) {
              int to = destination > start ? destination - 1 : destination;
              QTimer::singleShot(0, this, [this, start, to]() {
                CameraViewpoints::instance().move(start, to);
                ui.viewpointList->setCurrentRow(to);
              });
            });
    connect(ui.segmentDuration,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Internal::segmentChanged);
    connect(ui.segmentEased, &QCheckBox::toggled, this,
            &Internal::segmentChanged);
    // The list is shared with the state file, so it can change while the
    // dialog is open. Everything showing viewpoint names follows it.
    connect(&CameraViewpoints::instance(), &CameraViewpoints::changed, this,
            [this]() {
              refreshViewpoints();
              refreshSegmentOptions();
              refreshKeyframeRows();
              refreshAnimationList();
            });

    // Time series
    connect(&activeObjects(),
            &ActiveObjects::timeSeriesAnimationsEnableStateChanged,
            ui.enableTimeSeriesAnimations, &QCheckBox::setChecked);
    connect(ui.enableTimeSeriesAnimations, &QCheckBox::toggled,
            &activeObjects(), &ActiveObjects::enableTimeSeriesAnimations);
    connect(ui.enableTimeSeriesAnimations, &QCheckBox::toggled, this,
            [this](bool b) {
              updateEnableStates();
              if (b) {
                play();
              }
            });

    // Pipeline node changes
    if (auto* pip = pipeline()) {
      connect(pip, &pipeline::Pipeline::nodeAdded, this,
              &Internal::onNodeAdded);
      connect(pip, &pipeline::Pipeline::nodeRemoved, this,
              &Internal::onNodeRemoved);
      // Sinks re-cache their scalar range / slice count on every
      // consume, but the authoring row only re-reads them on selection
      // changes. Track executions so the ranges follow the data.
      connect(pip, &pipeline::Pipeline::executionFinished, this,
              &Internal::refreshModuleRanges);
    }

    // Visualization animation authoring
    connect(ui.selectedDataSource,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::selectedDataSourceChanged);
    connect(ui.selectedModule,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::selectedModuleChanged);
    connect(ui.animatedProperty,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() {
              configurePropertyPage();
              updateEnableStates();
            });
    connect(ui.captureCurve, &QPushButton::clicked, this,
            &Internal::captureCurve);
    connect(ui.clearCurve, &QPushButton::clicked, this,
            &Internal::clearCurve);
    connect(ui.keyframeList, &QListWidget::currentRowChanged, this,
            [this]() { updateEnableStates(); });
    connect(ui.keyframeList, &QListWidget::itemDoubleClicked, this,
            &Internal::loadCurveIntoEditor);
    connect(ui.addModuleAnimation, &QPushButton::clicked, this,
            &Internal::addModuleAnimation);
    // The registry can change from outside the dialog too, e.g. when a
    // state file loads a whole set at once.
    connect(&ModuleAnimations::instance(), &ModuleAnimations::changed, this,
            [this]() {
              refreshAnimationList();
              updateEnableStates();
            });

    // All animations
    linkToScene();
    // React to user edits of the frame count only. Connecting to the spin
    // box's valueChanged directly would also fire when the property link
    // syncs the widget from the scene (e.g. after a relink, or when a time
    // series load sets the frame count), and numberOfFramesModified would
    // then clobber a freshly set "Snap To TimeSteps" play mode.
    // qtWidgetChanged is only emitted for Qt-originated changes.
    connect(&pqLinks, &pqPropertyLinks::qtWidgetChanged, this,
            &Internal::numberOfFramesModified);
    // Loading a state file resets the ParaView session and replaces the
    // animation scene; rebind the frame count link when that happens while
    // the dialog is open (showEvent covers the closed case).
    connect(pqPVApplicationCore::instance()->animationManager(),
            &pqAnimationManager::activeSceneChanged, this,
            &Internal::linkToScene);
    connect(ui.exportMovie, &QPushButton::clicked, this,
            &Internal::exportMovie);
    connect(ui.clearAllAnimations, &QPushButton::clicked, this,
            &Internal::clearAllAnimations);
  }

  // Bind the frame count spin box to the current animation scene. Loading
  // a state file resets the ParaView session, which destroys the scene and
  // creates a new one, so the link built when the dialog was first opened
  // can be pointing at a dead proxy. The dialog is created once and only
  // hidden on close, so re-check the scene every time it is shown.
  void linkToScene()
  {
    auto* sceneProxy = scene() ? scene()->getProxy() : nullptr;
    if (!sceneProxy || sceneProxy == linkedScene) {
      return;
    }

    pqLinks.removeAllPropertyLinks();
    pqLinks.addPropertyLink(ui.numberOfFrames, "value",
                            SIGNAL(valueChanged(int)), sceneProxy,
                            sceneProxy->GetProperty("NumberOfFrames"), 0);
    linkedScene = sceneProxy;
  }

  void refresh()
  {
    linkToScene();
    updateGui();
    refreshViewpoints();
    refreshSegmentOptions();
    refreshAnimationList();
    refreshModuleRanges();
  }

  void play()
  {
    auto* animationScene = scene();
    if (!animationScene) {
      return;
    }
    auto* proxy = animationScene->getProxy();

    // Play() runs a blocking loop that pumps events, so a mode switch
    // made mid-playback executes inside that loop, and re-invoking Play
    // there is a hard vtkAnimationPlayer error ("Cannot play during an
    // active playback"). Stop() only raises a flag the loop honors once
    // control unwinds, so the restart has to be deferred; if the timer
    // fires while the old loop is still winding down, play() simply
    // defers again.
    auto* sceneObject =
      vtkSMAnimationScene::SafeDownCast(proxy->GetClientSideObject());
    if (sceneObject && sceneObject->GetInPlay()) {
      proxy->InvokeCommand("Stop");
      QTimer::singleShot(0, this, [this]() { play(); });
      return;
    }

    proxy->InvokeCommand("Play");
  }

  // The active view isn't necessarily a render view: a Plot module makes
  // an XYChartView active. Fall back to the first render view so the orbit
  // still ends up somewhere the user can see it.
  pqRenderView* renderViewForOrbit()
  {
    if (auto* renderView = activeObjects().activePqRenderView()) {
      return renderView;
    }

    auto* smModel = pqApplicationCore::instance()->getServerManagerModel();
    auto renderViews = smModel->findItems<pqRenderView*>();
    return renderViews.isEmpty() ? nullptr : renderViews.first();
  }

  void updateGui()
  {
    ui.enableTimeSeriesAnimations->setChecked(
      activeObjects().timeSeriesAnimationsEnabled());

    updateDataSourceOptions();
    updateEnableStates();
  }

  void updateEnableStates()
  {
    bool hasCameraCues = false;
    if (auto* animationScene = scene()) {
      for (auto* cue : animationScene->getCues()) {
        if (cue->getSMName().startsWith("CameraAnimationCue")) {
          hasCameraCues = true;
          break;
        }
      }
    }
    bool hasCameraPath = CameraViewpoints::instance().isFlying();
    bool hasCameraAnimations = hasCameraCues || hasCameraPath;

    // Reflect what is actually driving the camera. Blocked so setting the
    // state never re-runs the mode actions.
    {
      QSignalBlocker blockNone(ui.radioCameraNone);
      QSignalBlocker blockOrbit(ui.radioCameraOrbit);
      QSignalBlocker blockPath(ui.radioCameraPath);
      if (hasCameraPath) {
        ui.radioCameraPath->setChecked(true);
      } else if (hasCameraCues) {
        ui.radioCameraOrbit->setChecked(true);
      } else {
        ui.radioCameraNone->setChecked(true);
      }
    }

    bool hasTimeSeries = false;
    auto* tk = activeObjects().activeTimeKeeper();
    if (tk && !tk->getTimeSteps().empty()) {
      hasTimeSeries = true;
    }
    bool timeSeriesEnabled =
      ui.enableTimeSeriesAnimations->isChecked() && hasTimeSeries;
    // The checkbox means nothing without a series loaded, so it does not
    // take up footer space until one is.
    ui.enableTimeSeriesAnimations->setVisible(hasTimeSeries);

    int viewpointCount = CameraViewpoints::instance().size();
    int selectedViewpoint = ui.viewpointList->currentRow();
    bool viewpointSelected = selectedViewpoint >= 0;

    ui.addViewpoint->setEnabled(renderViewForOrbit() != nullptr);
    ui.updateViewpoint->setEnabled(viewpointSelected);
    ui.removeViewpoint->setEnabled(viewpointSelected);
    // One viewpoint is a camera position, not a path.
    ui.radioCameraPath->setEnabled(viewpointCount >= 2 || hasCameraPath);

    auto* node = selectedSink();
    bool hasModuleAnimations = !ModuleAnimations::instance().isEmpty();
    ui.selectedDataSource->setEnabled(ui.selectedDataSource->count() != 0);
    ui.selectedModule->setEnabled(ui.selectedModule->count() != 0);
    ui.animatedProperty->setEnabled(ui.animatedProperty->count() != 0);

    QString property = selectedProperty();
    bool curveProperty = property == "curve";
    int stagedCount = 0;
    if (node) {
      const int rows = keyframeRowCount();
      for (int anchor : stagedCurves.value(node).keys()) {
        if (anchor < rows) {
          ++stagedCount;
        }
      }
    }
    bool curveSelected = ui.keyframeList->currentRow() >= 0;
    bool curveStaged =
      node && curveSelected &&
      stagedCurves.value(node).contains(ui.keyframeList->currentRow());

    ui.captureCurve->setEnabled(curveProperty && curveSelected &&
                                liveOpacityCurve() != nullptr);
    ui.clearCurve->setEnabled(curveStaged);
    // A morph needs two curves to move between; everything else just
    // needs a visualization.
    ui.addModuleAnimation->setEnabled(node &&
                                      (!curveProperty || stagedCount >= 2));

    bool hasAnyAnimations =
      hasCameraAnimations || timeSeriesEnabled || hasModuleAnimations;
    ui.exportMovie->setEnabled(hasAnyAnimations);
    ui.clearAllAnimations->setEnabled(hasAnyAnimations);
  }

  // Camera animation mode
  void cameraModeChanged()
  {
    if (ui.radioCameraOrbit->isChecked()) {
      createCameraOrbitInternal();
    } else if (ui.radioCameraPath->isChecked()) {
      animateViewpointsInternal();
    } else {
      clearCameraCues();
      CameraViewpoints::instance().stopFlight();
    }
    updateEnableStates();
  }

  void createCameraOrbitInternal()
  {
    auto* renderView = renderViewForOrbit();
    if (!renderView) {
      return;
    }

    clearCameraCues(renderView->getRenderViewProxy());
    // The orbit and the viewpoint path both drive the camera every tick.
    CameraViewpoints::instance().stopFlight();
    createCameraOrbit(renderView->getRenderViewProxy());

    ensureAnimationFrames();
    play();
  }

  void animateViewpointsInternal()
  {
    auto* renderView = renderViewForOrbit();
    if (!renderView || CameraViewpoints::instance().size() < 2) {
      return;
    }

    // Only one animation can own the camera.
    clearCameraCues(renderView->getRenderViewProxy());
    ensureAnimationFrames();
    // startFlight() puts the camera at the head of the path straight
    // away, which previews it the moment it is switched on.
    CameraViewpoints::instance().startFlight(renderView);

    play();
  }

  // Camera viewpoints
  struct CameraContext
  {
    pqRenderView* view = nullptr;
    vtkSMRenderViewProxy* proxy = nullptr;
    vtkCamera* camera = nullptr;
  };

  // The render view and camera the viewpoint actions operate on. All
  // three are present or the camera is null.
  CameraContext cameraContext()
  {
    CameraContext context;
    context.view = renderViewForOrbit();
    context.proxy =
      context.view ? context.view->getRenderViewProxy() : nullptr;
    context.camera = context.proxy ? context.proxy->GetActiveCamera() : nullptr;
    return context;
  }

  void refreshViewpoints()
  {
    auto& viewpoints = CameraViewpoints::instance();

    QSignalBlocker blocked(ui.viewpointList);
    int previousRow = ui.viewpointList->currentRow();
    ui.viewpointList->clear();
    for (int i = 0; i < viewpoints.size(); ++i) {
      auto name = viewpoints.at(i).name;
      if (name.isEmpty()) {
        name = QString("Viewpoint %1").arg(i + 1);
      }
      auto* item = new QListWidgetItem(name);
      QPixmap thumbnail;
      if (thumbnail.loadFromData(viewpoints.at(i).thumbnail, "PNG")) {
        item->setIcon(QIcon(thumbnail));
      }
      ui.viewpointList->addItem(item);
    }
    if (previousRow >= 0 && previousRow < viewpoints.size()) {
      ui.viewpointList->setCurrentRow(previousRow);
    }

    updateSegmentControls();
    updateEnableStates();
  }

  void showViewpointMenu(const QPoint& pos)
  {
    auto* item = ui.viewpointList->itemAt(pos);
    if (!item) {
      return;
    }
    ui.viewpointList->setCurrentItem(item);

    QMenu menu;
    auto* rename = menu.addAction("Rename");
    auto* goTo = menu.addAction("Go To");
    auto* update = menu.addAction("Update From Current View");
    auto* remove = menu.addAction("Remove");
    auto* chosen = menu.exec(ui.viewpointList->mapToGlobal(pos));
    if (chosen == rename) {
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      ui.viewpointList->editItem(item);
    } else if (chosen == goTo) {
      goToViewpoint();
    } else if (chosen == update) {
      updateViewpoint();
    } else if (chosen == remove) {
      removeViewpoint();
    }
  }

  void commitViewpointRename(QListWidgetItem* item)
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->row(item);
    if (row < 0 || row >= viewpoints.size()) {
      return;
    }

    auto viewpoint = viewpoints.at(row);
    auto name = item->text().trimmed();
    if (name.isEmpty() || name == viewpoint.name) {
      // Rejected edit; put the old name back.
      refreshViewpoints();
      return;
    }

    viewpoint.name = name;
    viewpoints.replace(row, viewpoint);
  }

  // The duration and easing belong to the leg leaving the selected
  // viewpoint, so the last one in the list has nothing to edit.
  void updateSegmentControls()
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->currentRow();
    bool hasSegment = row >= 0 && row < viewpoints.size() - 1;

    QSignalBlocker blockedDuration(ui.segmentDuration);
    QSignalBlocker blockedEased(ui.segmentEased);
    QSignalBlocker blockedLabel(viewpointLabel);
    if (hasSegment) {
      ui.segmentDuration->setValue(viewpoints.at(row).duration);
      ui.segmentEased->setChecked(viewpoints.at(row).eased);
    }
    bool hasViewpoint = row >= 0 && row < viewpoints.size();
    viewpointLabel->setText(hasViewpoint ? viewpoints.at(row).label
                                         : QString());
    viewpointLabel->setEnabled(hasViewpoint);

    ui.segmentDurationLabel->setEnabled(hasSegment);
    ui.segmentDuration->setEnabled(hasSegment);
    ui.segmentEased->setEnabled(hasSegment);
  }

  void addViewpoint()
  {
    auto context = cameraContext();
    if (!context.camera) {
      return;
    }

    auto& viewpoints = CameraViewpoints::instance();
    Viewpoint viewpoint;
    viewpoint.readFrom(context.camera);
    viewpoint.thumbnail = captureThumbnail(context.view);
    viewpoint.name = viewpoints.nextDefaultName();
    if (recordScene->isChecked()) {
      viewpoint.scene = SceneSnapshot::capture(pipeline());
    }

    viewpoints.append(viewpoint);
    ui.viewpointList->setCurrentRow(viewpoints.size() - 1);

    // Building a path is a statement of intent: once there are enough
    // viewpoints to fly, make Play fly through them instead of doing a
    // camera orbit. Arm the flight only; no snap to the path head (the
    // user just framed this view) and no auto-play.
    if (viewpoints.size() >= 2 && !viewpoints.isFlying()) {
      clearCameraCues(context.proxy);
      viewpoints.startFlight(context.view, /*snapToHead=*/false);
      updateEnableStates();
    }
  }

  void updateViewpoint()
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->currentRow();
    auto context = cameraContext();
    if (row < 0 || row >= viewpoints.size() || !context.camera) {
      return;
    }

    // Re-frame the viewpoint but leave its place in the path alone.
    auto viewpoint = viewpoints.at(row);
    viewpoint.readFrom(context.camera);
    viewpoint.thumbnail = captureThumbnail(context.view);
    viewpoint.scene = recordScene->isChecked()
                        ? SceneSnapshot::capture(pipeline())
                        : SceneSnapshot();
    viewpoints.replace(row, viewpoint);
  }

  void goToViewpoint()
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->currentRow();
    auto context = cameraContext();
    if (row < 0 || row >= viewpoints.size() || !context.camera) {
      return;
    }

    viewpoints.at(row).applyTo(context.camera);
    viewpoints.at(row).scene.apply(pipeline());
    if (auto* renderer = context.proxy->GetRenderer()) {
      renderer->ResetCameraClippingRange();
    }
    context.view->render();
  }

  void removeViewpoint()
  {
    CameraViewpoints::instance().removeAt(ui.viewpointList->currentRow());
  }

  void labelChanged()
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->currentRow();
    if (row < 0 || row >= viewpoints.size()) {
      return;
    }
    auto viewpoint = viewpoints.at(row);
    if (viewpoint.label == viewpointLabel->text()) {
      return;
    }
    viewpoint.label = viewpointLabel->text();
    viewpoints.replace(row, viewpoint);
  }

  void segmentChanged()
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->currentRow();
    if (row < 0 || row >= viewpoints.size()) {
      return;
    }

    auto viewpoint = viewpoints.at(row);
    viewpoint.duration = ui.segmentDuration->value();
    viewpoint.eased = ui.segmentEased->isChecked();
    viewpoints.replace(row, viewpoint);
  }

  // The legs a visualization animation can be bound to: the whole
  // timeline, or one hop of the camera path.
  void refreshSegmentOptions()
  {
    QSignalBlocker blocked(ui.animationSegment);
    // An empty combo has no current data, and asking an invalid variant
    // for an int gives 0, which is the first leg rather than "no leg".
    auto current = ui.animationSegment->currentData();
    int previous = current.isValid() ? current.toInt() : -1;

    ui.animationSegment->clear();
    ui.animationSegment->addItem("Whole animation", -1);

    auto& viewpoints = CameraViewpoints::instance();
    for (int i = 0; i + 1 < viewpoints.size(); ++i) {
      ui.animationSegment->addItem(segmentLabel(i), i);
    }

    int index = ui.animationSegment->findData(previous);
    ui.animationSegment->setCurrentIndex(index < 0 ? 0 : index);
  }

  QString segmentLabel(int segment)
  {
    auto& viewpoints = CameraViewpoints::instance();
    if (segment < 0 || segment + 1 >= viewpoints.size()) {
      // The leg's viewpoints are gone; all that is left is which leg it
      // was.
      return QString("viewpoints %1 to %2").arg(segment + 1).arg(segment + 2);
    }
    return viewpoints.at(segment).name + " to " +
           viewpoints.at(segment + 1).name;
  }

  // Data sources (pipeline source nodes)
  void updateDataSourceOptions()
  {
    QSignalBlocker blocked(ui.selectedDataSource);
    auto* previouslySelected = selectedSource();
    int previouslySelectedIndex = -1;

    ui.selectedDataSource->clear();

    auto* pip = pipeline();
    if (!pip) {
      updateEnableStates();
      return;
    }

    QStringList usedLabels;
    int idx = 0;
    for (auto* node : pip->nodes()) {
      auto* source = qobject_cast<pipeline::SourceNode*>(node);
      if (!source) {
        continue;
      }

      auto label = source->label();
      if (label.isEmpty()) {
        label = "Source";
      }

      auto uniqueLabel = label;
      int n = 1;
      while (usedLabels.contains(uniqueLabel)) {
        uniqueLabel = label + " " + QString::number(++n);
      }
      usedLabels.append(uniqueLabel);

      ui.selectedDataSource->addItem(
        uniqueLabel, QVariant::fromValue(static_cast<QObject*>(source)));

      if (source == previouslySelected) {
        previouslySelectedIndex = idx;
      }
      ++idx;
    }

    if (previouslySelectedIndex != -1) {
      ui.selectedDataSource->setCurrentIndex(previouslySelectedIndex);
    } else {
      selectedDataSourceChanged();
    }

    updateEnableStates();
  }

  pipeline::SourceNode* selectedSource()
  {
    if (ui.selectedDataSource->count() == 0) {
      return nullptr;
    }

    return qobject_cast<pipeline::SourceNode*>(
      ui.selectedDataSource->currentData().value<QObject*>());
  }

  // Sinks (animatable modules)
  void updateModuleOptions()
  {
    QSignalBlocker blocked(ui.selectedModule);
    auto* previouslySelected = selectedSink();
    int previouslySelectedIndex = -1;

    ui.selectedModule->clear();

    auto* source = selectedSource();
    auto* pip = pipeline();
    if (!source || !pip) {
      selectedModuleChanged();
      return;
    }

    QList<pipeline::Node*> sinks;
    for (auto* node : pip->nodes()) {
      if (qobject_cast<pipeline::ContourSink*>(node) ||
          qobject_cast<pipeline::SliceSink*>(node) ||
          qobject_cast<pipeline::ClipSink*>(node) ||
          ScalarOpacityAnimation::supports(node)) {
        if (findUpstreamSource(node) == source) {
          sinks.append(node);
        }
      }
    }

    QStringList labels;
    for (auto* sink : sinks) {
      auto label = sink->label();
      if (label.isEmpty()) {
        if (qobject_cast<pipeline::ContourSink*>(sink)) {
          label = "Contour";
        } else if (qobject_cast<pipeline::SliceSink*>(sink)) {
          label = "Slice";
        } else if (qobject_cast<pipeline::ClipSink*>(sink)) {
          label = "Clip";
        } else if (qobject_cast<pipeline::VolumeSink*>(sink)) {
          label = "Volume";
        }
      }

      auto uniqueLabel = label;
      int n = 1;
      while (labels.contains(uniqueLabel)) {
        uniqueLabel = label + " " + QString::number(++n);
      }
      labels.append(uniqueLabel);
    }

    for (int i = 0; i < sinks.size(); ++i) {
      ui.selectedModule->addItem(
        labels[i], QVariant::fromValue(static_cast<QObject*>(sinks[i])));

      if (sinks[i] == previouslySelected) {
        previouslySelectedIndex = i;
      }
    }

    if (previouslySelectedIndex != -1) {
      ui.selectedModule->setCurrentIndex(previouslySelectedIndex);
    }
    selectedModuleChanged();
  }

  pipeline::Node* selectedSink()
  {
    if (ui.selectedModule->count() == 0) {
      return nullptr;
    }

    return qobject_cast<pipeline::Node*>(
      ui.selectedModule->currentData().value<QObject*>());
  }

  void selectedDataSourceChanged() { updateModuleOptions(); }

  void selectedModuleChanged()
  {
    populateProperties();
    configurePropertyPage();
    updateEnableStates();
  }

  // Which properties of the selected visualization can be animated.
  void populateProperties()
  {
    QSignalBlocker blocked(ui.animatedProperty);
    auto previous = ui.animatedProperty->currentData().toString();

    ui.animatedProperty->clear();

    auto* node = selectedSink();
    if (qobject_cast<pipeline::ContourSink*>(node)) {
      ui.animatedProperty->addItem("Iso value", "iso");
      ui.animatedProperty->addItem("Opacity", "opacity");
    } else if (auto* slice = qobject_cast<pipeline::SliceSink*>(node)) {
      // A Custom plane has no index to sweep
      if (slice->isOrtho()) {
        ui.animatedProperty->addItem("Slice index", "slice");
      }
      ui.animatedProperty->addItem("Opacity", "opacity");
    } else if (auto* clip = qobject_cast<pipeline::ClipSink*>(node)) {
      ui.animatedProperty->addItem(
        clip->isOrtho() ? "Slice index" : "Position", "clip");
      ui.animatedProperty->addItem("Opacity", "opacity");
    } else if (ScalarOpacityAnimation::supports(node)) {
      ui.animatedProperty->addItem("Opacity curve", "curve");
    }

    int index = ui.animatedProperty->findData(previous);
    ui.animatedProperty->setCurrentIndex(index < 0 ? 0 : index);
  }

  QString selectedProperty()
  {
    return ui.animatedProperty->currentData().toString();
  }

  void configurePropertyPage()
  {
    auto* node = selectedSink();
    QString property = selectedProperty();

    bool curveProperty = property == "curve";
    ui.propertyStack->setCurrentWidget(curveProperty ? ui.curvePage
                                                     : ui.rangePage);
    // A curve morph is keyed to the viewpoints themselves, so there is no
    // separate leg to pick.
    ui.animationSegmentLabel->setEnabled(!curveProperty);
    ui.animationSegment->setEnabled(!curveProperty);

    if (curveProperty) {
      seedStagedCurves(node);
      refreshKeyframeRows();
      return;
    }

    configureRange(node, property);
  }

  void configureRange(pipeline::Node* node, const QString& property)
  {
    configuredNode = node;
    configuredProperty = property;

    QString label = "Range:";
    int decimals = 2;
    double lo = 0.0;
    double hi = 1.0;
    double startDefault = 0.0;
    double stopDefault = 1.0;

    if (property == "iso") {
      auto* contour = qobject_cast<pipeline::ContourSink*>(node);
      double range[2] = { 0.0, 1.0 };
      if (contour) {
        contour->scalarRange(range);
      }
      label = "Iso value:";
      lo = range[0];
      hi = range[1];
      startDefault = (hi - lo) / 3 + lo;
      stopDefault = (hi - lo) * 2 / 3 + lo;
    } else if (property == "slice") {
      auto* slice = qobject_cast<pipeline::SliceSink*>(node);
      label = "Slice:";
      decimals = 0;
      hi = slice ? slice->maxSlice() : 0;
      stopDefault = hi;

      // This reconfigures on every reselection and data update, so drop
      // the previous connection first or they accumulate.
      if (slice) {
        disconnect(slice, &pipeline::SliceSink::directionChanged, this,
                   nullptr);
        connect(slice, &pipeline::SliceSink::directionChanged, this,
                [this, slice]() {
                  if (slice != this->selectedSink()) {
                    disconnect(slice, nullptr, this, nullptr);
                    return;
                  }
                  this->populateProperties();
                  this->configurePropertyPage();
                });
      }
    } else if (property == "clip") {
      auto* clip = qobject_cast<pipeline::ClipSink*>(node);
      configuredClipOrtho = clip ? clip->isOrtho() : true;
      if (configuredClipOrtho) {
        label = "Slice:";
        decimals = 0;
        hi = clip ? clip->maxSlice() : 0;
        stopDefault = hi;
      } else {
        // A plane at an arbitrary angle has no slices to count, so it is
        // positioned by how far it sits from the centre of the data along
        // its own normal.
        label = "Position:";
        if (clip) {
          clip->planeDistanceRange(lo, hi);
        }
        startDefault = lo;
        stopDefault = hi;
      }
    } else if (property == "opacity") {
      label = "Opacity:";
      // Fading out from wherever the module sits now is the useful
      // default; the user can invert it by swapping the two values.
      startDefault = OpacityAnimation::opacityOf(node);
      stopDefault = 0.0;
    }

    configuredLo = lo;
    configuredHi = hi;

    ui.rangeLabel->setText(label);
    for (auto* spin : { ui.rangeStart, ui.rangeStop }) {
      QSignalBlocker blocked(spin);
      spin->setDecimals(decimals);
      spin->setRange(lo, hi);
    }
    ui.rangeStart->setValue(startDefault);
    ui.rangeStop->setValue(stopDefault);
  }

  // Re-read the selected module's data-dependent bounds after the
  // pipeline produces new data. Only rebuild when they actually changed,
  // so user-entered start/stop values survive executions that don't
  // affect this module.
  void refreshModuleRanges()
  {
    auto* node = selectedSink();
    QString property = selectedProperty();
    if (!node || property.isEmpty() || property == "curve") {
      return;
    }

    if (node != configuredNode || property != configuredProperty) {
      configurePropertyPage();
      return;
    }

    double lo = configuredLo;
    double hi = configuredHi;
    if (property == "iso") {
      auto* contour = qobject_cast<pipeline::ContourSink*>(node);
      double range[2] = { lo, hi };
      if (contour) {
        contour->scalarRange(range);
      }
      lo = range[0];
      hi = range[1];
    } else if (property == "slice") {
      auto* slice = qobject_cast<pipeline::SliceSink*>(node);
      hi = slice ? slice->maxSlice() : hi;
    } else if (property == "clip") {
      auto* clip = qobject_cast<pipeline::ClipSink*>(node);
      if (clip && clip->isOrtho() != configuredClipOrtho) {
        // A clip that changed orientation is measured in a different unit
        // entirely, and offers a differently named property.
        populateProperties();
        configurePropertyPage();
        return;
      }
      if (clip && clip->isOrtho()) {
        hi = clip->maxSlice();
      } else if (clip) {
        clip->planeDistanceRange(lo, hi);
      }
    }

    if (lo != configuredLo || hi != configuredHi) {
      configureRange(node, property);
    }
  }

  // Opacity curve keyframes
  //
  // The curve the histogram editor is showing for the selected volume,
  // which is the sink's own if it has been detached and the data's
  // otherwise.
  vtkPiecewiseFunction* liveOpacityCurve()
  {
    auto* sink = qobject_cast<pipeline::VolumeSink*>(selectedSink());
    if (!sink) {
      return nullptr;
    }
    auto* proxy = sink->opacityMap();
    return proxy ? vtkPiecewiseFunction::SafeDownCast(
                     proxy->GetClientSideObject())
                 : nullptr;
  }

  // If nothing is staged for this volume but an authored morph exists,
  // start from its keyframes so editing is a round trip rather than a
  // recapture.
  void seedStagedCurves(pipeline::Node* node)
  {
    if (!node || !stagedCurves.value(node).isEmpty()) {
      return;
    }

    for (auto* animation : ModuleAnimations::instance().animations()) {
      auto* morph = qobject_cast<ScalarOpacityAnimation*>(animation);
      if (!morph || morph->baseNode != node) {
        continue;
      }
      for (const auto& keyframe : morph->keyframes()) {
        auto copy = vtkSmartPointer<vtkPiecewiseFunction>::New();
        copy->DeepCopy(keyframe.curve);
        stagedCurves[node].insert(keyframe.anchor, copy);
      }
      return;
    }
  }

  // One row per viewpoint, or a plain start and end when there is no
  // camera path to key against.
  int keyframeRowCount()
  {
    return std::max(2, CameraViewpoints::instance().size());
  }

  void refreshKeyframeRows()
  {
    if (selectedProperty() != "curve") {
      return;
    }

    QSignalBlocker blocked(ui.keyframeList);
    int previousRow = ui.keyframeList->currentRow();
    ui.keyframeList->clear();

    auto* node = selectedSink();
    auto* volume = qobject_cast<pipeline::VolumeSink*>(node);
    if (!volume) {
      return;
    }

    // Both previews and the blend read the curves over the volume's own
    // window; drawn over their own ranges, a narrow spike and a
    // full-width ramp look identical.
    double range[2] = { 0.0, 1.0 };
    auto volumeData = volume->volumeData();
    if (volumeData && volumeData->isValid()) {
      auto volumeRange = volumeData->colorMapRange();
      range[0] = volumeRange[0];
      range[1] = volumeRange[1];
    }

    auto& viewpoints = CameraViewpoints::instance();
    auto staged = stagedCurves.value(node);
    int rows = keyframeRowCount();
    for (int anchor = 0; anchor < rows; ++anchor) {
      QString label;
      if (viewpoints.size() >= 2) {
        label = "At " + viewpoints.at(anchor).name;
      } else {
        label = anchor == 0 ? "At start" : "At end";
      }

      auto* item = new QListWidgetItem();
      if (staged.contains(anchor)) {
        item->setText(label);
        item->setIcon(QIcon(curvePreview(staged.value(anchor), range,
                                         ui.keyframeList->iconSize(),
                                         parent->palette())));
      } else {
        item->setText(label + "  (not captured)");
        // A blank icon keeps the labels aligned with the captured rows.
        QPixmap blank(ui.keyframeList->iconSize());
        blank.fill(Qt::transparent);
        item->setIcon(QIcon(blank));
      }
      ui.keyframeList->addItem(item);
    }

    if (previousRow >= 0 && previousRow < rows) {
      ui.keyframeList->setCurrentRow(previousRow);
    }
  }

  void captureCurve()
  {
    auto* node = selectedSink();
    auto* live = liveOpacityCurve();
    int anchor = ui.keyframeList->currentRow();
    if (!node || !live || anchor < 0) {
      return;
    }

    auto copy = vtkSmartPointer<vtkPiecewiseFunction>::New();
    copy->DeepCopy(live);
    // The histogram editor parks nodes at the ends of the data range
    // purely so the chart looks right. Saving them would make a captured
    // curve's point count depend on what a panel happened to be doing, and
    // the point count decides whether two curves can be blended point by
    // point.
    removePlaceholderNodes(copy);

    stagedCurves[node].insert(anchor, copy);
    refreshKeyframeRows();
    updateEnableStates();
  }

  // Put a captured curve back into the histogram editor, so it can be
  // looked at and edited. The counterpart of double clicking a viewpoint
  // to move the camera there.
  void loadCurveIntoEditor(QListWidgetItem* item)
  {
    auto* node = selectedSink();
    auto* live = liveOpacityCurve();
    int anchor = ui.keyframeList->row(item);
    if (!node || !live || anchor < 0) {
      return;
    }

    auto captured = stagedCurves.value(node).value(anchor);
    if (!captured) {
      return;
    }

    live->DeepCopy(captured);

    // Captures strip the nodes the editor parks at the ends of the data
    // range, so put them back or its chart stops spanning the range.
    auto* volume = qobject_cast<pipeline::VolumeSink*>(node);
    auto volumeData = volume ? volume->volumeData() : nullptr;
    if (volumeData && volumeData->isValid()) {
      auto colorRange = volumeData->colorMapRange();
      double range[2] = { colorRange[0], colorRange[1] };
      addPlaceholderNodes(live, range);
    }

    // The editor watches this function and redraws, re-renders and
    // mirrors the change into its proxy off the one event. Render
    // anyway, in case the editor is currently showing another data
    // source and nothing else would.
    live->Modified();
    activeObjects().renderAllViews();
  }

  void clearCurve()
  {
    auto* node = selectedSink();
    int anchor = ui.keyframeList->currentRow();
    if (!node) {
      return;
    }

    stagedCurves[node].remove(anchor);
    refreshKeyframeRows();
    updateEnableStates();
  }

  void onNodeAdded(pipeline::Node*)
  {
    QTimer::singleShot(0, this, [this]() {
      updateDataSourceOptions();
      updateModuleOptions();
    });
  }

  void onNodeRemoved(pipeline::Node* node)
  {
    stagedCurves.remove(node);
    updateDataSourceOptions();
    updateModuleOptions();
    updateEnableStates();
  }

  // Adding an animation
  void addModuleAnimation()
  {
    auto* node = selectedSink();
    if (!node) {
      return;
    }

    QString property = selectedProperty();
    auto segmentData = ui.animationSegment->currentData();
    int segment = property == "curve"
                    ? -1
                    : (segmentData.isValid() ? segmentData.toInt() : -1);

    ModuleAnimation* animation = buildAnimation(node, property);
    if (!animation) {
      return;
    }
    animation->segment = segment;

    // One animation per visualization, property and leg: adding the same
    // thing again replaces it, while the same property on another leg is
    // a second animation.
    for (auto* existing : ModuleAnimations::instance().animations()) {
      if (existing->baseNode == node &&
          existing->type() == animation->type() &&
          (property == "curve" || existing->segment == segment)) {
        ModuleAnimations::instance().remove(existing);
      }
    }

    ModuleAnimations::instance().add(animation);
    ensureAnimationFrames();
    // Same reason as the camera path: put the module at its starting
    // value rather than leaving it wherever it was until the clock
    // first moves.
    animation->onTimeChanged();

    updateEnableStates();
    play();
  }

  ModuleAnimation* buildAnimation(pipeline::Node* node,
                                  const QString& property)
  {
    double start = ui.rangeStart->value();
    double stop = ui.rangeStop->value();

    if (property == "iso") {
      if (auto* contour = qobject_cast<pipeline::ContourSink*>(node)) {
        return new ContourAnimation(contour, start, stop);
      }
    } else if (property == "slice") {
      if (auto* slice = qobject_cast<pipeline::SliceSink*>(node)) {
        return new SliceAnimation(slice, start, stop);
      }
    } else if (property == "clip") {
      if (auto* clip = qobject_cast<pipeline::ClipSink*>(node)) {
        auto unit =
          clip->isOrtho() ? ClipAnimation::Slice : ClipAnimation::Distance;
        return new ClipAnimation(clip, start, stop, unit);
      }
    } else if (property == "opacity") {
      if (OpacityAnimation::supports(node)) {
        return new OpacityAnimation(node, start, stop);
      }
    } else if (property == "curve") {
      auto* volume = qobject_cast<pipeline::VolumeSink*>(node);
      if (volume && ScalarOpacityAnimation::supports(node)) {
        QList<OpacityKeyframe> keyframes;
        auto staged = stagedCurves.value(node);
        const int rows = keyframeRowCount();
        for (auto it = staged.constBegin(); it != staged.constEnd(); ++it) {
          // Curves captured against viewpoints that have since been
          // removed are kept in case those viewpoints come back, but
          // they are not part of what the list is currently offering.
          if (it.key() >= rows) {
            continue;
          }
          OpacityKeyframe keyframe;
          keyframe.anchor = it.key();
          keyframe.curve = it.value();
          keyframes.append(keyframe);
        }
        if (keyframes.size() >= 2) {
          return new ScalarOpacityAnimation(volume, keyframes);
        }
      }
    }

    return nullptr;
  }

  // The list of everything that will animate, one row per animation, so
  // authoring is visible state rather than something to remember.
  void refreshAnimationList()
  {
    ui.animationList->clear();

    for (auto* animation : ModuleAnimations::instance().animations()) {
      QString nodeLabel =
        animation->baseNode ? animation->baseNode->label() : QString();
      if (nodeLabel.isEmpty()) {
        nodeLabel = "Visualization";
      }

      QString text = nodeLabel + ": " + animation->describeParameters();
      if (animation->segment >= 0) {
        text += ", during " + segmentLabel(animation->segment);
      }

      auto* row = new QWidget();
      auto* layout = new QHBoxLayout(row);
      layout->setContentsMargins(4, 1, 4, 1);
      auto* label = new QLabel(text, row);
      layout->addWidget(label);
      layout->addStretch();
      auto* removeButton = new QToolButton(row);
      removeButton->setText("x");
      removeButton->setAutoRaise(true);
      removeButton->setToolTip("Remove this animation");
      layout->addWidget(removeButton);

      // Deferred: the button lives in the row this removal deletes.
      QPointer<ModuleAnimation> target(animation);
      connect(removeButton, &QToolButton::clicked, this, [target]() {
        QTimer::singleShot(0, [target]() {
          if (target) {
            ModuleAnimations::instance().remove(target);
          }
        });
      });

      auto* item = new QListWidgetItem();
      item->setSizeHint(row->sizeHint());
      ui.animationList->addItem(item);
      ui.animationList->setItemWidget(item, row);
    }
  }

  // All animations
  void numberOfFramesModified()
  {
    pqSMAdaptor::setEnumerationProperty(
      scene()->getProxy()->GetProperty("PlayMode"), "Sequence");
    // qtWidgetChanged is emitted after the property link has copied and
    // flushed the frame count, so the play mode needs its own push to
    // reach the animation player.
    scene()->getProxy()->UpdateVTKObjects();
  }

  void exportMovie()
  {
    MovieExportDialog dialog(parent);
    dialog.exec();
  }

  void clearAllAnimations()
  {
    clearCameraCues();
    CameraViewpoints::instance().stopFlight();
    if (ui.enableTimeSeriesAnimations->isVisible()) {
      ui.enableTimeSeriesAnimations->setChecked(false);
    }
    ModuleAnimations::instance().clear();

    updateEnableStates();
  }

  ActiveObjects& activeObjects() { return ActiveObjects::instance(); }

  pipeline::Pipeline* pipeline()
  {
    return activeObjects().pipeline();
  }

  pqAnimationScene* scene()
  {
    return pqPVApplicationCore::instance()
      ->animationManager()
      ->getActiveScene();
  }
}; // end class Internal

AnimationHelperDialog::AnimationHelperDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
  // Float above the main window so the dialog does not slip behind it on
  // macOS.
  floatAboveMainWindow(this);
}

AnimationHelperDialog::~AnimationHelperDialog() = default;

void AnimationHelperDialog::showEvent(QShowEvent* e)
{
  QDialog::showEvent(e);
  m_internal->refresh();
}

} // namespace tomviz
