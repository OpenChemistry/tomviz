/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OpacityPresetWidget.h"

#include "DoubleSliderWidget.h"
#include "Utilities.h"
#include "pipeline/data/VolumeData.h"

#include <vtkDiscretizableColorTransferFunction.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkWeakPointer.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tomviz {

class OpacityPresetWidget::Internals
{
public:
  enum Preset
  {
    Gaussian = 0,
    Linear,
    LinearWithCutoff
  };

  pipeline::VolumeDataPtr volumeData;
  vtkWeakPointer<vtkDiscretizableColorTransferFunction> lut;
  // The curve and color range as they were when the dialog opened
  vtkNew<vtkPiecewiseFunction> originalOpacity;
  double originalColorRange[2] = { 0.0, 1.0 };
  double range[2] = { 0.0, 1.0 };

  QComboBox* preset = nullptr;
  QLabel* centerLabel = nullptr;
  QLabel* widthLabel = nullptr;
  DoubleSliderWidget* center = nullptr;
  DoubleSliderWidget* width = nullptr;
  QCheckBox* colorFollows = nullptr;

  vtkPiecewiseFunction* opacity() const
  {
    return lut ? lut->GetScalarOpacityFunction() : nullptr;
  }

  void readRange()
  {
    if (volumeData) {
      auto r = volumeData->scalarRange();
      range[0] = r[0];
      range[1] = r[1];
    }
    if (!(range[1] > range[0])) {
      range[1] = range[0] + 1.0;
    }
  }

  void snapshot()
  {
    if (auto* pwf = opacity()) {
      originalOpacity->DeepCopy(pwf);
    }
    if (lut) {
      lut->GetRange(originalColorRange);
    }
  }

  // Opacity of the current preset at x
  double evaluate(double x) const
  {
    double c = center->value();
    double w = std::max(width->value(), 1e-9 * (range[1] - range[0]));
    switch (preset->currentIndex()) {
      case Gaussian: {
        double t = (x - c) / w;
        return std::exp(-0.5 * t * t);
      }
      case Linear:
        return std::clamp((x - (c - 0.5 * w)) / w, 0.0, 1.0);
      default: // LinearWithCutoff: transparent below the cutoff
        return x < c ? 0.0 : std::clamp((x - c) / w, 0.0, 1.0);
    }
  }

  // Sample points where the shape bends, plus the range ends so the
  // editor keeps spanning the whole data range.
  std::vector<double> samplePoints() const
  {
    double c = center->value();
    double w = std::max(width->value(), 1e-9 * (range[1] - range[0]));
    std::vector<double> xs{ range[0], range[1] };
    switch (preset->currentIndex()) {
      case Gaussian:
        for (int k = -6; k <= 6; ++k) {
          xs.push_back(c + 0.5 * k * w);
        }
        break;
      case Linear:
        xs.push_back(c - 0.5 * w);
        xs.push_back(c + 0.5 * w);
        break;
      default:
        xs.push_back(c);
        xs.push_back(c + w);
        break;
    }
    std::vector<double> inRange;
    for (double x : xs) {
      if (x >= range[0] && x <= range[1]) {
        inRange.push_back(x);
      }
    }
    std::sort(inRange.begin(), inRange.end());
    inRange.erase(std::unique(inRange.begin(), inRange.end()),
                  inRange.end());
    return inRange;
  }

  void apply()
  {
    auto* pwf = opacity();
    if (!pwf) {
      return;
    }
    pwf->RemoveAllPoints();
    for (double x : samplePoints()) {
      pwf->AddPoint(x, evaluate(x));
    }
    // One Modified() is enough: the histogram editor listens for it,
    // re-renders, and mirrors the points onto the proxy.
    pwf->Modified();

    if (colorFollows->isChecked() && lut &&
        preset->currentIndex() == Gaussian) {
      // The color map's upper end tracks the tail of the Gaussian, so
      // the brightest color lands where the opacity fades out.
      double tail = std::min(center->value() + 3.0 * width->value(),
                             range[1]);
      double lo = std::min(originalColorRange[0], tail);
      if (tail > lo) {
        removePlaceholderNodes(lut);
        rescaleNodes(lut.Get(), lo, tail);
        addPlaceholderNodes(lut, range);
        lut->Modified();
      }
    }
  }

  void restore()
  {
    if (auto* pwf = opacity()) {
      pwf->DeepCopy(originalOpacity);
      pwf->Modified();
    }
    if (lut) {
      removePlaceholderNodes(lut);
      rescaleNodes(lut.Get(), originalColorRange[0], originalColorRange[1]);
      addPlaceholderNodes(lut, range);
      lut->Modified();
    }
  }

