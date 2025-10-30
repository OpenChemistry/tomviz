/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ViewMenuManager.h"

#include <pqCoreUtilities.h>
#include <pqRenderView.h>
#include <pqView.h>
#include <vtkPVRenderView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkGridAxesActor3D.h>
#include <vtkImageData.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkTextProperty.h>

#include <QAction>
#include <QActionGroup>
#include <QDebug>
#include <QDialog>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "CameraReaction.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "ModuleSlice.h"
#include "SliceViewDialog.h"
#include "Utilities.h"

namespace tomviz {

class PreviousImageViewerSettings
{
public:
  vtkNew<vtkCamera> camera;
  QString projection;
  bool newSliceModule;
  QPointer<ModuleSlice> sliceModule;
  QJsonObject sliceModuleSettings;
  QList<QPointer<Module>> visibleModules;
  int interactionMode;

  void clear()
  {
    // Only clear whatever needs to be cleared
    visibleModules.clear();
    newSliceModule = false;
    sliceModule = nullptr;
    sliceModuleSettings = QJsonObject();
  };
};

ViewMenuManager::ViewMenuManager(QMainWindow* mainWindow, QMenu* menu)
  : pqViewMenuManager(mainWindow, menu), m_perspectiveProjectionAction(nullptr),
    m_orthographicProjectionAction(nullptr)
{
  m_view = ActiveObjects::instance().activeView();
  if (m_view) {
    m_viewObserverId =
      pqCoreUtilities::connect(m_view, vtkCommand::PropertyModifiedEvent, this,
                               SLOT(onViewPropertyChanged()));
  }
  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          SLOT(onViewChanged()));

  connect(&ActiveObjects::instance(), &ActiveObjects::dataSourceActivated, this,
          &ViewMenuManager::updateDataSource);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::transformedDataSourceActivated, this,
          &ViewMenuManager::updateDataSource);
  connect(&ActiveObjects::instance(), &ActiveObjects::setImageViewerMode, this,
          &ViewMenuManager::setImageViewerMode);

  Menu->addSeparator();
  // Projection modes
  QActionGroup* projectionGroup = new QActionGroup(this);

  m_perspectiveProjectionAction = Menu->addAction("Perspective Projection");
  m_perspectiveProjectionAction->setCheckable(true);
  m_perspectiveProjectionAction->setActionGroup(projectionGroup);
  m_perspectiveProjectionAction->setChecked(true);
  connect(m_perspectiveProjectionAction, SIGNAL(triggered()),
          SLOT(setProjectionModeToPerspective()));
  m_orthographicProjectionAction = Menu->addAction("Orthographic Projection");
  m_orthographicProjectionAction->setCheckable(true);
  m_orthographicProjectionAction->setActionGroup(projectionGroup);
  m_orthographicProjectionAction->setChecked(false);
  connect(m_orthographicProjectionAction, SIGNAL(triggered()),
          SLOT(setProjectionModeToOrthographic()));

  Menu->addSeparator();

  m_showCenterAxesAction = Menu->addAction("Show Center Axes");
  m_showCenterAxesAction->setCheckable(true);
  m_showCenterAxesAction->setChecked(false);
  connect(m_showCenterAxesAction, &QAction::triggered, this,
          &ViewMenuManager::setShowCenterAxes);
  m_showOrientationAxesAction = Menu->addAction("Show Orientation Axes");
  m_showOrientationAxesAction->setCheckable(true);
  m_showOrientationAxesAction->setChecked(true);
  connect(m_showOrientationAxesAction, &QAction::triggered, this,
          &ViewMenuManager::setShowOrientationAxes);

  Menu->addSeparator();

  m_imageViewerModeAction = Menu->addAction("Image Viewer Mode");
  m_imageViewerModeAction->setCheckable(true);
  m_imageViewerModeAction->setChecked(false);
  connect(m_imageViewerModeAction, &QAction::triggered, this,
          &ViewMenuManager::setImageViewerMode);

  Menu->addSeparator();

  m_showDarkWhiteDataAction = Menu->addAction("Show Dark/White Data");
  m_showDarkWhiteDataAction->setEnabled(false);
  connect(m_showDarkWhiteDataAction, &QAction::triggered, this,
          &ViewMenuManager::showDarkWhiteData);

  Menu->addSeparator();

  m_previousImageViewerSettings.reset(new PreviousImageViewerSettings);

  if (hasLookingGlassPlugin()) {
    setupLookingGlassPlaceholder(mainWindow);
  }
}

