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
#include <pqSettings.h>

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

  // Output type
  QLabel* outputTypelabel = new QLabel("Output type:");
  QHBoxLayout* typeGroup = new QHBoxLayout;
  this->m_exportType = new QComboBox;
  this->m_exportType->addItem("Images: Current scene");
  this->m_exportType->addItem("Images: Volume exploration");
  this->m_exportType->addItem("Images: Contour exploration");
  this->m_exportType->addItem("Geometry: Current scene contour(s)");
  this->m_exportType->addItem("Geometry: Contour exploration");
  this->m_exportType->addItem("Geometry: Volume");
  // this->exportType->addItem("Composite surfaces"); // specularColor segfault
  this->m_exportType->setCurrentIndex(0);
  typeGroup->addWidget(outputTypelabel);
  typeGroup->addWidget(this->m_exportType, 1);
  v->addLayout(typeGroup);

  // Image size
  QLabel* imageSizeGroupLabel = new QLabel("View size:");

  QLabel* imageWidthLabel = new QLabel("Width");
  this->m_imageWidth = new QSpinBox();
  this->m_imageWidth->setRange(50, 2048);
  this->m_imageWidth->setSingleStep(1);
  this->m_imageWidth->setValue(500);
  this->m_imageWidth->setMinimumWidth(100);

  QLabel* imageHeightLabel = new QLabel("Height");
  this->m_imageHeight = new QSpinBox();
  this->m_imageHeight->setRange(50, 2048);
  this->m_imageHeight->setSingleStep(1);
  this->m_imageHeight->setValue(500);
  this->m_imageHeight->setMinimumWidth(100);

  QHBoxLayout* imageSizeGroupLayout = new QHBoxLayout;
  imageSizeGroupLayout->addWidget(imageSizeGroupLabel);
  imageSizeGroupLayout->addStretch();
  imageSizeGroupLayout->addWidget(imageWidthLabel);
  imageSizeGroupLayout->addWidget(m_imageWidth);
  imageSizeGroupLayout->addSpacing(30);
  imageSizeGroupLayout->addWidget(imageHeightLabel);
  imageSizeGroupLayout->addWidget(m_imageHeight);

  this->m_imageSizeGroup = new QWidget();
  this->m_imageSizeGroup->setLayout(imageSizeGroupLayout);
  v->addWidget(this->m_imageSizeGroup);

  // Camera settings
  QLabel* cameraGrouplabel = new QLabel("Camera tilts:");

  QLabel* phiLabel = new QLabel("Phi");
  this->m_nbPhi = new QSpinBox();
  this->m_nbPhi->setRange(4, 72);
  this->m_nbPhi->setSingleStep(4);
  this->m_nbPhi->setValue(36);
  this->m_nbPhi->setMinimumWidth(100);

  QLabel* thetaLabel = new QLabel("Theta");
  this->m_nbTheta = new QSpinBox();
  this->m_nbTheta->setRange(1, 20);
  this->m_nbTheta->setSingleStep(1);
  this->m_nbTheta->setValue(5);
  this->m_nbTheta->setMinimumWidth(100);

  QHBoxLayout* cameraGroupLayout = new QHBoxLayout;
  cameraGroupLayout->addWidget(cameraGrouplabel);
  cameraGroupLayout->addStretch();
  cameraGroupLayout->addWidget(phiLabel);
  cameraGroupLayout->addWidget(m_nbPhi);
  cameraGroupLayout->addSpacing(30);
  cameraGroupLayout->addWidget(thetaLabel);
  cameraGroupLayout->addWidget(m_nbTheta);

  this->m_cameraGroup = new QWidget();
  this->m_cameraGroup->setLayout(cameraGroupLayout);
  v->addWidget(this->m_cameraGroup);

  // Volume exploration
  QLabel* opacityLabel = new QLabel("Max opacity");
  this->m_maxOpacity = new QSpinBox();
  this->m_maxOpacity->setRange(10, 100);
  this->m_maxOpacity->setSingleStep(10);
  this->m_maxOpacity->setValue(50);
  this->m_maxOpacity->setMinimumWidth(100);

  QLabel* spanLabel = new QLabel("Tent width");
  this->m_spanValue = new QSpinBox();
  this->m_spanValue->setRange(1, 200);
  this->m_spanValue->setSingleStep(1);
  this->m_spanValue->setValue(10);
  this->m_spanValue->setMinimumWidth(100);

  QHBoxLayout* volumeExplorationGroupLayout = new QHBoxLayout;
  volumeExplorationGroupLayout->addWidget(opacityLabel);
  volumeExplorationGroupLayout->addWidget(m_maxOpacity);
  cameraGroupLayout->addStretch();
  volumeExplorationGroupLayout->addWidget(spanLabel);
  volumeExplorationGroupLayout->addWidget(m_spanValue);

  this->m_volumeExplorationGroup = new QWidget();
  this->m_volumeExplorationGroup->setLayout(volumeExplorationGroupLayout);
  v->addWidget(this->m_volumeExplorationGroup);

  // Multi-value exploration
  QLabel* multiValueLabel = new QLabel("Values:");
  this->m_multiValue = new QLineEdit("25, 50, 75, 100, 125, 150, 175, 200, 225");

  QHBoxLayout* multiValueGroupLayout = new QHBoxLayout;
  multiValueGroupLayout->addWidget(multiValueLabel);
  multiValueGroupLayout->addWidget(this->m_multiValue);

  this->m_valuesGroup = new QWidget();
  this->m_valuesGroup->setLayout(multiValueGroupLayout);
  v->addWidget(this->m_valuesGroup);

  // Volume down sampling
  QLabel* scaleLabel = new QLabel("Sampling stride");
  this->m_scale = new QSpinBox();
  this->m_scale->setRange(1, 5);
  this->m_scale->setSingleStep(1);
  this->m_scale->setValue(1);
  this->m_scale->setMinimumWidth(100);

  QHBoxLayout* scaleGroupLayout = new QHBoxLayout;
  scaleGroupLayout->addWidget(scaleLabel);
  scaleGroupLayout->addWidget(this->m_scale);

  this->m_volumeResampleGroup = new QWidget();
  this->m_volumeResampleGroup->setLayout(scaleGroupLayout);
  v->addWidget(this->m_volumeResampleGroup);

  v->addStretch();

  // Action buttons
  QHBoxLayout* actionGroup = new QHBoxLayout;
  this->m_keepData = new QCheckBox("Generate data for viewer");
  this->m_exportButton = new QPushButton("Export");
  this->m_cancelButton = new QPushButton("Cancel");
  actionGroup->addWidget(this->m_keepData);
  actionGroup->addStretch();
  actionGroup->addWidget(this->m_exportButton);
  actionGroup->addSpacing(20);
  actionGroup->addWidget(this->m_cancelButton);
  v->addLayout(actionGroup);

  // UI binding
  this->connect(this->m_exportButton, SIGNAL(pressed()), this, SLOT(onExport()));
  this->connect(this->m_cancelButton, SIGNAL(pressed()), this, SLOT(onCancel()));
  this->connect(this->m_exportType, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onTypeChange(int)));

  // Initialize visibility
  this->onTypeChange(0);

  this->restoreSettings();

  connect(this, &QDialog::finished, this,
          &WebExportWidget::writeWidgetSettings);
}