  void relabel()
  {
    switch (preset->currentIndex()) {
      case Gaussian:
        centerLabel->setText("Center");
        widthLabel->setText("Width (sigma)");
        break;
      case Linear:
        centerLabel->setText("Ramp Center");
        widthLabel->setText("Ramp Width");
        break;
      default:
        centerLabel->setText("Cutoff");
        widthLabel->setText("Ramp Width");
        break;
    }
    colorFollows->setEnabled(preset->currentIndex() == Gaussian);
  }

  void configureSliders()
  {
    double span = range[1] - range[0];
    for (auto* slider : { center, width }) {
      slider->setMinimum(range[0]);
      slider->setMaximum(range[1]);
    }
    width->setMinimum(span / 1000.0);
    width->setMaximum(span);
  }

  void setDefaults()
  {
    double span = range[1] - range[0];
    double mid = 0.5 * (range[0] + range[1]);
    switch (preset->currentIndex()) {
      case Gaussian:
        center->setValue(mid);
        width->setValue(span / 6.0);
        break;
      case Linear:
        center->setValue(mid);
        width->setValue(span);
        break;
      default:
        center->setValue(range[0] + 0.2 * span);
        width->setValue(0.8 * span);
        break;
    }
  }
};

OpacityPresetWidget::OpacityPresetWidget(
  pipeline::VolumeDataPtr volumeData, vtkDiscretizableColorTransferFunction* lut,
  QWidget* parent)
  : QWidget(parent), m_internals(new Internals)
{
  auto& in = *m_internals;
  in.volumeData = volumeData;
  in.lut = lut;
  in.readRange();
  in.snapshot();

  auto* layout = new QFormLayout(this);
  in.preset = new QComboBox(this);
  in.preset->addItems({ "Gaussian", "Linear", "Linear with cutoff" });
  layout->addRow("Shape", in.preset);

  auto makeSlider = [this]() {
    auto* slider = new DoubleSliderWidget(true, this);
    slider->setLineEditWidth(70);
    slider->setResolution(1000);
    return slider;
  };
  in.center = makeSlider();
  in.width = makeSlider();
  in.centerLabel = new QLabel(this);
  in.widthLabel = new QLabel(this);
  layout->addRow(in.centerLabel, in.center);
  layout->addRow(in.widthLabel, in.width);

  in.colorFollows = new QCheckBox("Color map end follows the Gaussian", this);
  in.colorFollows->setToolTip(
    "Move the color map's upper end to three widths past the center, so "
    "the brightest color sits where the opacity fades out.");
  layout->addRow(in.colorFollows);

  auto* buttons = new QHBoxLayout;
  auto* reset = new QPushButton("Reset", this);
  reset->setToolTip("Put back the opacity curve from when this opened.");
  buttons->addStretch();
  buttons->addWidget(reset);
  layout->addRow(buttons);

  in.configureSliders();
  in.relabel();
  in.setDefaults();

  auto applyNow = [this]() { m_internals->apply(); };
  connect(in.preset, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, applyNow]() {
            auto& internals = *m_internals;
            internals.relabel();
            {
              QSignalBlocker b1(internals.center), b2(internals.width);
              internals.setDefaults();
            }
            applyNow();
          });
  for (auto* slider : { in.center, in.width }) {
    connect(slider, &DoubleSliderWidget::valueChanged, this, applyNow);
    connect(slider, &DoubleSliderWidget::valueEdited, this, applyNow);
  }
  connect(in.colorFollows, &QCheckBox::toggled, this, [this, applyNow](bool on) {
    if (on) {
      applyNow();
    } else {
      m_internals->restore();
      applyNow();
    }
  });
  connect(reset, &QPushButton::clicked, this,
          [this]() { m_internals->restore(); });

  // Opening the dialog does not change anything until a control is used
}

OpacityPresetWidget::~OpacityPresetWidget() = default;

void OpacityPresetWidget::setVolumeData(pipeline::VolumeDataPtr volumeData)
{
  m_internals->volumeData = volumeData;
}

void OpacityPresetWidget::setLut(vtkDiscretizableColorTransferFunction* lut)
{
  m_internals->lut = lut;
}

void OpacityPresetWidget::updateGui()
{
  // A new dataset or color map: re-baseline against it
  auto& in = *m_internals;
  in.readRange();
  in.snapshot();
  // Only a control touched by the user may change the curve
  QSignalBlocker b1(in.center), b2(in.width);
  in.configureSliders();
  in.setDefaults();
}

} // namespace tomviz