ViewMenuManager::~ViewMenuManager()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
  }
}

QString ViewMenuManager::projectionMode() const
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return "";
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();

  return parallel == 0 ? "Perspective" : "Orthographic";
}

void ViewMenuManager::setProjectionMode(QString mode)
{
  if (mode == "Perspective") {
    setProjectionModeToPerspective();
  } else if (mode == "Orthographic") {
    setProjectionModeToOrthographic();
  } else {
    qCritical() << "Invalid projection mode: " << mode;
  }
}

void ViewMenuManager::setProjectionModeToPerspective()
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (parallel) {
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").Set(0);
    m_view->UpdateVTKObjects();
    render();
  }
}

void ViewMenuManager::setProjectionModeToOrthographic()
{
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (!parallel) {
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").Set(1);
    m_view->UpdateVTKObjects();
    render();
  }
}

void ViewMenuManager::onViewPropertyChanged()
{
  if (!m_perspectiveProjectionAction || !m_orthographicProjectionAction) {
    return;
  }
  if (!m_view->GetProperty("CameraParallelProjection")) {
    return;
  }
  int parallel =
    vtkSMPropertyHelper(m_view, "CameraParallelProjection").GetAsInt();
  if (parallel && m_perspectiveProjectionAction->isChecked()) {
    m_orthographicProjectionAction->setChecked(true);
  } else if (!parallel && m_orthographicProjectionAction->isChecked()) {
    m_perspectiveProjectionAction->setChecked(true);
  }
}

void ViewMenuManager::onViewChanged()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
  }

  m_view = ActiveObjects::instance().activeView();

  if (m_view) {
    m_viewObserverId =
      pqCoreUtilities::connect(m_view, vtkCommand::PropertyModifiedEvent, this,
                               SLOT(onViewPropertyChanged()));
  }

  bool enableProjectionModes =
    (m_view && m_view->GetProperty("CameraParallelProjection"));
  m_orthographicProjectionAction->setEnabled(enableProjectionModes);
  m_perspectiveProjectionAction->setEnabled(enableProjectionModes);
  if (enableProjectionModes) {
    vtkSMPropertyHelper parallelProjection(m_view, "CameraParallelProjection");
    m_orthographicProjectionAction->setChecked(parallelProjection.GetAsInt() ==
                                               1);
    m_perspectiveProjectionAction->setChecked(parallelProjection.GetAsInt() !=
                                              1);
  }
  bool enableCenterAxes = m_view && m_view->GetProperty("CenterAxesVisibility");
  bool enableOrientationAxes =
    m_view && m_view->GetProperty("OrientationAxesVisibility");
  m_showCenterAxesAction->setEnabled(enableCenterAxes);
  m_showOrientationAxesAction->setEnabled(enableOrientationAxes);
  if (enableCenterAxes) {
    vtkSMPropertyHelper showCenterAxes(m_view, "CenterAxesVisibility");
    m_showCenterAxesAction->setChecked(showCenterAxes.GetAsInt() == 1);
  }
  if (enableOrientationAxes) {
    vtkSMPropertyHelper showOrientationAxes(m_view,
                                            "OrientationAxesVisibility");
    m_showOrientationAxesAction->setChecked(showOrientationAxes.GetAsInt() ==
                                            1);
  }
}

void ViewMenuManager::setShowCenterAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("CenterAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "CenterAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  render();
}

void ViewMenuManager::setShowOrientationAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("OrientationAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "OrientationAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  render();
}