void WebExportWidget::onTypeChange(int index)
{
  pqView* view = pqActiveObjects::instance().activeView();
  QSize size = view->getSize();
  this->m_imageWidth->setMaximum(size.width());
  this->m_imageHeight->setMaximum(size.height());

  this->m_imageSizeGroup->setVisible(index < 3);
  this->m_cameraGroup->setVisible(index < 3);
  this->m_volumeExplorationGroup->setVisible(index == 1);
  this->m_valuesGroup->setVisible(index == 1 || index == 2 || index == 4);
  this->m_volumeResampleGroup->setVisible(index == 5);
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
  this->m_kwargs["executionPath"] =
    QVariant(QCoreApplication::applicationDirPath());
  this->m_kwargs["exportType"] = QVariant(this->m_exportType->currentIndex());
  this->m_kwargs["imageWidth"] = QVariant(this->m_imageWidth->value());
  this->m_kwargs["imageHeight"] = QVariant(this->m_imageHeight->value());
  this->m_kwargs["nbPhi"] = QVariant(this->m_nbPhi->value());
  this->m_kwargs["nbTheta"] = QVariant(this->m_nbTheta->value());
  this->m_kwargs["keepData"] = QVariant(this->m_keepData->checkState());
  this->m_kwargs["maxOpacity"] = QVariant(this->m_maxOpacity->value());
  this->m_kwargs["tentWidth"] = QVariant(this->m_spanValue->value());
  this->m_kwargs["volumeScale"] = QVariant(this->m_scale->value());
  this->m_kwargs["multiValue"] = QVariant(this->m_multiValue->text());

  return &this->m_kwargs;
}

