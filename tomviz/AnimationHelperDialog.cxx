/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationHelperDialog.h"

#include "animations/AnimationSceneGuard.h"
#include "ui_AnimationHelperDialog.h"

#include "ActiveObjects.h"
#include "CameraAnimation.h"
#include "CameraViewpoints.h"
#include "AnimatableProperties.h"
#include "ModuleAnimations.h"
#include "MovieExportDialog.h"
#include "RecordedAnimations.h"
#include "ScalarOpacityAnimation.h"
#include "SceneSnapshot.h"
#include "Utilities.h"

#include "pipeline/Pipeline.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/SourceNode.h"
#include "pipeline/sinks/ClipSink.h"
#include "pipeline/sinks/ContourSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <functional>
#include <optional>

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
#include <QSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
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

#include <cstdlib>

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
  // Where every caption is centered, as fractions of the view from its
  // lower-left corner
  QDoubleSpinBox* captionX = nullptr;
  QDoubleSpinBox* captionY = nullptr;
  // The orbit at the selected viewpoint
  QCheckBox* viewpointOrbit = nullptr;
  QSpinBox* orbitTurns = nullptr;
  QComboBox* orbitDirection = nullptr;
  QSpinBox* orbitFrames = nullptr;
  QLabel* orbitFramesTitle = nullptr;
  QCheckBox* orbitEased = nullptr;
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
  // What each row of the animation list puts into the controls above
  // when it is selected, in row order.
  QList<std::function<void()>> animationRowSelectors;
  QString configuredProperty;
  double configuredLo = 0.0;
  double configuredHi = 0.0;
  // The range caption: a slice or clip that turns custom is measured in
  // a different unit and offers a differently named property.
  QString configuredLabel;

  Internal(AnimationHelperDialog* p) : QObject(p), parent(p)
  {
    ui.setupUi(p);

    auto* labelRow = new QHBoxLayout;
    auto* labelTitle = new QLabel("Label:", parent);
    viewpointLabel = new QLineEdit(parent);
    viewpointLabel->setPlaceholderText(
      "Caption shown transitioning to the next viewpoint");
    viewpointLabel->setToolTip(
      "Text drawn in the 3D view, and in exported movies, while the camera "
      "is at this viewpoint (including its orbit) and on its way to the "
      "next one.");
    labelTitle->setBuddy(viewpointLabel);
    labelRow->addWidget(labelTitle);
    labelRow->addWidget(viewpointLabel, 1);
    ui.cameraLayout->insertLayout(
      ui.cameraLayout->indexOf(ui.segmentLayout) + 1, labelRow);
    connect(viewpointLabel, &QLineEdit::editingFinished, this,
            &Internal::labelChanged);

    auto* positionRow = new QHBoxLayout;
    auto* positionTitle = new QLabel("Label position:", parent);
    positionTitle->setToolTip(
      "Where the captions are centered in the 3D view and in exported "
      "movies, as a fraction of the view's width and height measured from "
      "the lower-left corner. 0.5, 0.5 is the middle of the view; the "
      "default, 0.5, 0.05, is the bottom middle. One position serves "
      "every viewpoint.");
    positionRow->addWidget(positionTitle);
    auto addAxis = [&](const QString& axis) {
      auto* box = new QDoubleSpinBox(parent);
      box->setRange(0.0, 1.0);
      box->setSingleStep(0.01);
      box->setDecimals(2);
      // Commit once editing is done: a value committed per keystroke is
      // reflected straight back into the box, which reformats the text
      // under the user and swallows the digit they were about to type
      box->setKeyboardTracking(false);
      box->setToolTip(positionTitle->toolTip());
      auto* axisLabel = new QLabel(axis, parent);
      axisLabel->setBuddy(box);
      positionRow->addWidget(axisLabel);
      positionRow->addWidget(box);
      return box;
    };
    captionX = addAxis("x");
    captionY = addAxis("y");
    positionRow->addStretch(1);
    ui.cameraLayout->insertLayout(ui.cameraLayout->indexOf(labelRow) + 1,
                                  positionRow);
    refreshCaptionPosition();
    auto positionEdited = [this]() {
      CameraViewpoints::instance().setCaptionPosition(captionX->value(),
                                                      captionY->value());
    };
    connect(captionX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            positionEdited);
    connect(captionY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            positionEdited);
    // A loaded state file moves it from outside the dialog
    connect(&CameraViewpoints::instance(),
            &CameraViewpoints::captionPositionChanged, this,
            &Internal::refreshCaptionPosition);

    // The orbit at the viewpoint, above the leg controls: what happens
    // on arriving here comes before what happens on leaving.
    auto* orbitRow = new QHBoxLayout;
    viewpointOrbit = new QCheckBox("Add orbit", parent);
    viewpointOrbit->setObjectName("viewpointOrbit");
    viewpointOrbit->setToolTip(
      "Once the camera reaches this viewpoint, swing it around the focal "
      "point before it moves on: full turns, in this direction, lasting "
      "this long relative to the legs of the path. The last viewpoint can "
      "orbit too, to end on a spin, and a single viewpoint that orbits is "
      "an animation on its own: what Play makes of the current view when "
      "nothing else is set up.");
    orbitTurns = new QSpinBox(parent);
    orbitTurns->setObjectName("orbitTurns");
    orbitTurns->setRange(1, 10);
    orbitTurns->setSuffix(" turn(s)");
    // Typed values commit once editing is done: every change rebuilds
    // the viewpoint list, which would otherwise reset the text mid-edit
    orbitTurns->setKeyboardTracking(false);
    orbitTurns->setToolTip("Full turns around the focal point.");
    orbitDirection = new QComboBox(parent);
    orbitDirection->setObjectName("orbitDirection");
    orbitDirection->addItems({ "Counterclockwise", "Clockwise" });
    orbitDirection->setToolTip(
      "Which way the camera goes round, as seen from above (looking down "
      "the view's up direction).");
    orbitFramesTitle = new QLabel("lasting", parent);
    orbitFrames = new QSpinBox(parent);
    orbitFrames->setObjectName("orbitFrames");
    // Never zero: an orbit of no length would be a checkbox that does
    // nothing
    orbitFrames->setRange(1, 100000);
    orbitFrames->setSingleStep(10);
    orbitFrames->setSuffix(" frames");
    orbitFrames->setValue(120);
    orbitFrames->setKeyboardTracking(false);
    orbitFrames->setToolTip(
      "Frames the orbit takes, on top of the legs.");
    orbitFramesTitle->setBuddy(orbitFrames);
    orbitEased = new QCheckBox("Ease In/Out", parent);
    orbitEased->setObjectName("orbitEased");
    orbitEased->setToolTip(
      "Start the orbit slowly and slow it to a stop at the end. Leave off "
      "for a spin at constant speed, which loops without a pause.");
    orbitRow->addWidget(viewpointOrbit);
    orbitRow->addWidget(orbitTurns);
    orbitRow->addWidget(orbitDirection);
    orbitRow->addWidget(orbitFramesTitle);
    orbitRow->addWidget(orbitFrames);
    orbitRow->addWidget(orbitEased);
    orbitRow->addStretch(1);
    // The details only appear once there is an orbit to describe
    for (auto* detail : orbitDetails()) {
      detail->setVisible(false);
    }
    ui.cameraLayout->insertLayout(ui.cameraLayout->indexOf(ui.segmentLayout),
                                  orbitRow);
    connect(viewpointOrbit, &QCheckBox::toggled, this, &Internal::orbitChanged);
    connect(orbitTurns, qOverload<int>(&QSpinBox::valueChanged), this,
            &Internal::orbitChanged);
    connect(orbitDirection, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &Internal::orbitChanged);
    connect(orbitFrames, qOverload<int>(&QSpinBox::valueChanged), this,
            &Internal::orbitChanged);
    connect(orbitEased, &QCheckBox::toggled, this, &Internal::orbitChanged);

    recordScene = new QCheckBox("Record module state with viewpoints", parent);
    recordScene->setToolTip(
      "Also save the state of every visualization with each viewpoint: "
      "which are visible, their opacity or opacity curve, volume "
      "solidity, where slice and clip planes sit, iso values, threshold "
      "ranges, hidden labels, and any volume cut-out or exploded view. Whatever differs between two "
      "viewpoints is listed under Visualizations, marked recorded, and "
      "plays between them. A module added after a viewpoint was saved "
      "counts as hidden there until that viewpoint is updated.");
    recordScene->setChecked(true);
    ui.cameraLayout->addWidget(recordScene);

    ui.viewpointList->setIconSize(QSize(96, 72));
    ui.viewpointList->setDragDropMode(QAbstractItemView::InternalMove);
    ui.viewpointList->setDefaultDropAction(Qt::MoveAction);
    ui.viewpointList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui.viewpointList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.viewpointList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui.keyframeList->setIconSize(QSize(96, 28));

    // The viewpoint and leg controls start out matching the (empty)
    // selection, like the ones the .ui file starts disabled
    updateSegmentControls();
    updateGui();
    setupConnections();
  }

  void setupConnections()
  {
    // No control arms the camera path: it flies whenever there is one
    // (see CameraViewpoints::syncFlight), and Clear All Animations is
    // how it stops.
    ui.clearAllAnimations->setToolTip(
      "Remove every animation: the viewpoints and their path, the module "
      "state recorded with them, the visualization animations, and the "
      "time series playback.");

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
    connect(ui.segmentDuration, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Internal::segmentChanged);
    connect(ui.segmentEased, &QCheckBox::toggled, this,
            &Internal::segmentChanged);
    // The list is shared with the state file, so it can change while the
    // dialog is open. Everything showing viewpoint names follows it.
    connect(&CameraViewpoints::instance(), &CameraViewpoints::changed, this,
            [this]() {
              // The flight and the playback follow the change on their
              // own (AnimationSceneGuard); this is the display.
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
              interruptAnimationPlayback();
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
    // Selecting a row shows that animation in the controls above, so it
    // can be read off and, with Add, adjusted
    connect(ui.animationList, &QListWidget::currentRowChanged, this,
            &Internal::selectAnimationRow);
    connect(ui.animationList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
              selectAnimationRow(ui.animationList->row(item));
            });
    connect(&ModuleAnimations::instance(), &ModuleAnimations::changed, this,
            [this]() {
              refreshAnimationList();
              // The keyframe list shows the recorded curves, which are
              // rebuilt whenever the list changes
              refreshKeyframeRows();
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
    bool hasCameraAnimations = CameraViewpoints::instance().isFlying();

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

    int selectedViewpoint = ui.viewpointList->currentRow();
    bool viewpointSelected = selectedViewpoint >= 0;

    ui.addViewpoint->setEnabled(renderViewForOrbit() != nullptr);
    ui.updateViewpoint->setEnabled(viewpointSelected);
    ui.removeViewpoint->setEnabled(viewpointSelected);

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

    // With a path the frame count is what its legs and orbits add up to,
    // so the box gives way to the total; without one it is the count.
    auto& viewpoints = CameraViewpoints::instance();
    const bool pathSetsFrames = viewpoints.isPath();
    ui.numberOfFrames->setVisible(!pathSetsFrames);
    ui.numberOfFramesLabel->setText(
      pathSetsFrames ? tr("Total: %1 frames").arg(viewpoints.totalFrames())
                     : tr("Number of Frames:"));
    ui.numberOfFramesLabel->setToolTip(
      pathSetsFrames ? tr("The legs and orbits of the camera path added up.")
                     : QString());

    bool hasAnyAnimations =
      hasCameraAnimations || timeSeriesEnabled || hasModuleAnimations;
    ui.exportMovie->setEnabled(hasAnyAnimations);
    // A lone viewpoint is not yet an animation, but it is something to
    // clear
    ui.clearAllAnimations->setEnabled(hasAnyAnimations ||
                                      CameraViewpoints::instance().size() > 0);
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
      const int turns = viewpoints.at(i).orbitTurns;
      if (turns != 0) {
        item->setToolTip(QString("Orbits %1 full turn(s) %2 here")
                           .arg(std::abs(turns))
                           .arg(turns > 0 ? "counterclockwise" : "clockwise"));
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
    // The menu runs its own event loop, during which the list can be
    // rebuilt (a playback stopping, a state file), so hold the row, not
    // the item, and look the item up again afterwards.
    const int row = ui.viewpointList->row(item);
    ui.viewpointList->setCurrentRow(row);

    QMenu menu;
    auto* rename = menu.addAction("Rename");
    auto* goTo = menu.addAction("Go To");
    auto* update = menu.addAction("Update From Current View");
    auto* remove = menu.addAction("Remove");
    auto* chosen = menu.exec(ui.viewpointList->mapToGlobal(pos));
    item = ui.viewpointList->item(row);
    if (!chosen || !item) {
      return;
    }
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

  // itemChanged fires from inside the list widget's own edit commit,
  // and replacing the viewpoint rebuilds the list, which would delete
  // the item Qt is still working with: the crash on the next right
  // click. So take the row and the text now, and apply them once the
  // widget is done. It also fires for the editable flag Rename sets
  // just before opening the editor, when the text is unchanged: that
  // must not rebuild the list either, or the editor is gone before the
  // user has typed a letter.
  void commitViewpointRename(QListWidgetItem* item)
  {
    const int row = ui.viewpointList->row(item);
    const auto name = item->text().trimmed();
    QTimer::singleShot(0, this, [this, row, name]() {
      auto& viewpoints = CameraViewpoints::instance();
      if (row < 0 || row >= viewpoints.size()) {
        return;
      }
      auto viewpoint = viewpoints.at(row);
      if (name == viewpoint.name) {
        return;
      }
      if (name.isEmpty()) {
        // Rejected edit; put the old name back.
        refreshViewpoints();
        return;
      }
      viewpoint.name = name;
      viewpoints.replace(row, viewpoint);
    });
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
    QSignalBlocker blockedOrbit(viewpointOrbit);
    QSignalBlocker blockedTurns(orbitTurns);
    QSignalBlocker blockedDirection(orbitDirection);
    QSignalBlocker blockedOrbitFrames(orbitFrames);
    QSignalBlocker blockedOrbitEased(orbitEased);
    QSignalBlocker blockedLabel(viewpointLabel);
    if (hasSegment) {
      ui.segmentDuration->setValue(viewpoints.at(row).legFrames);
      ui.segmentEased->setChecked(viewpoints.at(row).eased);
    }
    bool hasViewpoint = row >= 0 && row < viewpoints.size();
    bool orbiting = false;
    if (hasViewpoint) {
      const int turns = viewpoints.at(row).orbitTurns;
      orbiting = turns != 0;
      orbitTurns->setValue(orbiting ? std::abs(turns) : 1);
      orbitDirection->setCurrentIndex(turns < 0 ? 1 : 0);
      orbitFrames->setValue(viewpoints.at(row).orbitFrames);
      orbitEased->setChecked(viewpoints.at(row).orbitEased);
    }
    viewpointOrbit->setChecked(orbiting);
    viewpointLabel->setText(hasViewpoint ? viewpoints.at(row).label
                                         : QString());
    viewpointLabel->setEnabled(hasViewpoint);

    ui.segmentDurationLabel->setEnabled(hasSegment);
    ui.segmentDuration->setEnabled(hasSegment);
    ui.segmentEased->setEnabled(hasSegment);
    viewpointOrbit->setEnabled(hasViewpoint);
    for (auto* detail : orbitDetails()) {
      detail->setVisible(hasViewpoint && orbiting);
    }
  }

  // The orbit controls other than the checkbox that shows them
  QList<QWidget*> orbitDetails() const
  {
    return { orbitTurns, orbitDirection, orbitFramesTitle, orbitFrames,
             orbitEased };
  }

  void orbitChanged()
  {
    auto& viewpoints = CameraViewpoints::instance();
    int row = ui.viewpointList->currentRow();
    if (row < 0 || row >= viewpoints.size()) {
      return;
    }

    auto viewpoint = viewpoints.at(row);
    const int turns = orbitTurns->value();
    viewpoint.orbitTurns =
      viewpointOrbit->isChecked()
        ? (orbitDirection->currentIndex() == 0 ? turns : -turns)
        : 0;
    viewpoint.orbitFrames = orbitFrames->value();
    viewpoint.orbitEased = orbitEased->isChecked();
    viewpoints.replace(row, viewpoint);
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

    // The flight would put the camera back on the path at its next
    // tick; stopped where it is, the next Play resumes from there.
    interruptAnimationPlayback(/*rewind=*/false);
    viewpoints.at(row).applyTo(context.camera);
    // Modules some viewpoint recorded but this one lacks were not on
    // screen here; ones no viewpoint knows are left as they are.
    auto known = RecordedAnimations::instance().recordedNodeIds();
    viewpoints.at(row).scene.apply(pipeline(), &known);
    if (auto* renderer = context.proxy->GetRenderer()) {
      renderer->ResetCameraClippingRange();
    }
    context.view->render();
  }

  void removeViewpoint()
  {
    CameraViewpoints::instance().removeAt(ui.viewpointList->currentRow());
  }

  void refreshCaptionPosition()
  {
    const auto position = CameraViewpoints::instance().captionPosition();
    QSignalBlocker blockX(captionX);
    QSignalBlocker blockY(captionY);
    captionX->setValue(position[0]);
    captionY->setValue(position[1]);
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
    viewpoint.legFrames = ui.segmentDuration->value();
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
      if (hasAnimatableProperties(node) ||
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
    // The curve morph has its own page; every swept property comes from
    // the table
    if (ScalarOpacityAnimation::supports(node)) {
      ui.animatedProperty->addItem("Opacity curve", "curve");
    }
    for (const auto& property : animatableProperties()) {
      if (node && property.applies(node)) {
        ui.animatedProperty->addItem(property.label(node), property.id);
      }
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

    PropertyRange range;
    if (const auto* animatable = animatableProperty(property)) {
      range = animatable->range(node);
    }
    const QString label = range.label;
    const int decimals = range.decimals;
    const double lo = range.lo;
    const double hi = range.hi;
    const double startDefault = range.start;
    const double stopDefault = range.stop;
    configuredLabel = label;

    // A slice or clip that turns custom, or back, is measured in a
    // different unit and offers a differently named property. This
    // reconfigures on every reselection and data update, so drop the
    // previous connection first or they accumulate.
    auto follow = [this](auto* sink) {
      using SinkT = std::remove_pointer_t<decltype(sink)>;
      disconnect(sink, &SinkT::directionChanged, this, nullptr);
      connect(sink, &SinkT::directionChanged, this, [this, sink]() {
        if (sink != this->selectedSink()) {
          disconnect(sink, nullptr, this, nullptr);
          return;
        }
        this->populateProperties();
        this->configurePropertyPage();
      });
    };
    if (auto* slice = qobject_cast<pipeline::SliceSink*>(node)) {
      follow(slice);
    } else if (auto* clip = qobject_cast<pipeline::ClipSink*>(node)) {
      follow(clip);
    } else if (auto* volume = qobject_cast<pipeline::VolumeSink*>(node)) {
      // The exploded offset's bound follows the slab count and the
      // direction, which the panel can change while this is showing
      disconnect(volume, &pipeline::VolumeSink::explodedChanged, this,
                 nullptr);
      connect(volume, &pipeline::VolumeSink::explodedChanged, this,
              [this, volume]() {
                if (volume != this->selectedSink()) {
                  disconnect(volume, &pipeline::VolumeSink::explodedChanged,
                             this, nullptr);
                  return;
                }
                this->refreshModuleRanges();
              });
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

    const auto* animatable = animatableProperty(property);
    if (!animatable) {
      return;
    }
    const PropertyRange range = animatable->range(node);
    if (range.label != configuredLabel) {
      // A plane that changed orientation is measured in a different unit
      // entirely, and offers a differently named property.
      populateProperties();
      configurePropertyPage();
      return;
    }
    if (range.lo != configuredLo || range.hi != configuredHi) {
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
      // Only a morph authored here: the recorded one belongs to the
      // viewpoints and is edited by updating them
      if (!morph || morph->recorded() || morph->baseNode != node) {
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

  // The curves the viewpoints recorded for this volume, keyed by
  // viewpoint, or an empty map when nothing was recorded or an authored
  // morph has taken the curve over.
  QMap<int, vtkSmartPointer<vtkPiecewiseFunction>> recordedCurves(
    pipeline::Node* node)
  {
    QMap<int, vtkSmartPointer<vtkPiecewiseFunction>> curves;
    for (auto* animation : ModuleAnimations::instance().animations()) {
      auto* morph = qobject_cast<ScalarOpacityAnimation*>(animation);
      if (!morph || !morph->recorded() || morph->baseNode != node) {
        continue;
      }
      for (const auto& keyframe : morph->keyframes()) {
        curves.insert(keyframe.anchor, keyframe.curve);
      }
    }
    return curves;
  }

  // The first edit to a volume's keyframes starts from what the
  // viewpoints recorded, so the recorded morph can be adjusted rather
  // than recaptured anchor by anchor. Adding the result authors a morph
  // that takes the curve over from the recorded one.
  void seedStagedFromRecorded(pipeline::Node* node)
  {
    if (!node || !stagedCurves.value(node).isEmpty()) {
      return;
    }
    auto recorded = recordedCurves(node);
    for (auto it = recorded.constBegin(); it != recorded.constEnd(); ++it) {
      auto copy = vtkSmartPointer<vtkPiecewiseFunction>::New();
      copy->DeepCopy(it.value());
      stagedCurves[node].insert(it.key(), copy);
    }
  }

  static bool curveIsAllZero(vtkPiecewiseFunction* curve)
  {
    double node[4];
    for (int i = 0; curve && i < curve->GetSize(); ++i) {
      curve->GetNodeValue(i, node);
      if (node[1] > 0.0) {
        return false;
      }
    }
    return true;
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
    // Until the user edits, the list shows what the viewpoints recorded
    // for this volume, so the recorded morph is something to look at
    // rather than take on faith.
    const bool showingRecorded = staged.isEmpty();
    auto recorded = showingRecorded ? recordedCurves(node) : decltype(staged)();
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
      } else if (recorded.contains(anchor)) {
        auto curve = recorded.value(anchor);
        item->setText(label + (curveIsAllZero(curve) ? "  (recorded: hidden)"
                                                     : "  (recorded)"));
        item->setToolTip("Recorded with the viewpoint. Update the viewpoint "
                         "to change it, or capture over it here to start a "
                         "morph of your own from the recorded curves.");
        item->setIcon(QIcon(curvePreview(curve, range,
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

    seedStagedFromRecorded(node);
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

    seedStagedFromRecorded(node);
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
    // Recorded animations are not replaced here: they step aside from
    // the legs an authored one runs on by themselves.
    for (auto* existing : ModuleAnimations::instance().animations()) {
      if (!existing->recorded() && existing->baseNode == node &&
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

    if (const auto* animatable = animatableProperty(property)) {
      return animatable->make(node, start, stop);
    }
    if (property == "curve") {
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

  // One row of the animation list, with its remove button. The removal
  // is deferred because the button lives in the row it deletes.
  void addAnimationRow(const QString& text, const QString& tooltip,
                       const QString& removeTooltip,
                       std::function<void()> remove,
                       std::function<void()> select)
  {
    animationRowSelectors.append(select);
    auto* row = new QWidget();
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(4, 1, 4, 1);
    auto* label = new QLabel(text, row);
    label->setToolTip(tooltip);
    layout->addWidget(label);
    layout->addStretch();
    auto* removeButton = new QToolButton(row);
    removeButton->setText("x");
    removeButton->setAutoRaise(true);
    removeButton->setToolTip(removeTooltip);
    layout->addWidget(removeButton);
    connect(removeButton, &QToolButton::clicked, this, [remove]() {
      QTimer::singleShot(0, remove);
    });

    auto* item = new QListWidgetItem();
    item->setSizeHint(row->sizeHint());
    ui.animationList->addItem(item);
    ui.animationList->setItemWidget(item, row);
  }

  QString nodeLabel(pipeline::Node* node)
  {
    QString label = node ? node->label() : QString();
    return label.isEmpty() ? QString("Visualization") : label;
  }

  // The list of everything that will animate, one row per animation, so
  // authoring is visible state rather than something to remember. The
  // animations authored here come first; then, marked as such, every
  // change the camera viewpoints recorded.
  void refreshAnimationList()
  {
    QSignalBlocker blocked(ui.animationList);
    ui.animationList->clear();
    animationRowSelectors.clear();

    for (auto* animation : ModuleAnimations::instance().animations()) {
      if (animation->recorded()) {
        continue;
      }
      QString text =
        nodeLabel(animation->baseNode) + ": " + animation->describeParameters();
      if (animation->segment >= 0) {
        text += ", during " + segmentLabel(animation->segment);
      }
      QPointer<ModuleAnimation> target(animation);
      addAnimationRow(
        text, QString(), "Remove this animation",
        [target]() {
          if (target) {
            ModuleAnimations::instance().remove(target);
          }
        },
        [this, target]() {
          if (target) {
            showAnimationInControls(target);
          }
        });
    }

    auto& viewpoints = CameraViewpoints::instance();
    auto* pip = pipeline();
    for (const auto& change : RecordedAnimations::instance().changes(pip)) {
      auto* node = pip ? pip->nodeById(change.nodeId) : nullptr;
      const QString from = viewpoints.at(change.fromAnchor).name;
      const QString to = viewpoints.at(change.toAnchor).name;
      QString text = nodeLabel(node) + ": " + change.description + ", " +
                     from + " to " + to + "  (recorded)";
      QString tooltip =
        "Recorded with the viewpoints: this is how the visualization "
        "differs between " + from + " and " + to + ". Update " + to +
        " from the view to change it.";
      QString removeTooltip = "Keep it as it is at " + from +
                              " on this leg (edits " + to + ")";
      addAnimationRow(
        text, tooltip, removeTooltip,
        [change, pip]() { RecordedAnimations::instance().remove(change, pip); },
        [this, change, node]() {
          // A stretch over blank viewpoints is several legs; the combo
          // takes one, so offer the first and let the user pick another
          showInControls(node,
                         change.controlProperty.isEmpty()
                           ? change.property
                           : change.controlProperty,
                         change.startValue, change.stopValue,
                         change.fromAnchor);
        });
    }
  }

  void selectAnimationRow(int row)
  {
    if (row >= 0 && row < animationRowSelectors.size() &&
        animationRowSelectors[row]) {
      animationRowSelectors[row]();
    }
  }

  // Put an authored animation into the controls: its visualization,
  // property, values and leg. Adding again then replaces it.
  void showAnimationInControls(ModuleAnimation* animation)
  {
    auto* node = animation ? animation->baseNode.data() : nullptr;
    if (!node) {
      return;
    }
    std::optional<double> start;
    std::optional<double> stop;
    QString property = animation->type();
    double from = 0.0;
    double to = 0.0;
    if (const auto* animatable = animatablePropertyOf(animation, from, to)) {
      property = animatable->id;
      start = from;
      stop = to;
    } else if (qobject_cast<ScalarOpacityAnimation*>(animation)) {
      property = "curve";
    }
    showInControls(node, property, start, stop, animation->segment);
  }

  // Select @a node's data source, the node, and @a property in the
  // authoring controls, then the values and the leg. Properties the
  // controls cannot author (visibility, hidden labels) select the
  // visualization and leave the property as it was.
  void showInControls(pipeline::Node* node, const QString& property,
                      std::optional<double> start, std::optional<double> stop,
                      int segment)
  {
    if (!node) {
      return;
    }
    auto* source = findUpstreamSource(node);
    int sourceIndex = ui.selectedDataSource->findData(
      QVariant::fromValue(static_cast<QObject*>(source)));
    if (sourceIndex >= 0 && sourceIndex != ui.selectedDataSource->currentIndex()) {
      QSignalBlocker blocked(ui.selectedDataSource);
      ui.selectedDataSource->setCurrentIndex(sourceIndex);
      updateModuleOptions();
    }
    int moduleIndex = ui.selectedModule->findData(
      QVariant::fromValue(static_cast<QObject*>(node)));
    if (moduleIndex < 0) {
      return;
    }
    if (moduleIndex != ui.selectedModule->currentIndex()) {
      QSignalBlocker blocked(ui.selectedModule);
      ui.selectedModule->setCurrentIndex(moduleIndex);
      selectedModuleChanged();
    }
    int propertyIndex = ui.animatedProperty->findData(property);
    if (propertyIndex >= 0 &&
        propertyIndex != ui.animatedProperty->currentIndex()) {
      QSignalBlocker blocked(ui.animatedProperty);
      ui.animatedProperty->setCurrentIndex(propertyIndex);
      configurePropertyPage();
    }
    if (propertyIndex >= 0 && property != "curve") {
      if (start) {
        ui.rangeStart->setValue(*start);
      }
      if (stop) {
        ui.rangeStop->setValue(*stop);
      }
      int segmentIndex = ui.animationSegment->findData(segment);
      if (segmentIndex >= 0) {
        ui.animationSegment->setCurrentIndex(segmentIndex);
      }
    }
    updateEnableStates();
  }

  // All animations
  void numberOfFramesModified()
  {
    interruptAnimationPlayback();
    pqSMAdaptor::setEnumerationProperty(
      scene()->getProxy()->GetProperty("PlayMode"), "Sequence");
    // qtWidgetChanged is emitted after the property link has copied and
    // flushed the frame count, so the play mode needs its own push to
    // reach the animation player.
    scene()->getProxy()->UpdateVTKObjects();
  }

  void exportMovie() { MovieExportDialog::exportMovie(parent); }

  void clearAllAnimations()
  {
    auto& viewpoints = CameraViewpoints::instance();
    // The viewpoints are the camera animation, so they go with the rest;
    // clearing them stops the flight.
    viewpoints.clear();
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
