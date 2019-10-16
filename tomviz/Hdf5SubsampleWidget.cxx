/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Hdf5SubsampleWidget.h"
#include "ui_Hdf5SubsampleWidget.h"

#include <array>

namespace tomviz {

template <typename T>
QString getSizeNearestThousand(T num, bool labelAsBytes = false)
{
  char format = 'f';
  int prec = 1;

  QString ret;
  if (num < 1e3)
    ret = QString::number(num) + " ";
  else if (num < 1e6)
    ret = QString::number(num / 1e3, format, prec) + " K";
  else if (num < 1e9)
    ret = QString::number(num / 1e6, format, prec) + " M";
  else if (num < 1e12)
    ret = QString::number(num / 1e9, format, prec) + " G";
  else
    ret = QString::number(num / 1e12, format, prec) + " T";

  if (labelAsBytes)
    ret += "B";

  return ret;
}

class Hdf5SubsampleWidget::Internals
{
public:
  Internals(int dimensions[3], int dataTypeSize)
    : dims({ dimensions[0], dimensions[1], dimensions[2] }),
      dataSize(dataTypeSize)
  {
  }

  std::array<int, 3> dims;
  int dataSize;
  Ui::Hdf5SubsampleWidget ui;

  void setDefaults()
  {
    ui.startX->setValue(0);
    ui.startY->setValue(0);
    ui.startZ->setValue(0);
    ui.endX->setValue(dims[0]);
    ui.endY->setValue(dims[1]);
    ui.endZ->setValue(dims[2]);

    // Show the upper limits with tooltips
    ui.endX->setToolTip("Max: " + QString::number(dims[0]));
    ui.endY->setToolTip("Max: " + QString::number(dims[1]));
    ui.endZ->setToolTip("Max: " + QString::number(dims[2]));

    updateRanges();
    updateSizeString();
  }

  void updateRanges()
  {
    blockSpinnerSignals(true);

    int bs[6];
    bounds(bs);

    ui.startX->setRange(0, bs[1]);
    ui.startY->setRange(0, bs[3]);
    ui.startZ->setRange(0, bs[5]);
    ui.endX->setRange(bs[0], dims[0]);
    ui.endY->setRange(bs[2], dims[1]);
    ui.endZ->setRange(bs[4], dims[2]);

    blockSpinnerSignals(false);
  }

  void bounds(int bs[6]) const
  {
    int index = 0;
    bs[index++] = ui.startX->value();
    bs[index++] = ui.endX->value();
    bs[index++] = ui.startY->value();
    bs[index++] = ui.endY->value();
    bs[index++] = ui.startZ->value();
    bs[index++] = ui.endZ->value();
  }

  void setBounds(int bs[6])
  {
    // If any of these are less than 0, just ignore the request
    for (int i = 0; i < 6; ++i) {
      if (bs[i] < 0)
        return;
    }

    blockSpinnerSignals(true);

    int index = 0;
    ui.startX->setValue(bs[index++]);
    ui.endX->setValue(bs[index++]);
    ui.startY->setValue(bs[index++]);
    ui.endY->setValue(bs[index++]);
    ui.startZ->setValue(bs[index++]);
    ui.endZ->setValue(bs[index++]);

    blockSpinnerSignals(false);
  }

  void setStrides(int s[3])
  {
    blockSpinnerSignals(true);

    // If any are less than one, use 1 instead
    ui.strideX->setValue((s[0] > 0 ? s[0] : 1));
    ui.strideY->setValue((s[1] > 0 ? s[1] : 1));
    ui.strideZ->setValue((s[2] > 0 ? s[2] : 1));

    // Check that the same stride is used if they are the same
    ui.sameStride->setChecked(s[0] == s[1] && s[0] == s[2]);

    blockSpinnerSignals(false);
  }

  void strides(int s[3]) const
  {
    s[0] = ui.strideX->value();

    if (ui.sameStride->isChecked()) {
      s[1] = s[0];
      s[2] = s[0];
    } else {
      s[1] = ui.strideY->value();
      s[2] = ui.strideZ->value();
    }
  }

  QList<QSpinBox*> volumeSpinBoxes() const
  {
    return { ui.startX, ui.startY, ui.startZ, ui.endX, ui.endY, ui.endZ };
  }

  QList<QSpinBox*> strideSpinBoxes() const
  {
    return { ui.strideX, ui.strideY, ui.strideZ };
  }

  QList<QSpinBox*> allSpinBoxes() const
  {
    return volumeSpinBoxes() + strideSpinBoxes();
  }

  void blockSpinnerSignals(bool block)
  {
    auto boxes = allSpinBoxes();
    for (auto* box : boxes)
      box->blockSignals(block);
  }

  size_t calculateSize() const
  {
    int b[6];
    bounds(b);

    int strideX = ui.strideX->value();
    int strideY = ui.strideY->value();
    int strideZ = ui.strideZ->value();

    size_t s3;
    if (ui.sameStride->isChecked())
      s3 = strideX * strideX * strideX;
    else
      s3 = strideX * strideY * strideZ;

    // Cast one to size_t so that they all get promoted to size_t
    return static_cast<size_t>(b[1] - b[0]) * (b[3] - b[2]) * (b[5] - b[4]) *
           dataSize / s3;
  }

  void updateSizeString() const
  {
    auto size = calculateSize();
    QString sizeStr = getSizeNearestThousand(size, true);
    ui.memory->setText(sizeStr);
  }
};

Hdf5SubsampleWidget::Hdf5SubsampleWidget(int dims[3], int dataTypeSize,
                                         QWidget* p)
  : Superclass(p),
    m_internals(new Hdf5SubsampleWidget::Internals(dims, dataTypeSize))
{
  auto& ui = m_internals->ui;
  ui.setupUi(this);

  m_internals->setDefaults();

  auto boxes = m_internals->allSpinBoxes();
  for (auto* box : boxes) {
    connect(box, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Hdf5SubsampleWidget::valueChanged);
  }
  connect(ui.sameStride, &QCheckBox::toggled, this,
          &Hdf5SubsampleWidget::valueChanged);
}

Hdf5SubsampleWidget::~Hdf5SubsampleWidget() = default;

void Hdf5SubsampleWidget::valueChanged()
{
  m_internals->updateRanges();
  m_internals->updateSizeString();
}

void Hdf5SubsampleWidget::setBounds(int bs[6])
{
  m_internals->setBounds(bs);
  valueChanged();
}

void Hdf5SubsampleWidget::bounds(int bs[6]) const
{
  m_internals->bounds(bs);
}

void Hdf5SubsampleWidget::setStrides(int s[3])
{
  m_internals->setStrides(s);
  valueChanged();
}

void Hdf5SubsampleWidget::strides(int s[3]) const
{
  m_internals->strides(s);
}

} // namespace tomviz
