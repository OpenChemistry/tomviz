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

#include "WebExportWidget.h"

#include <vtkObject.h>

#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"
#include "pqFileDialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace tomviz {

WebExportWidget::WebExportWidget(QWidget* p) : QDialog(p)
{
  QVBoxLayout* v = new QVBoxLayout(this);
  this->setMinimumWidth(500);
  this->setMinimumHeight(200);
  this->setWindowTitle("Web export data");

  // Output directory path
  QLabel* outputDirectorylabel = new QLabel("Output directory:");
  QHBoxLayout* pathGroup = new QHBoxLayout;
  this->outputPath = new QLineEdit();
  this->browseButton = new QPushButton("Browse");
  pathGroup->addWidget(outputDirectorylabel);
  pathGroup->addWidget(this->outputPath);
  pathGroup->addWidget(this->browseButton);
  v->addLayout(pathGroup);

  // Output type
  QLabel* outputTypelabel = new QLabel("Output type:");
  QHBoxLayout* typeGroup = new QHBoxLayout;
  this->exportType = new QComboBox;
  this->exportType->addItem("Images");
  this->exportType->addItem("Composite surfaces");
  this->exportType->setCurrentIndex(0);
  typeGroup->addWidget(outputTypelabel);
  typeGroup->addWidget(this->exportType, 1);
  v->addLayout(typeGroup);

  // Camera settings
  QLabel* cameraGrouplabel = new QLabel("Camera delta angles:");

  QLabel* phiLabel = new QLabel("Phi");
  this->deltaPhi = new QSpinBox();
  this->deltaPhi->setRange(10, 90);
  this->deltaPhi->setSingleStep(10);
  this->deltaPhi->setValue(30);
  this->deltaPhi->setMinimumWidth(100);

  QLabel* thetaLabel = new QLabel("Theta");
  this->deltaTheta = new QSpinBox();
  this->deltaTheta->setRange(10, 60);
  this->deltaTheta->setSingleStep(5);
  this->deltaTheta->setValue(30);
  this->deltaTheta->setMinimumWidth(100);

  QHBoxLayout* cameraGroup = new QHBoxLayout;
  cameraGroup->addWidget(cameraGrouplabel);
  cameraGroup->addStretch();
  cameraGroup->addWidget(phiLabel);
  cameraGroup->addWidget(deltaPhi);
  cameraGroup->addSpacing(30);
  cameraGroup->addWidget(thetaLabel);
  cameraGroup->addWidget(deltaTheta);
  v->addLayout(cameraGroup);

  v->addStretch();

  // Action buttons
  QHBoxLayout* actionGroup = new QHBoxLayout;
  this->exportButton = new QPushButton("Export");
  this->exportButton->setDisabled(true);
  this->cancelButton = new QPushButton("Cancel");
  actionGroup->addStretch();
  actionGroup->addWidget(this->exportButton);
  actionGroup->addSpacing(20);
  actionGroup->addWidget(this->cancelButton);
  v->addLayout(actionGroup);

  // UI binding
  this->connect(this->outputPath, SIGNAL(textChanged(const QString&)), this,
                SLOT(onPathChange()));
  this->connect(this->browseButton, SIGNAL(pressed()), this, SLOT(onBrowse()));
  this->connect(this->exportButton, SIGNAL(pressed()), this, SLOT(onExport()));
  this->connect(this->cancelButton, SIGNAL(pressed()), this, SLOT(onCancel()));
}

WebExportWidget::~WebExportWidget()
{
}

void WebExportWidget::onBrowse()
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  pqFileDialog fileDialog(server, pqCoreUtilities::mainWidget(),
                          tr("Save Scene for Web:"), QString(), "");
  fileDialog.setObjectName("DirectorySaveDialog");
  fileDialog.setFileMode(pqFileDialog::Directory);
  if (fileDialog.exec() == QDialog::Accepted) {
    this->outputPath->setText(fileDialog.getSelectedFiles()[0]);
    this->exportButton->setDisabled(false);
  }
}

void WebExportWidget::onPathChange()
{
  this->exportButton->setDisabled(!(this->outputPath->text().length() > 3));
}

void WebExportWidget::onExport()
{
  this->accept();
}

void WebExportWidget::onCancel()
{
  this->reject();
}

QString WebExportWidget::getOutputPath()
{
  return this->outputPath->text();
}

int WebExportWidget::getExportType()
{
  return this->exportType->currentIndex();
}

int WebExportWidget::getDeltaPhi()
{
  return this->deltaPhi->value();
}

int WebExportWidget::getDeltaTheta()
{
  return this->deltaTheta->value();
}
}