QMap<QString, QVariant> WebExportWidget::readSettings()
{
  QMap<QString, QVariant> settingsMap;

  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("web");
  foreach (QString key, settings->childKeys()) {
    settingsMap[key] = settings->value(key);
  }
  settings->endGroup();

  return settingsMap;
}

void WebExportWidget::writeSettings(const QMap<QString, QVariant>& settingsMap)
{

  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("web");
  for (QMap<QString, QVariant>::const_iterator iter = settingsMap.begin();
       iter != settingsMap.end(); ++iter) {
    settings->setValue(iter.key(), iter.value());
  }
  settings->endGroup();
}

void WebExportWidget::writeWidgetSettings()
{
  QVariantMap settingsMap;

  settingsMap["phi"] = this->m_nbPhi->value();
  settingsMap["theta"] = this->m_nbTheta->value();
  settingsMap["imageWidth"] = this->m_imageWidth->value();
  settingsMap["imageHeight"] = this->m_imageHeight->value();
  settingsMap["generateDataViewer"] = this->m_keepData->isChecked();
  settingsMap["exportType"] = this->m_exportType->currentIndex();
  settingsMap["maxOpacity"] = this->m_maxOpacity->value();
  settingsMap["tentWidth"] = this->m_spanValue->value();
  settingsMap["volumeScale"] = this->m_scale->value();
  settingsMap["multiValue"] = this->m_multiValue->text();

  this->writeSettings(settingsMap);
}

void WebExportWidget::restoreSettings()
{
  QVariantMap settingsMap = this->readSettings();
  if (settingsMap.contains("phi")) {
    this->m_nbPhi->setValue(settingsMap.value("phi").toInt());
  }

  if (settingsMap.contains("theta")) {
    this->m_nbTheta->setValue(settingsMap.value("theta").toInt());
  }

  if (settingsMap.contains("imageWidth")) {
    this->m_imageWidth->setValue(settingsMap.value("imageWidth").toInt());
  }

  if (settingsMap.contains("imageHeight")) {
    this->m_imageHeight->setValue(settingsMap.value("imageHeight").toInt());
  }

  if (settingsMap.contains("generateDataViewer")) {
    this->m_keepData->setChecked(
      settingsMap.value("generateDataViewer").toBool());
  }

  if (settingsMap.contains("exportType")) {
    this->m_exportType->setCurrentIndex(
      settingsMap.value("exportType").toInt());
  }

  if (settingsMap.contains("maxOpacity")) {
    this->m_maxOpacity->setValue(settingsMap.value("maxOpacity").toInt());
  }

  if (settingsMap.contains("tentWidth")) {

    this->m_spanValue->setValue(settingsMap.value("tentWidth").toInt());
  }

  if (settingsMap.contains("volumeScale")) {
    this->m_scale->setValue(settingsMap.value("volumeScale").toInt());
  }

  if (settingsMap.contains("multiValue")) {
    this->m_multiValue->setText(settingsMap.value("multiValue").toString());
  }
}
}
