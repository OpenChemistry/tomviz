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

#include <pqActiveObjects.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
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
  this->setMinimumHeight(400);
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
  this->exportType->addItem("Images: Current scene");
  this->exportType->addItem("Images: Volume exploration");
  this->exportType->addItem("Images: Contour exploration");
  this->exportType->addItem("Geometry: Current scene contour(s)");
  this->exportType->addItem("Geometry: Contour exploration");
  this->exportType->addItem("Geometry: Volume");
  // this->exportType->addItem("Composite surfaces"); // specularColor segfault
  this->exportType->setCurrentIndex(0);
  typeGroup->addWidget(outputTypelabel);
  typeGroup->addWidget(this->exportType, 1);
  v->addLayout(typeGroup);

  // Image size
  QLabel* imageSizeGroupLabel = new QLabel("View size:");

  QLabel* imageWidthLabel = new QLabel("Width");
  this->imageWidth = new QSpinBox();
  this->imageWidth->setRange(50, 2048);
  this->imageWidth->setSingleStep(1);
  this->imageWidth->setValue(500);
  this->imageWidth->setMinimumWidth(100);

  QLabel* imageHeightLabel = new QLabel("Height");
  this->imageHeight = new QSpinBox();
  this->imageHeight->setRange(50, 2048);
  this->imageHeight->setSingleStep(1);
  this->imageHeight->setValue(500);
  this->imageHeight->setMinimumWidth(100);

  QHBoxLayout* imageSizeGroupLayout = new QHBoxLayout;
  imageSizeGroupLayout->addWidget(imageSizeGroupLabel);
  imageSizeGroupLayout->addStretch();
  imageSizeGroupLayout->addWidget(imageWidthLabel);
  imageSizeGroupLayout->addWidget(imageWidth);
  imageSizeGroupLayout->addSpacing(30);
  imageSizeGroupLayout->addWidget(imageHeightLabel);
  imageSizeGroupLayout->addWidget(imageHeight);

  this->imageSizeGroup = new QWidget();
  this->imageSizeGroup->setLayout(imageSizeGroupLayout);
  v->addWidget(this->imageSizeGroup);

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

  // Volume exploration
  QLabel* opacityLabel = new QLabel("Max opacity");
  this->maxOpacity = new QSpinBox();
  this->maxOpacity->setRange(10, 100);
  this->maxOpacity->setSingleStep(10);
  this->maxOpacity->setValue(50);
  this->maxOpacity->setMinimumWidth(100);

  QLabel* spanLabel = new QLabel("Tent width");
  this->spanValue = new QSpinBox();
  this->spanValue->setRange(1, 200);
  this->spanValue->setSingleStep(1);
  this->spanValue->setValue(10);
  this->spanValue->setMinimumWidth(100);

  QHBoxLayout* volumeExplorationGroupLayout = new QHBoxLayout;
  volumeExplorationGroupLayout->addWidget(opacityLabel);
  volumeExplorationGroupLayout->addWidget(maxOpacity);
  cameraGroupLayout->addStretch();
  volumeExplorationGroupLayout->addWidget(spanLabel);
  volumeExplorationGroupLayout->addWidget(spanValue);

  this->volumeExplorationGroup = new QWidget();
  this->volumeExplorationGroup->setLayout(volumeExplorationGroupLayout);
  v->addWidget(this->volumeExplorationGroup);

  // Multi-value exploration
  QLabel* multiValueLabel = new QLabel("Values:");
  this->multiValue = new QLineEdit("25, 50, 75, 100, 125, 150, 175, 200, 225");

  QHBoxLayout* multiValueGroupLayout = new QHBoxLayout;
  multiValueGroupLayout->addWidget(multiValueLabel);
  multiValueGroupLayout->addWidget(this->multiValue);

  this->valuesGroup = new QWidget();
  this->valuesGroup->setLayout(multiValueGroupLayout);
  v->addWidget(this->valuesGroup);

  // Volume down sampling
  QLabel* scaleLabel = new QLabel("Sampling stride");
  this->scale = new QSpinBox();
  this->scale->setRange(1, 5);
  this->scale->setSingleStep(1);
  this->scale->setValue(1);
  this->scale->setMinimumWidth(100);

  QHBoxLayout* scaleGroupLayout = new QHBoxLayout;
  scaleGroupLayout->addWidget(scaleLabel);
  scaleGroupLayout->addWidget(this->scale);

  this->volumeResampleGroup = new QWidget();
  this->volumeResampleGroup->setLayout(scaleGroupLayout);
  v->addWidget(this->volumeResampleGroup);

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

  // Initialize visibility
  this->onTypeChange(0);
}

void WebExportWidget::onBrowse()
{
  QFileDialog fileDialog(tomviz::mainWidget(),
                         tr("Save Scene for Web:"));
  fileDialog.setObjectName("DirectorySaveDialog");
  fileDialog.setFileMode(QFileDialog::Directory);
  if (fileDialog.exec() == QDialog::Accepted) {
    this->outputPath->setText(fileDialog.selectedFiles()[0]);
    this->exportButton->setDisabled(false);
  }
}

void WebExportWidget::onTypeChange(int index)
{
  pqView* view = pqActiveObjects::instance().activeView();
  QSize size = view->getSize();
  this->imageWidth->setMaximum(size.width());
  this->imageHeight->setMaximum(size.height());

  this->imageSizeGroup->setVisible(index < 3);
  this->cameraGroup->setVisible(index < 3);
  this->volumeExplorationGroup->setVisible(index == 1);
  this->valuesGroup->setVisible(index == 1 || index == 2 || index == 4);
  this->volumeResampleGroup->setVisible(index == 5);
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
  this->kwargs["imageWidth"] = QVariant(this->imageWidth->value());
  this->kwargs["imageHeight"] = QVariant(this->imageHeight->value());
  this->kwargs["nbPhi"] = QVariant(this->nbPhi->value());
  this->kwargs["nbTheta"] = QVariant(this->nbTheta->value());
  this->kwargs["keepData"] = QVariant(this->keepData->checkState());
  this->kwargs["maxOpacity"] = QVariant(this->maxOpacity->value());
  this->kwargs["tentWidth"] = QVariant(this->spanValue->value());
  this->kwargs["volumeScale"] = QVariant(this->scale->value());
  this->kwargs["multiValue"] = QVariant(this->multiValue->text());

  return &this->kwargs;
}
}
