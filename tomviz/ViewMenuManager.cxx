/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ViewMenuManager.h"

#include <pqCoreUtilities.h>
#include <pqView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkGridAxesActor3D.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>

#include <QAction>
#include <QActionGroup>
#include <QDebug>
#include <QDialog>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "Utilities.h"

namespace tomviz {

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
  connect(&ActiveObjects::instance(),
          static_cast<void (ActiveObjects::*)(vtkSMViewProxy*)>(
            &ActiveObjects::viewChanged),
          this, &ViewMenuManager::onViewChanged);

  Menu->addSeparator();
  // Projection modes
  QActionGroup* projectionGroup = new QActionGroup(this);

  m_perspectiveProjectionAction = Menu->addAction("Perspective Projection");
  m_perspectiveProjectionAction->setCheckable(true);
  m_perspectiveProjectionAction->setActionGroup(projectionGroup);
  m_perspectiveProjectionAction->setChecked(true);
  connect(m_perspectiveProjectionAction, &QAction::triggered, this,
          &ViewMenuManager::setProjectionModeToPerspective);
  m_orthographicProjectionAction = Menu->addAction("Orthographic Projection");
  m_orthographicProjectionAction->setCheckable(true);
  m_orthographicProjectionAction->setActionGroup(projectionGroup);
  m_orthographicProjectionAction->setChecked(false);
  connect(m_orthographicProjectionAction, &QAction::triggered, this,
          &ViewMenuManager::setProjectionModeToOrthographic);

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

void ViewMenuManager::render()
{
  auto* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
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
