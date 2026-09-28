/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TimeSeriesLabel.h"

#include "ActiveObjects.h"
#include "Utilities.h"

#include "pipeline/OutputPort.h"
#include "pipeline/PortData.h"
#include "pipeline/data/VolumeData.h"

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

  QPointer<pqView> activeView;
  QPointer<pipeline::OutputPort> watchedPort;

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
    connect(&activeObjects(), &ActiveObjects::activeTipOutputPortChanged, this,
            &Internal::activeDataChanged);
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

  // The VolumeData on the active tip output port, or null.
  pipeline::VolumeDataPtr activeVolumeData()
  {
    auto* port = activeObjects().activeTipOutputPort();
    if (!port || !port->hasData()) {
      return nullptr;
    }
    try {
      return port->data().value<pipeline::VolumeDataPtr>();
    } catch (const std::bad_any_cast&) {
      return nullptr;
    }
  }

  void activeDataChanged()
  {
    // Follow the active port so in-place data mutations (time series
    // playback switching steps) refresh the label text too.
    auto* port = activeObjects().activeTipOutputPort();
    if (port != watchedPort) {
      if (watchedPort) {
        disconnect(watchedPort, &pipeline::OutputPort::intermediateDataApplied,
                   this, nullptr);
      }
      watchedPort = port;
      if (port) {
        connect(port, &pipeline::OutputPort::intermediateDataApplied, this,
                &Internal::updateLabel);
      }
    }

    updateVisibility();
    updateLabel();
  }

  void updateLabel()
  {
    auto vol = activeVolumeData();
    if (!vol || !vol->hasTimeSteps()) {
      return;
    }

    auto steps = vol->timeSteps();
    int index = vol->currentTimeStepIndex();
    if (index < 0 || index >= static_cast<int>(steps.size())) {
      return;
    }

    auto label = steps[index].label;
    if (label == textActor->GetInput()) {
      // No changes needed
      return;
    }

    textActor->SetInput(label.toLocal8Bit().data());
    render();
  }

  void updateVisibility()
  {
    // Show if the setting is enabled, we have an interactor, and the active
    // data is a time series.
    bool show = activeObjects().showTimeSeriesLabel();
    bool hasInteractor = textWidget->GetInteractor() != nullptr;
    auto vol = activeVolumeData();
    bool hasTimeSteps = vol && vol->hasTimeSteps();

    bool visible = show && hasInteractor && hasTimeSteps;

    if (visible != static_cast<bool>(textWidget->GetEnabled())) {
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
