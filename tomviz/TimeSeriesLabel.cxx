/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TimeSeriesLabel.h"

#include "ActiveObjects.h"
#include "TimeSeriesStep.h"
#include "Utilities.h"

#include <pqView.h>
#include <vtkSMViewProxy.h>

#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTextRepresentation.h>
#include <vtkTextWidget.h>

#include <QPointer>

namespace tomviz {

class TimeSeriesLabel::Internal : public QObject
{
public:
  vtkNew<vtkNamedColors> colors;
  vtkNew<vtkTextActor> textActor;
  vtkNew<vtkTextRepresentation> textRepresentation;
  vtkNew<vtkTextWidget> textWidget;

  QPointer<DataSource> activeDataSource;
  QPointer<pqView> activeView;

  Internal(QObject* p) : QObject(p)
  {
    textWidget->SetRepresentation(textRepresentation);
    textWidget->SetTextActor(textActor);
    textWidget->SelectableOff();

    resetColor();
    resetPosition();

    setupConnections();
  }

  void setupConnections()
  {
    connect(&activeObjects(),
            QOverload<vtkSMViewProxy*>::of(&ActiveObjects::viewChanged), this,
            &Internal::viewChanged);
    connect(&activeObjects(), &ActiveObjects::dataSourceActivated, this,
            &Internal::dataSourceActivated);
    connect(&activeObjects(), &ActiveObjects::showTimeSeriesLabelChanged, this,
            &Internal::updateVisibility);
  }

  void viewChanged(vtkSMViewProxy* view)
  {
    auto* pqview = tomviz::convert<pqView*>(view);
    if (activeView == pqview) {
      return;
    }

    if (view && view->GetRenderWindow()) {
      textWidget->SetInteractor(view->GetRenderWindow()->GetInteractor());
    } else {
      textWidget->SetInteractor(nullptr);
    }

    // This will render the old view if the visibility has changed
    updateVisibility();

    activeView = pqview;
    // Now render the new view
    render();
  }

  void dataSourceActivated(DataSource* ds)
  {
    if (ds == activeDataSource) {
      return;
    }

    if (activeDataSource) {
      disconnect(activeDataSource);
    }

    if (ds) {
      connect(ds, &DataSource::timeStepsModified, this,
              &Internal::timeStepsModified);
      connect(ds, &DataSource::timeStepChanged, this,
              &Internal::timeStepChanged);
    }

    activeDataSource = ds;
    updateVisibility();
    timeStepChanged();
  }

  void timeStepsModified()
  {
    // In case there wasn't a time series before...
    updateVisibility();
    timeStepChanged();
  }

  void timeStepChanged()
  {
    if (!activeDataSource || !activeDataSource->hasTimeSteps()) {
      return;
    }

    auto label = activeDataSource->currentTimeSeriesStep().label;
    if (label == textActor->GetInput()) {
      // No changes needed
      return;
    }

    textActor->SetInput(label.toLocal8Bit().data());
    render();
  }

  void updateVisibility()
  {
    // If we have an interactor and the settings indicate it should
    bool show = activeObjects().showTimeSeriesLabel();
    bool hasInteractor = textWidget->GetInteractor() != nullptr;
    bool hasTimeSteps = activeDataSource && activeDataSource->hasTimeSteps();

    bool visible = show && hasInteractor && hasTimeSteps;

    if (visible != textWidget->GetEnabled()) {
      textWidget->SetEnabled(visible);
      render();
    }
  }

  void resetColor()
  {
    auto* property = textActor->GetTextProperty();
    property->SetColor(colors->GetColor3d("White").GetData());
  }

  void resetPosition()
  {
    textRepresentation->GetPositionCoordinate()->SetValue(0.7, 0.9);
    textRepresentation->GetPosition2Coordinate()->SetValue(0.29, 0.09);
  }

  void render()
  {
    if (!activeView) {
      return;
    }

    activeView->render();
  }

  ActiveObjects& activeObjects() { return ActiveObjects::instance(); }
};

TimeSeriesLabel::TimeSeriesLabel(QObject* p) : QObject(p)
{
  m_internal.reset(new Internal(p));
}

TimeSeriesLabel::~TimeSeriesLabel() = default;

} // namespace tomviz
