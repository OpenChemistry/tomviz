/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

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

SaveScreenshotDialog::SaveScreenshotDialog(QWidget* p)
  : QDialog(p)
{
  setWindowTitle("Save Screenshot Options");
  auto vLayout = new QVBoxLayout;

  auto label = new QLabel("Select resolution for the image to save");
  vLayout->addWidget(label);

  auto dimensionsLayout = new QHBoxLayout;
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
  dimensionsLayout->addWidget(m_width);
  dimensionsLayout->addWidget(label);
  dimensionsLayout->addWidget(m_height);
  dimensionsLayout->addWidget(lockAspectButton);
  vLayout->addItem(dimensionsLayout);

  label = new QLabel("Override Color Palette");
  vLayout->addWidget(label);

  m_palettes = new QComboBox;
  m_palettes->addItem("Current Palette", "");
  m_palettes->addItem("Transparent Background", "Transparent Background");
  vLayout->addWidget(m_palettes);

  auto buttonBox =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, this,
                   &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, this,
                   &QDialog::reject);
  vLayout->addWidget(buttonBox);

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

}

} // namespace tomviz
