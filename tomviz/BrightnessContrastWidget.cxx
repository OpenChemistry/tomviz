/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "BrightnessContrastWidget.h"
#include "ui_BrightnessContrastWidget.h"

#include <cmath>
#include <vector>

#include <QPointer>

#include <vtkCallbackCommand.h>
#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkSmartPointer.h>

#include "DataSource.h"
#include "Utilities.h"

namespace tomviz {

class BrightnessContrastWidget::Internals
{
public:
  Internals(DataSource* ds, vtkDiscretizableColorTransferFunction* lut)
    : m_ds(ds), m_lut(lut)
  {
    m_opacity = m_lut->GetScalarOpacityFunction();
    resetOriginalData();
    resetUncroppedData();
    dataModifiedCallbackCommand->SetCallback(dataModifiedCallback);
    dataModifiedCallbackCommand->SetClientData(this);
    connectDataModifiedCallback();
  }

  ~Internals() { disconnectDataModifiedCallback(); }

  QPointer<DataSource> m_ds;
  vtkSmartPointer<vtkDiscretizableColorTransferFunction> m_lut;
  vtkSmartPointer<vtkPiecewiseFunction> m_opacity;
  vtkNew<vtkDiscretizableColorTransferFunction> m_uncroppedLut;
  vtkNew<vtkDiscretizableColorTransferFunction> m_originalLut;
  vtkNew<vtkPiecewiseFunction> m_uncroppedOpacity;
  vtkNew<vtkPiecewiseFunction> m_originalOpacity;
  Ui::BrightnessContrastWidget ui;
  vtkNew<vtkCallbackCommand> dataModifiedCallbackCommand;
  bool m_pushingChanges = false;

  void connectDataModifiedCallback()
  {
    m_lut->AddObserver(vtkCommand::ModifiedEvent, dataModifiedCallbackCommand);
    m_opacity->AddObserver(vtkCommand::ModifiedEvent,
                           dataModifiedCallbackCommand);
  }

  void disconnectDataModifiedCallback()
  {
    m_lut->RemoveObserver(dataModifiedCallbackCommand);
    m_opacity->RemoveObserver(dataModifiedCallbackCommand);
  }

  void setupGui()
  {
    auto blockers = blockSignals();

    // Offset the numbers, because otherwise we can get NaN values.
    double offset = 1;

    ui.contrast->setMinimum(offset);
    ui.contrast->setMaximum(100 - offset);
    ui.contrast->setValue(50);

    ui.brightness->setMinimum(offset);
    ui.brightness->setMaximum(100 - offset);
    ui.brightness->setValue(50);

    updateRanges();
  }

  void updateRanges()
  {
    if (!m_ds) {
      return;
    }

    auto blockers = blockSignals();

    double range[2];
    m_ds->getRange(range);

    // Offset some of the extrema to avoid NaN values
    double offset = (range[1] - range[0]) / 1000;
    ui.minimum->setMinimum(range[0]);
    ui.minimum->setMaximum(range[1] - offset);
    ui.maximum->setMinimum(range[0] + offset);
    ui.maximum->setMaximum(range[1]);
  }

  void setupConnections()
  {
    QObject::connect(ui.minimum, &DoubleSliderWidget::valueEdited,
                     [this](double v) { setMinimum(v); });
    QObject::connect(ui.maximum, &DoubleSliderWidget::valueEdited,
                     [this](double v) { setMaximum(v); });
    QObject::connect(ui.brightness, &DoubleSliderWidget::valueEdited,
                     [this](double v) { setBrightness(v); });
    QObject::connect(ui.contrast, &DoubleSliderWidget::valueEdited,
                     [this](double v) { setContrast(v); });
  }

  void setDataSource(DataSource* ds)
  {
    if (m_ds == ds) {
      return;
    }

    m_ds = ds;
    updateRanges();
    updateGui();
  }

  void setLut(vtkDiscretizableColorTransferFunction* lut)
  {
    if (m_lut == lut) {
      return;
    }

    disconnectDataModifiedCallback();

    m_lut = lut;
    m_opacity = m_lut->GetScalarOpacityFunction();

    connectDataModifiedCallback();

    resetUncroppedData();
    resetOriginalData();
    updateGui();
  }

  void resetUncroppedData()
  {
    m_uncroppedLut->DeepCopy(m_lut);
    m_uncroppedOpacity->DeepCopy(m_opacity);

    // Uncropped data has placeholder nodes removed...
    removePlaceholderNodes(m_uncroppedLut);
    removePlaceholderNodes(m_uncroppedOpacity);
  }

  void resetOriginalData()
  {
    m_originalLut->DeepCopy(m_lut);
    m_originalOpacity->DeepCopy(m_opacity);
  }

  void reset()
  {
    m_pushingChanges = true;
    m_lut->DeepCopy(m_originalLut);
    m_opacity->DeepCopy(m_originalOpacity);
    m_pushingChanges = false;
    resetUncroppedData();
    updateGui();
  }

  void updateGui()
  {
    if (!m_ds) {
      return;
    }

    auto blockers = blockSignals();
    ui.minimum->setValue(computeMinimum());
    ui.maximum->setValue(computeMaximum());
    ui.brightness->setValue(computeBrightness());
    ui.contrast->setValue(computeContrast());
  }

  double computeMinimum()
  {
    // The first node is the minimum
    auto* lut = m_uncroppedLut.Get();
    if (lut->GetSize() < 1) {
      return DBL_MAX;
    }

    double node[6];
    lut->GetNodeValue(0, node);
    return node[0];
  }

