/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveScreenshotDialog.h"

#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace tomviz {

SaveScreenshotDialog::SaveScreenshotDialog(QWidget* p) : QDialog(p)
{
  setWindowTitle("Save Screenshot Options");
  auto vLayout = new QVBoxLayout;

  auto dimensionsLayout = new QHBoxLayout;
  auto label = new QLabel("Resolution:");
  dimensionsLayout->addWidget(label);
  m_width = new QSpinBox;
  m_width->setRange(42, 42000);
  m_width->setValue(69);
  label = new QLabel("x");
  m_height = new QSpinBox;
  m_height->setRange(42, 42000);
  m_height->setValue(69);
  auto lockAspectButton =
    new QPushButton(QIcon(":/pqWidgets/Icons/pqLock24.png"), "");
  lockAspectButton->setToolTip("Lock aspect ratio");
  lockAspectButton->setCheckable(true);
  dimensionsLayout->addWidget(m_width);
  dimensionsLayout->addWidget(label);
  dimensionsLayout->addWidget(m_height);
  dimensionsLayout->addWidget(lockAspectButton);
  vLayout->addItem(dimensionsLayout);

  auto paletteLayout = new QHBoxLayout;
  label = new QLabel("Palette:");
  paletteLayout->addWidget(label);

  m_palettes = new QComboBox;
  m_palettes->addItem("Current Palette", "");
  m_palettes->addItem("Transparent Background", "Transparent Background");
  paletteLayout->addWidget(m_palettes);
  vLayout->addItem(paletteLayout);

  auto buttonBox =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, this,
                   &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, this,
                   &QDialog::reject);
  vLayout->addWidget(buttonBox);

  QObject::connect(lockAspectButton, SIGNAL(clicked()), this,
                   SLOT(setLockAspectRatio()));

  setLayout(vLayout);
}

SaveScreenshotDialog::~SaveScreenshotDialog() = default;

void SaveScreenshotDialog::setSize(int w, int h)
{
  m_width->setValue(w);
  m_height->setValue(h);
}

int SaveScreenshotDialog::width() const
{
  return m_width->value();
}

int SaveScreenshotDialog::height() const
{
  return m_height->value();
}

void SaveScreenshotDialog::addPalette(const QString& name, const QString& key)
{
  m_palettes->addItem(name, key);
}

QString SaveScreenshotDialog::palette() const
{
  return m_palettes->itemData(m_palettes->currentIndex()).toString();
}

void SaveScreenshotDialog::setLockAspectRatio()
{
  m_lockAspectRatio = !m_lockAspectRatio;
  if (m_lockAspectRatio) {
    m_aspectRatio = m_width->value() / static_cast<double>(m_height->value());
    connect(m_width, SIGNAL(valueChanged(int)), this, SLOT(widthChanged(int)));
    connect(m_height, SIGNAL(valueChanged(int)), this,
            SLOT(heightChanged(int)));
  } else {
    disconnect(m_width, SIGNAL(valueChanged(int)), this,
               SLOT(widthChanged(int)));
    disconnect(m_height, SIGNAL(valueChanged(int)), this,
               SLOT(heightChanged(int)));
  }
}

void SaveScreenshotDialog::widthChanged(int i)
{
  m_height->blockSignals(true);
  m_height->setValue(i / m_aspectRatio);
  m_height->blockSignals(false);
}

void SaveScreenshotDialog::heightChanged(int i)
{
  m_width->blockSignals(true);
  m_width->setValue(i * m_aspectRatio);
  m_width->blockSignals(false);
}

} // namespace tomviz
