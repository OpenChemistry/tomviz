/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
  setMinimumWidth(500);
  setMinimumHeight(400);
  setWindowTitle("Web export data");

  // Output type
  QLabel* outputTypelabel = new QLabel("Output type:");
  QHBoxLayout* typeGroup = new QHBoxLayout;
  m_exportType = new QComboBox;
  m_exportType->addItem("Images: Current scene");
  m_exportType->addItem("Images: Volume exploration");
  m_exportType->addItem("Images: Contour exploration");
  m_exportType->addItem("Geometry: Current scene contour(s)");
  m_exportType->addItem("Geometry: Contour exploration");
  m_exportType->addItem("Geometry: Volume");
  // exportType->addItem("Composite surfaces"); // specularColor segfault
  m_exportType->setCurrentIndex(0);
  typeGroup->addWidget(outputTypelabel);
  typeGroup->addWidget(m_exportType, 1);
  v->addLayout(typeGroup);

  // Image size
  QLabel* imageSizeGroupLabel = new QLabel("View size:");

  QLabel* imageWidthLabel = new QLabel("Width");
  m_imageWidth = new QSpinBox();
  m_imageWidth->setRange(50, 2048);
  m_imageWidth->setSingleStep(1);
  m_imageWidth->setValue(500);
  m_imageWidth->setMinimumWidth(100);

  QLabel* imageHeightLabel = new QLabel("Height");
  m_imageHeight = new QSpinBox();
  m_imageHeight->setRange(50, 2048);
  m_imageHeight->setSingleStep(1);
  m_imageHeight->setValue(500);
  m_imageHeight->setMinimumWidth(100);

  QHBoxLayout* imageSizeGroupLayout = new QHBoxLayout;
  imageSizeGroupLayout->addWidget(imageSizeGroupLabel);
  imageSizeGroupLayout->addStretch();
  imageSizeGroupLayout->addWidget(imageWidthLabel);
  imageSizeGroupLayout->addWidget(m_imageWidth);
  imageSizeGroupLayout->addSpacing(30);
  imageSizeGroupLayout->addWidget(imageHeightLabel);
  imageSizeGroupLayout->addWidget(m_imageHeight);

  m_imageSizeGroup = new QWidget();
  m_imageSizeGroup->setLayout(imageSizeGroupLayout);
  v->addWidget(m_imageSizeGroup);

  // Camera settings
  QLabel* cameraGrouplabel = new QLabel("Camera tilts:");

  QLabel* phiLabel = new QLabel("Phi");
  m_nbPhi = new QSpinBox();
  m_nbPhi->setRange(4, 72);
  m_nbPhi->setSingleStep(4);
  m_nbPhi->setValue(36);
  m_nbPhi->setMinimumWidth(100);

  QLabel* thetaLabel = new QLabel("Theta");
  m_nbTheta = new QSpinBox();
  m_nbTheta->setRange(1, 20);
  m_nbTheta->setSingleStep(1);
  m_nbTheta->setValue(5);
  m_nbTheta->setMinimumWidth(100);

  QHBoxLayout* cameraGroupLayout = new QHBoxLayout;
  cameraGroupLayout->addWidget(cameraGrouplabel);
  cameraGroupLayout->addStretch();
  cameraGroupLayout->addWidget(phiLabel);
  cameraGroupLayout->addWidget(m_nbPhi);
  cameraGroupLayout->addSpacing(30);
  cameraGroupLayout->addWidget(thetaLabel);
  cameraGroupLayout->addWidget(m_nbTheta);

  m_cameraGroup = new QWidget();
  m_cameraGroup->setLayout(cameraGroupLayout);
  v->addWidget(m_cameraGroup);

  // Volume exploration
  QLabel* opacityLabel = new QLabel("Max opacity");
  m_maxOpacity = new QSpinBox();
  m_maxOpacity->setRange(10, 100);
  m_maxOpacity->setSingleStep(10);
  m_maxOpacity->setValue(50);
  m_maxOpacity->setMinimumWidth(100);

  QLabel* spanLabel = new QLabel("Tent width");
  m_spanValue = new QSpinBox();
  m_spanValue->setRange(1, 200);
  m_spanValue->setSingleStep(1);
  m_spanValue->setValue(10);
  m_spanValue->setMinimumWidth(100);

  QHBoxLayout* volumeExplorationGroupLayout = new QHBoxLayout;
  volumeExplorationGroupLayout->addWidget(opacityLabel);
  volumeExplorationGroupLayout->addWidget(m_maxOpacity);
  cameraGroupLayout->addStretch();
  volumeExplorationGroupLayout->addWidget(spanLabel);
  volumeExplorationGroupLayout->addWidget(m_spanValue);

  m_volumeExplorationGroup = new QWidget();
  m_volumeExplorationGroup->setLayout(volumeExplorationGroupLayout);
  v->addWidget(m_volumeExplorationGroup);

  // Multi-value exploration
  QLabel* multiValueLabel = new QLabel("Values:");
  m_multiValue = new QLineEdit("25, 50, 75, 100, 125, 150, 175, 200, 225");

  QHBoxLayout* multiValueGroupLayout = new QHBoxLayout;
  multiValueGroupLayout->addWidget(multiValueLabel);
  multiValueGroupLayout->addWidget(m_multiValue);

  m_valuesGroup = new QWidget();
  m_valuesGroup->setLayout(multiValueGroupLayout);
  v->addWidget(m_valuesGroup);

  // Volume down sampling
  QLabel* scaleLabel = new QLabel("Sampling stride");
  m_scale = new QSpinBox();
  m_scale->setRange(1, 5);
  m_scale->setSingleStep(1);
  m_scale->setValue(1);
  m_scale->setMinimumWidth(100);

  QHBoxLayout* scaleGroupLayout = new QHBoxLayout;
  scaleGroupLayout->addWidget(scaleLabel);
  scaleGroupLayout->addWidget(m_scale);

  m_volumeResampleGroup = new QWidget();
  m_volumeResampleGroup->setLayout(scaleGroupLayout);
  v->addWidget(m_volumeResampleGroup);

  v->addStretch();

  // Action buttons
  QHBoxLayout* actionGroup = new QHBoxLayout;
  m_keepData = new QCheckBox("Generate data for viewer");
  m_exportButton = new QPushButton("Export");
  m_cancelButton = new QPushButton("Cancel");
  actionGroup->addWidget(m_keepData);
  actionGroup->addStretch();
  actionGroup->addWidget(m_exportButton);
  actionGroup->addSpacing(20);
  actionGroup->addWidget(m_cancelButton);
  v->addLayout(actionGroup);

  // UI binding
  connect(m_exportButton, SIGNAL(pressed()), this, SLOT(onExport()));
  connect(m_cancelButton, SIGNAL(pressed()), this, SLOT(onCancel()));
  connect(m_exportType, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onTypeChange(int)));

  // Initialize visibility
  onTypeChange(0);

  restoreSettings();

  connect(this, &QDialog::finished, this,
          &WebExportWidget::writeWidgetSettings);
}

