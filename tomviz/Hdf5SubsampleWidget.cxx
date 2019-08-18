/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Hdf5SubsampleWidget.h"
#include "ui_Hdf5SubsampleWidget.h"

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
  {}

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

  QList<QSpinBox*> volumeSpinBoxes() const
  {
    return QList<QSpinBox*>(
      { ui.startX, ui.startY, ui.startZ, ui.endX, ui.endY, ui.endZ });
  }

  QList<QSpinBox*> allSpinBoxes() const
  {
    auto ret = volumeSpinBoxes();
    ret += ui.stride;
    return ret;
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

    int stride = ui.stride->value();
    size_t s3 = stride * stride * stride;

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
}

Hdf5SubsampleWidget::~Hdf5SubsampleWidget() = default;

void Hdf5SubsampleWidget::valueChanged()
{
  m_internals->updateRanges();
  m_internals->updateSizeString();
}

void Hdf5SubsampleWidget::bounds(int bs[6]) const
{
  m_internals->bounds(bs);
}

int Hdf5SubsampleWidget::stride() const
{
  return m_internals->ui.stride->value();
}

} // namespace tomviz
