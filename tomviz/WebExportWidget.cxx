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
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

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
  this->exportType->addItem("Volume exploration");
  this->exportType->addItem("Contour exploration");
  this->exportType->addItem("Contours Geometry");
  this->exportType->addItem("Contour exploration Geometry");
  this->exportType->addItem("Volume");
  // this->exportType->addItem("Composite surfaces"); // specularColor segfault
  this->exportType->setCurrentIndex(0);
  typeGroup->addWidget(outputTypelabel);
  typeGroup->addWidget(this->exportType, 1);
  v->addLayout(typeGroup);

  // Camera settings
  QLabel* cameraGrouplabel = new QLabel("Camera tilts:");

  QLabel* phiLabel = new QLabel("Phi");
  this->nbPhi = new QSpinBox();
  this->nbPhi->setRange(4, 72);
  this->nbPhi->setSingleStep(4);
  this->nbPhi->setValue(36);
  this->nbPhi->setMinimumWidth(100);

  QLabel* thetaLabel = new QLabel("Theta");
  this->nbTheta = new QSpinBox();
  this->nbTheta->setRange(1, 20);
  this->nbTheta->setSingleStep(1);
  this->nbTheta->setValue(5);
  this->nbTheta->setMinimumWidth(100);

  QHBoxLayout* cameraGroupLayout = new QHBoxLayout;
  cameraGroupLayout->addWidget(cameraGrouplabel);
  cameraGroupLayout->addStretch();
  cameraGroupLayout->addWidget(phiLabel);
  cameraGroupLayout->addWidget(nbPhi);
  cameraGroupLayout->addSpacing(30);
  cameraGroupLayout->addWidget(thetaLabel);
  cameraGroupLayout->addWidget(nbTheta);

  this->cameraGroup = new QWidget();
  this->cameraGroup->setLayout(cameraGroupLayout);
  v->addWidget(this->cameraGroup);

  v->addStretch();

  // Action buttons
  QHBoxLayout* actionGroup = new QHBoxLayout;
  this->keepData = new QCheckBox("Generate data for viewer");
  this->exportButton = new QPushButton("Export");
  this->exportButton->setDisabled(true);
  this->cancelButton = new QPushButton("Cancel");
  actionGroup->addWidget(this->keepData);
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
  this->connect(this->exportType, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onTypeChange(int)));
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

void WebExportWidget::onTypeChange(int index)
{
  this->cameraGroup->setVisible(index < 3);
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

QMap<QString, QVariant>* WebExportWidget::getKeywordArguments()
{
  this->kwargs["executionPath"] =
    QVariant(QCoreApplication::applicationDirPath());
  this->kwargs["destPath"] = QVariant(this->outputPath->text());
  this->kwargs["exportType"] = QVariant(this->exportType->currentIndex());
  this->kwargs["nbPhi"] = QVariant(this->nbPhi->value());
  this->kwargs["nbTheta"] = QVariant(this->nbTheta->value());
  this->kwargs["keepData"] = QVariant(this->keepData->checkState());

  return &this->kwargs;
}
}