void WebExportWidget::onTypeChange(int index)
{
  pqView* view = pqActiveObjects::instance().activeView();
  QSize size = view->getSize();
  m_imageWidth->setMaximum(size.width());
  m_imageHeight->setMaximum(size.height());

  m_imageSizeGroup->setVisible(index < 3);
  m_cameraGroup->setVisible(index < 3);
  m_volumeExplorationGroup->setVisible(index == 1);
  m_valuesGroup->setVisible(index == 1 || index == 2 || index == 4);
  m_volumeResampleGroup->setVisible(index == 5);
}

void WebExportWidget::onExport()
{
  accept();
}

void WebExportWidget::onCancel()
{
  reject();
}

QMap<QString, QVariant> WebExportWidget::getKeywordArguments()
{
  m_kwargs["executionPath"] = QVariant(QCoreApplication::applicationDirPath());
  m_kwargs["exportType"] = QVariant(m_exportType->currentIndex());
  m_kwargs["imageWidth"] = QVariant(m_imageWidth->value());
  m_kwargs["imageHeight"] = QVariant(m_imageHeight->value());
  m_kwargs["nbPhi"] = QVariant(m_nbPhi->value());
  m_kwargs["nbTheta"] = QVariant(m_nbTheta->value());
  m_kwargs["keepData"] = QVariant(m_keepData->checkState());
  m_kwargs["maxOpacity"] = QVariant(m_maxOpacity->value());
  m_kwargs["tentWidth"] = QVariant(m_spanValue->value());
  m_kwargs["volumeScale"] = QVariant(m_scale->value());
  m_kwargs["multiValue"] = QVariant(m_multiValue->text());

  return m_kwargs;
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

  settingsMap["phi"] = m_nbPhi->value();
  settingsMap["theta"] = m_nbTheta->value();
  settingsMap["imageWidth"] = m_imageWidth->value();
  settingsMap["imageHeight"] = m_imageHeight->value();
  settingsMap["generateDataViewer"] = m_keepData->isChecked();
  settingsMap["exportType"] = m_exportType->currentIndex();
  settingsMap["maxOpacity"] = m_maxOpacity->value();
  settingsMap["tentWidth"] = m_spanValue->value();
  settingsMap["volumeScale"] = m_scale->value();
  settingsMap["multiValue"] = m_multiValue->text();

  writeSettings(settingsMap);
}

void WebExportWidget::restoreSettings()
{
  QVariantMap settingsMap = readSettings();
  if (settingsMap.contains("phi")) {
    m_nbPhi->setValue(settingsMap.value("phi").toInt());
  }

  if (settingsMap.contains("theta")) {
    m_nbTheta->setValue(settingsMap.value("theta").toInt());
  }

  if (settingsMap.contains("imageWidth")) {
    m_imageWidth->setValue(settingsMap.value("imageWidth").toInt());
  }

  if (settingsMap.contains("imageHeight")) {
    m_imageHeight->setValue(settingsMap.value("imageHeight").toInt());
  }

  if (settingsMap.contains("generateDataViewer")) {
    m_keepData->setChecked(settingsMap.value("generateDataViewer").toBool());
  }

  if (settingsMap.contains("exportType")) {
    m_exportType->setCurrentIndex(settingsMap.value("exportType").toInt());
  }

  if (settingsMap.contains("maxOpacity")) {
    m_maxOpacity->setValue(settingsMap.value("maxOpacity").toInt());
  }

  if (settingsMap.contains("tentWidth")) {

    m_spanValue->setValue(settingsMap.value("tentWidth").toInt());
  }

  if (settingsMap.contains("volumeScale")) {
    m_scale->setValue(settingsMap.value("volumeScale").toInt());
  }

  if (settingsMap.contains("multiValue")) {
    m_multiValue->setText(settingsMap.value("multiValue").toString());
  }
}
} // namespace tomviz
