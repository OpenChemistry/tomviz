/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ViewMenuManager.h"

#include <pqCoreUtilities.h>
#include <pqView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <vtkCommand.h>
#include <vtkGridAxes3DActor.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkTextProperty.h>

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>

#include "ActiveObjects.h"
#include "DataSource.h"
#include "QVTKGLWidget.h"
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
  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          SLOT(onViewChanged()));

  connect(&ActiveObjects::instance(), &ActiveObjects::dataSourceActivated, this,
          &ViewMenuManager::updateDataSource);
  connect(&ActiveObjects::instance(),
          &ActiveObjects::transformedDataSourceActivated, this,
          &ViewMenuManager::updateDataSource);

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

  m_showDarkDataAction = Menu->addAction("Show Dark Data");
  m_showDarkDataAction->setEnabled(false);
  connect(m_showDarkDataAction, &QAction::triggered, this,
          &ViewMenuManager::showDarkData);
  m_showWhiteDataAction = Menu->addAction("Show White Data");
  m_showWhiteDataAction->setEnabled(false);
  connect(m_showWhiteDataAction, &QAction::triggered, this,
          &ViewMenuManager::showWhiteData);

  Menu->addSeparator();
}

ViewMenuManager::~ViewMenuManager()
{
  if (m_view) {
    m_view->RemoveObserver(m_viewObserverId);
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
    pqView* view = tomviz::convert<pqView*>(m_view);
    if (view) {
      view->render();
    }
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
    pqView* view = tomviz::convert<pqView*>(m_view);
    if (view) {
      view->render();
    }
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
  pqView* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

void ViewMenuManager::setShowOrientationAxes(bool show)
{
  if (!m_view || !m_view->GetProperty("OrientationAxesVisibility")) {
    return;
  }
  vtkSMPropertyHelper visibility(m_view, "OrientationAxesVisibility");
  visibility.Set(show ? 1 : 0);
  m_view->UpdateVTKObjects();
  pqView* view = tomviz::convert<pqView*>(m_view);
  if (view) {
    view->render();
  }
}

void ViewMenuManager::updateDataSource(DataSource* s)
{
  m_dataSource = s;
  updateDataSourceEnableStates();
}

void ViewMenuManager::updateDataSourceEnableStates()
{
  m_showDarkDataAction->setEnabled(m_dataSource && m_dataSource->darkData());
  m_showWhiteDataAction->setEnabled(m_dataSource && m_dataSource->whiteData());
}

void ViewMenuManager::showDarkData()
{
  if (!m_dataSource || !m_dataSource->darkData()) {
    return;
  }

  // Just for simpler reference
  const auto& renderer = m_darkRenderer;
  const auto& slice = m_darkImageSlice;
  const auto& mapper = m_darkImageSliceMapper;

  if (!m_darkDataDialog) {
    m_darkDataDialog.reset(new QDialog);

    // Pick a reasonable size. It is very tiny otherwise.
    m_darkDataDialog->resize(500, 500);
    m_darkDataDialog->setWindowTitle("Dark Data");

    auto* layout = new QHBoxLayout(m_darkDataDialog.data());
    m_darkDataDialog->setLayout(layout);
    auto* widget = new QVTKGLWidget(m_darkDataDialog.data());
    layout->addWidget(widget);

    slice->SetMapper(mapper);
    renderer->AddViewProp(slice);

    widget->renderWindow()->AddRenderer(renderer);

    vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle;
    interatorStyle->SetRenderOnMouseMove(true);
    widget->interactor()->SetInteractorStyle(interatorStyle);
  }

  // Update the input
  mapper->SetInputData(m_dataSource->darkData());
  mapper->Update();

  // Update the color map
  vtkScalarsToColors* lut = vtkScalarsToColors::SafeDownCast(
    m_dataSource->colorMap()->GetClientSideObject());
  if (lut) {
    slice->GetProperty()->SetLookupTable(lut);
  }

  // Re-render
  auto* widget = qobject_cast<QVTKGLWidget*>(
    m_darkDataDialog->layout()->itemAt(0)->widget());

  widget->renderWindow()->Render();

  tomviz::setupRenderer(renderer, mapper);

  m_darkDataDialog->exec();
}

void ViewMenuManager::showWhiteData()
{
  if (!m_dataSource || !m_dataSource->whiteData()) {
    return;
  }

  // Just for simpler reference
  const auto& renderer = m_whiteRenderer;
  const auto& slice = m_whiteImageSlice;
  const auto& mapper = m_whiteImageSliceMapper;

  if (!m_whiteDataDialog) {
    m_whiteDataDialog.reset(new QDialog);

    // Pick a reasonable size. It is very tiny otherwise.
    m_whiteDataDialog->resize(500, 500);
    m_whiteDataDialog->setWindowTitle("White Data");

    auto* layout = new QHBoxLayout(m_whiteDataDialog.data());
    m_whiteDataDialog->setLayout(layout);
    auto* widget = new QVTKGLWidget(m_whiteDataDialog.data());
    layout->addWidget(widget);

    slice->SetMapper(mapper);
    renderer->AddViewProp(slice);

    widget->renderWindow()->AddRenderer(renderer);

    vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle;
    interatorStyle->SetRenderOnMouseMove(true);
    widget->interactor()->SetInteractorStyle(interatorStyle);
  }

  // Update the input
  mapper->SetInputData(m_dataSource->whiteData());
  mapper->Update();

  // Update the color map
  vtkScalarsToColors* lut = vtkScalarsToColors::SafeDownCast(
    m_dataSource->colorMap()->GetClientSideObject());
  if (lut) {
    slice->GetProperty()->SetLookupTable(lut);
  }

  // Re-render
  auto* widget = qobject_cast<QVTKGLWidget*>(
    m_whiteDataDialog->layout()->itemAt(0)->widget());

  widget->renderWindow()->Render();

  tomviz::setupRenderer(renderer, mapper);

  m_whiteDataDialog->exec();
}

} // namespace tomviz