int ViewMenuManager::interactionMode() const
{
  auto* renderView = ActiveObjects::instance().activePqRenderView();
  return vtkSMPropertyHelper(renderView->getProxy(), "InteractionMode")
    .GetAsInt();
}

void ViewMenuManager::setInteractionMode(int mode)
{
  auto* renderView = ActiveObjects::instance().activePqRenderView();
  vtkSMPropertyHelper(renderView->getProxy(), "InteractionMode").Set(mode);
  renderView->getProxy()->UpdateProperty("InteractionMode", 1);
}

void ViewMenuManager::render()
{
  auto* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

static void resize2DCameraToFit(vtkSMRenderViewProxy* view, double bounds[6],
                                int axis)
{
  double lengths[3] = {
    bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4],
  };

  int* size = view->GetRenderWindow()->GetSize();
  double w = size[0];
  double h = size[1];

  double bw, bh;
  if (axis == 0 || axis == 2) {
    bw = lengths[(axis + 1) % 3];
    bh = lengths[(axis + 2) % 3];
  } else {
    bw = lengths[(axis + 2) % 3];
    bh = lengths[(axis + 1) % 3];
  }
  double viewAspect = w / h;
  double boundsAspect = bw / bh;

  double scale = 0;
  if (viewAspect >= boundsAspect) {
    scale = bh / 2;
  } else {
    scale = bw / 2 / viewAspect;
  }

  auto* camera = view->GetActiveCamera();
  camera->SetParallelScale(scale);
}

void ViewMenuManager::setImageViewerMode(bool enable)
{
  if (m_imageViewerModeAction->isChecked() != enable) {
    QSignalBlocker blocked(m_imageViewerModeAction);
    m_imageViewerModeAction->setChecked(enable);
  }

  if (!enable && enable == m_imageViewerMode) {
    // Just return, nothing to do...
    return;
  }
  m_imageViewerMode = enable;

  if (!enable) {
    emit imageViewerModeToggled(enable);
    // Restore the state to where it was before we began image viewer mode
    restoreImageViewerSettings();
    return;
  }

  auto* ds = ActiveObjects::instance().activeDataSource();
  auto* view =
    vtkSMRenderViewProxy::SafeDownCast(ActiveObjects::instance().activeView());
  auto* camera = view->GetActiveCamera();

  auto& moduleManager = ModuleManager::instance();

  // Save some of the old settings to restore them later
  auto& oldSettings = m_previousImageViewerSettings;
  oldSettings->clear();
  oldSettings->camera->ShallowCopy(camera);
  oldSettings->projection = projectionMode();
  oldSettings->interactionMode = interactionMode();

  // Do the following:
  // 1. Set the projection to orthographic
  // 2. Set the render interactor mode to 2D
  // 3. Find the first slice module, or create one. Hide all other modules.
  // 4. Align the camera so the single slice is filling the screen
  // 5. Show the image viewer slider widget
  setProjectionModeToOrthographic();
  setInteractionMode(vtkPVRenderView::INTERACTION_MODE_2D);

  auto sliceModules = moduleManager.findModules<ModuleSlice*>(ds, view);
  auto* sliceModule = !sliceModules.empty() ? sliceModules[0] : nullptr;

  oldSettings->newSliceModule = !sliceModule;
  if (sliceModule) {
    // Save it's settings before modifying it...
    oldSettings->sliceModuleSettings = sliceModule->serialize();
    // Make sure it is visible
    sliceModule->show();
  } else {
    // If there are no slice modules, create one
    sliceModule = qobject_cast<ModuleSlice*>(
      moduleManager.createAndAddModule("Slice", ds, view));
  }
  oldSettings->sliceModule = sliceModule;

  // Hide all other modules on this data source
  for (auto* module : moduleManager.findModulesGeneric(ds, view)) {
    if (module != sliceModule && module->visibility()) {
      oldSettings->visibleModules.append(module);
      module->hide();
    }
  }

  // Use XY direction, set the index to 0, and hide the arrow
  sliceModule->onDirectionChanged(ModuleSlice::XY);
  sliceModule->onSliceChanged(0);
  sliceModule->setShowArrow(false);

  CameraReaction::resetNegativeZ();

  double bounds[6];
  sliceModule->planeBounds(bounds);
  resize2DCameraToFit(view, bounds, 2);

  emit imageViewerModeToggled(enable);
  emit moduleManager.pipelineViewRenderNeeded();
}

