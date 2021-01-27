/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ColorMapSettingsWidget.h"
#include "ui_ColorMapSettingsWidget.h"

#include <vector>

#include <vtkColorTransferFunction.h>
#include <vtkWeakPointer.h>

namespace tomviz {

static const QList<QPair<QString, int>> COLOR_SPACE_OPTIONS = {
  { "RGB", VTK_CTF_RGB },
  { "HSV", VTK_CTF_HSV },
  { "Lab", VTK_CTF_LAB },
  { "Diverging", VTK_CTF_DIVERGING },
  { "Lab/CIEDE2000", VTK_CTF_LAB_CIEDE2000 },
  { "Step", VTK_CTF_STEP },
};

static QStringList colorSpaceKeys()
{
  QStringList ret;
  for (const auto& element : COLOR_SPACE_OPTIONS) {
    ret.append(element.first);
  }
  return ret;
}

static QList<int> colorSpaceValues()
{
  QList<int> ret;
  for (const auto& element : COLOR_SPACE_OPTIONS) {
    ret.append(element.second);
  }
  return ret;
}

class ColorMapSettingsWidget::Internals
{
public:
  Internals(vtkColorTransferFunction* lut) : m_lut(lut) {}

  vtkWeakPointer<vtkColorTransferFunction> m_lut;
  Ui::ColorMapSettingsWidget ui;

  void setupConnections()
  {
    QObject::connect(ui.colorSpace,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [this]() { colorSpaceChanged(); });
  }

  void setLut(vtkColorTransferFunction* lut)
  {
    if (m_lut == lut) {
      return;
    }

    m_lut = lut;
    updateGui();
  }

  void updateGui()
  {
    if (!m_lut) {
      return;
    }

    auto blockers = blockSignals();
    updateColorSpaceUi();
  }

  void updateColorSpaceUi()
  {
    if (!m_lut) {
      return;
    }

    ui.colorSpace->clear();
    ui.colorSpace->addItems(colorSpaceKeys());

    int colorSpace = m_lut->GetColorSpace();
    const auto& values = colorSpaceValues();
    for (int i = 0; i < values.size(); ++i) {
      if (values[i] == colorSpace) {
        ui.colorSpace->setCurrentIndex(i);
        break;
      }
    }
  }

  std::vector<QSignalBlocker> blockSignals()
  {
    QList<QWidget*> blockList = {
      ui.colorSpace,
    };

    std::vector<QSignalBlocker> blockers;
    for (auto* w : blockList) {
      blockers.emplace_back(w);
    }

    return blockers;
  }

  int selectedColorSpace()
  {
    auto text = ui.colorSpace->currentText();
    for (const auto& element : COLOR_SPACE_OPTIONS) {
      if (element.first == text) {
        return element.second;
      }
    }
    return -1;
  }

  void colorSpaceChanged()
  {
    auto colorSpace = selectedColorSpace();

    if (!m_lut || colorSpace < 0) {
      return;
    }

    m_lut->SetColorSpace(colorSpace);
  }
};

ColorMapSettingsWidget::ColorMapSettingsWidget(vtkColorTransferFunction* lut,
                                               QWidget* p)
  : Superclass(p), m_internals(new ColorMapSettingsWidget::Internals(lut))
{
  auto& ui = m_internals->ui;
  ui.setupUi(this);
  m_internals->setupConnections();

  updateGui();
}

ColorMapSettingsWidget::~ColorMapSettingsWidget() = default;

void ColorMapSettingsWidget::setLut(vtkColorTransferFunction* lut)
{
  m_internals->setLut(lut);
}

void ColorMapSettingsWidget::updateGui()
{
  m_internals->updateGui();
}

} // namespace tomviz