  double computeMaximum()
  {
    // The last node is the maximum
    auto* lut = m_uncroppedLut.Get();
    if (lut->GetSize() < 1) {
      return -DBL_MAX;
    }

    double node[6];
    lut->GetNodeValue(lut->GetSize() - 1, node);
    return node[0];
  }

  double computeBrightness()
  {
    // Brightness is the offset of the average between the
    // minimum and the maximum, scaled to the 0-100 range.
    double min = computeMinimum();
    double max = computeMaximum();
    double mean = (max + min) / 2;

    if (!m_ds || min == DBL_MAX || max == -DBL_MAX) {
      return -1;
    }

    double range[2];
    m_ds->getRange(range);
    return rescale(mean, range[0], range[1], 100, 0);
  }

  double computeContrast()
  {
    // Contrast is how wide the color range is compared to the data.
    double min = computeMinimum();
    double max = computeMaximum();

    if (!m_ds || min == DBL_MAX || max == -DBL_MAX) {
      return -1;
    }

    double range[2];
    m_ds->getRange(range);

    double width = max - min;
    double dataWidth = range[1] - range[0];

    double angle = atan((width - dataWidth) / dataWidth);
    return rescale(angle, -M_PI / 4, M_PI / 4, 100, 0);
  }

  void setMinimum(double v)
  {
    double oldMax = computeMaximum();

    double newMin = v;
    double newMax = oldMax > newMin ? oldMax : newMin + 1;

    rescaleNodes(newMin, newMax);
  }

  void setMaximum(double v)
  {
    double oldMin = computeMinimum();

    double newMax = v;
    double newMin = oldMin < newMax ? oldMin : newMax - 1;

    rescaleNodes(newMin, newMax);
  }

  void setContrast(double v)
  {
    if (!m_ds) {
      return;
    }

    double range[2];
    m_ds->getRange(range);

    double dataWidth = range[1] - range[0];

    double previousContrast = computeContrast();
    double previousAngle =
      rescale(previousContrast, 100, 0, -M_PI / 4, M_PI / 4);
    double previousWidth = tan(previousAngle) * dataWidth + dataWidth;

    double angle = rescale(v, 100, 0, -M_PI / 4, M_PI / 4);
    double width = tan(angle) * dataWidth + dataWidth;

    double offset = (width - previousWidth) / 2.0;

    double oldMin = computeMinimum();
    double oldMax = computeMaximum();

    rescaleNodes(oldMin - offset, oldMax + offset);
  }

  void setBrightness(double v)
  {
    if (!m_ds) {
      return;
    }

    double range[2];
    m_ds->getRange(range);

    double previousBrightness = computeBrightness();
    double previousMean =
      rescale(previousBrightness, 100, 0, range[0], range[1]);

    double newMean = rescale(v, 100, 0, range[0], range[1]);

    double offset = newMean - previousMean;

    auto oldMin = computeMinimum();
    auto oldMax = computeMaximum();

    rescaleNodes(oldMin + offset, oldMax + offset);
  }

  void rescaleNodes(double newMin, double newMax)
  {
    tomviz::rescaleNodes(m_uncroppedLut, newMin, newMax);
    tomviz::rescaleNodes(m_uncroppedOpacity, newMin, newMax);
    pushChanges();
    updateGui();
  }

  void pushChanges()
  {
    m_pushingChanges = true;
    if (!m_ds) {
      m_pushingChanges = false;
      return;
    }

    m_lut->DeepCopy(m_uncroppedLut);
    addPlaceholderNodes(m_lut, m_ds);
    removePointsOutOfRange(m_lut, m_ds);

    m_opacity->DeepCopy(m_uncroppedOpacity);
    addPlaceholderNodes(m_opacity, m_ds);
    removePointsOutOfRange(m_opacity, m_ds);

    m_pushingChanges = false;
  }

  std::vector<QSignalBlocker> blockSignals()
  {
    QList<QWidget*> blockList = {
      ui.minimum, ui.maximum, ui.brightness, ui.contrast,
    };

    std::vector<QSignalBlocker> blockers;
    for (auto* w : blockList) {
      blockers.emplace_back(w);
    }

    return blockers;
  }

  void onDataModified()
  {
    // This should only get called when it gets modified from outside
    // of this class. We will reset everything if that's the case.
    if (m_pushingChanges) {
      return;
    }

    resetUncroppedData();
    resetOriginalData();
    updateGui();
  }

  static void dataModifiedCallback(vtkObject*, unsigned long, void* clientData,
                                   void*)
  {
    auto* obj =
      reinterpret_cast<BrightnessContrastWidget::Internals*>(clientData);
    obj->onDataModified();
  }
};

BrightnessContrastWidget::BrightnessContrastWidget(
  DataSource* ds, vtkDiscretizableColorTransferFunction* lut, QWidget* p)
  : Superclass(p), m_internals(new BrightnessContrastWidget::Internals(ds, lut))
{
  auto& ui = m_internals->ui;
  ui.setupUi(this);
  m_internals->setupGui();
  m_internals->setupConnections();

  connect(ui.autoButton, &QPushButton::pressed, this, [this]() {
    m_internals->reset();
    emit autoPressed();
  });

  connect(ui.resetButton, &QPushButton::pressed, this, [this]() {
    m_internals->reset();
    emit resetPressed();
  });

  updateGui();
}

BrightnessContrastWidget::~BrightnessContrastWidget() = default;

void BrightnessContrastWidget::setDataSource(DataSource* ds)
{
  m_internals->setDataSource(ds);
}

void BrightnessContrastWidget::setLut(
  vtkDiscretizableColorTransferFunction* lut)
{
  m_internals->setLut(lut);
}

void BrightnessContrastWidget::updateGui()
{
  m_internals->updateGui();
}

} // namespace tomviz