void ViewMenuManager::restoreImageViewerSettings()
{
  auto& settings = m_previousImageViewerSettings;

  auto* view =
    vtkSMRenderViewProxy::SafeDownCast(ActiveObjects::instance().activeView());
  auto* camera = view->GetActiveCamera();
  auto& moduleManager = ModuleManager::instance();

  setInteractionMode(settings->interactionMode);
  setProjectionMode(settings->projection);
  camera->ShallowCopy(settings->camera);

  if (settings->sliceModule) {
    if (settings->newSliceModule) {
      // Remove the newly created slice module
      moduleManager.removeModule(settings->sliceModule);
    } else {
      // Restore the settings on the slice module we grabbed
      settings->sliceModule->deserialize(settings->sliceModuleSettings);
    }
  }

  // Restore visible modules
  for (auto module : settings->visibleModules) {
    if (module) {
      module->show();
    }
  }
  emit moduleManager.pipelineViewRenderNeeded();

  // FIXME: at this point, the center is in a different place, and view
  // is not updated to match the camera position. As a quick fix, just
  // reset the camera. We can improve this in the future if needed.
  view->ResetCamera();
}

void ViewMenuManager::updateDataSource(DataSource* s)
{
  m_dataSource = s;
  updateDataSourceEnableStates();
}

void ViewMenuManager::updateDataSourceEnableStates()
{
  // Currently, both white and dark are required to use this
  // We can change this in the future if needed...
  m_showDarkWhiteDataAction->setEnabled(
    m_dataSource && m_dataSource->darkData() && m_dataSource->whiteData());
}

void ViewMenuManager::showDarkWhiteData()
{
  if (!m_dataSource || !m_dataSource->darkData() ||
      !m_dataSource->whiteData()) {
    return;
  }

  if (!m_sliceViewDialog) {
    m_sliceViewDialog.reset(new SliceViewDialog);
  }

  auto* lut = vtkColorTransferFunction::SafeDownCast(
    m_dataSource->colorMap()->GetClientSideObject());

  m_sliceViewDialog->setLookupTable(lut);
  m_sliceViewDialog->setDarkImage(m_dataSource->darkData());
  m_sliceViewDialog->setWhiteImage(m_dataSource->whiteData());
  m_sliceViewDialog->switchToDark();

  m_sliceViewDialog->exec();
}

void ViewMenuManager::setupLookingGlassPlaceholder(QMainWindow* mainWindow)
{
  // Add the Looking Glass menu item placeholder, which, when checked,
  // will cause the plugin to load, and the placeholder will remove itself.
  // This needs to be done so that the EULA will only pop up if the user
  // tries to use the looking glass plugin.
  // We're gonna take advantage of the fact that
  // pqViewMenuManager::updateMenu() automatically adds entries for dock
  // widgets in order. We will make a fake dock widget, which will get added,
  // then when that action is triggered, we will load the plugin and replace
  // the fake action with the real one.

  // Create the fake dock widget
  QPointer<QDockWidget> lgPlaceholderWidget = new QDockWidget(mainWindow);
  lgPlaceholderWidget->setVisible(false);

  // Get the action
  auto* lgPlaceholderAction = lgPlaceholderWidget->toggleViewAction();
  lgPlaceholderAction->setText("Looking Glass");

  // This will place the dock widget action in the menu
  this->updateMenu();

  // If the action is triggered, load the plugin and remove the placeholder
  connect(lgPlaceholderAction, &QAction::triggered, lgPlaceholderWidget,
          [lgPlaceholderWidget]() {
            loadLookingGlassPlugin();
            lgPlaceholderWidget->deleteLater();
          });
}

} // namespace tomviz
