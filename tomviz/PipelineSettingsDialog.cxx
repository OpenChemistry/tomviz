/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineSettingsDialog.h"
#include "ui_PipelineSettingsDialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QCloseEvent>
#include <QDebug>
#include <QMetaEnum>
#include <QPushButton>
#include <QFileDialog>
#include <QPushButton>

#include "PipelineManager.h"
#include "Utilities.h"

namespace tomviz {

PipelineSettingsDialog::PipelineSettingsDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::PipelineSettingsDialog)
{
  m_ui->setupUi(this);

  QList<QString> executor;

  m_executorTypeMetaEnum = QMetaEnum::fromType<Pipeline::ExecutionMode>();

  m_ui->modeComboBox->addItem(
    m_executorTypeMetaEnum.valueToKey(Pipeline::ExecutionMode::Threaded));
  m_ui->modeComboBox->addItem(
    m_executorTypeMetaEnum.valueToKey(Pipeline::ExecutionMode::Docker));
  m_ui->modeComboBox->addItem(
    m_executorTypeMetaEnum.valueToKey(Pipeline::ExecutionMode::ExternalPython));

  readSettings();

  m_ui->dockerGroupBox->setHidden(
    m_executorTypeMetaEnum.keyToValue(
      m_ui->modeComboBox->currentText().toLatin1().data()) !=
    Pipeline::ExecutionMode::Docker);

  m_ui->externalGroupBox->setHidden(
    m_executorTypeMetaEnum.keyToValue(
      m_ui->modeComboBox->currentText().toLatin1().data()) !=
    Pipeline::ExecutionMode::ExternalPython);

  connect(m_ui->dockerImageLineEdit, &QLineEdit::textChanged,
          [this](const QString& text) {
            Q_UNUSED(text);
            checkEnableOk();
          });

  connect(m_ui->externalLineEdit, &QLineEdit::textChanged,
          [this](const QString& text) {
            Q_UNUSED(text);
            checkEnableOk();
            this->m_ui->errorLabel->setText("");
          });

  connect(m_ui->modeComboBox, &QComboBox::currentTextChanged,
          [this](const QString& text) {
            auto executionMode =
              m_executorTypeMetaEnum.keyToValue(text.toLatin1().data());
            m_ui->dockerGroupBox->setHidden(executionMode !=
                                            Pipeline::ExecutionMode::Docker);
            m_ui->externalGroupBox->setHidden(executionMode !=
                                              Pipeline::ExecutionMode::ExternalPython);
          });

  connect(this, &QDialog::accepted, this, [this]() {

    PipelineSettings currentSettings;
    auto newMode = m_executorTypeMetaEnum.keyToValue(
      m_ui->modeComboBox->currentText().toLatin1().data());
    if (newMode != currentSettings.executionMode()) {
      PipelineManager::instance().updateExecutionMode(
        static_cast<Pipeline::ExecutionMode>(newMode));
    }

    writeSettings();
  });

  connect(m_ui->buttonBox, &QDialogButtonBox::helpRequested,
          []() { openHelpUrl("pipelines/#configuration"); });

  connect(m_ui->browseButton, &QPushButton::clicked, [this]() {
    auto executable = QFileDialog::getOpenFileName(this, "Select Python executable");
    m_ui->externalLineEdit->setText(executable);
  });

  checkEnableOk();
}

PipelineSettingsDialog::~PipelineSettingsDialog()
{
}

void PipelineSettingsDialog::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  if (!settings->contains("pipeline/geometry")) {
    return;
  }
  setGeometry(settings->value("pipeline/geometry").toRect());

  PipelineSettings pipelineSettings;

  m_ui->modeComboBox->setCurrentText(
    m_executorTypeMetaEnum.valueToKey(pipelineSettings.executionMode()));

  auto dockerImage = pipelineSettings.dockerImage();
  if (!dockerImage.isEmpty()) {
    m_ui->dockerImageLineEdit->setText(dockerImage);
  }

  m_ui->pullImageCheckBox->setChecked(pipelineSettings.dockerPull());
  m_ui->removeContainersCheckBox->setChecked(pipelineSettings.dockerRemove());

  auto pythonExecutable = pipelineSettings.externalPythonExecutablePath();
  if (!pythonExecutable.isEmpty()) {
    m_ui->externalLineEdit->setText(pythonExecutable);
  }
}

void PipelineSettingsDialog::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->setValue("pipeline/geometry", geometry());

  PipelineSettings pipelineSettings;
  pipelineSettings.setExecutionMode(m_ui->modeComboBox->currentText());
  pipelineSettings.setDockerImage(m_ui->dockerImageLineEdit->text());
  pipelineSettings.setDockerPull(m_ui->pullImageCheckBox->isChecked());
  pipelineSettings.setDockerRemove(m_ui->removeContainersCheckBox->isChecked());
  pipelineSettings.setExternalPythonExecutablePath(
    m_ui->externalLineEdit->text());
}

void PipelineSettingsDialog::checkEnableOk()
{

  bool enabled = true;
  auto currentMode = m_executorTypeMetaEnum.keyToValue(
        m_ui->modeComboBox->currentText().toLatin1().data());

  if (currentMode == Pipeline::ExecutionMode::Docker) {
    enabled = !m_ui->dockerImageLineEdit->text().isEmpty();
  }
  else if (currentMode == Pipeline::ExecutionMode::ExternalPython) {
    enabled = !m_ui->externalLineEdit->text().isEmpty();
  }

  m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

bool PipelineSettingsDialog::validatePythonEnvironment()
{
  auto pythonExecutable = QFileInfo(m_ui->externalLineEdit->text());
  if (!pythonExecutable.exists()) {
    m_ui->errorLabel->setText("The external python executable doesn't exist.");
    return false;
  }

  auto baseDir = pythonExecutable.dir();
  auto tomvizPipelineExecutable = QFileInfo(baseDir.filePath("tomviz-pipeline"));
  if (!tomvizPipelineExecutable.exists()) {
    m_ui->errorLabel->setText("Unable to find tomviz-pipeline executable, please ensure " \
            "tomviz package has been installed in python environment.");
    return false;
  }

  return true;
}

void PipelineSettingsDialog::done(int r)
{
  auto currentMode = m_executorTypeMetaEnum.keyToValue(
    m_ui->modeComboBox->currentText().toLatin1().data());

  if (currentMode != Pipeline::ExecutionMode::ExternalPython) {
    QDialog::done(r);
    return;
  }

  // Perform validation for external Python
  if(QDialog::Accepted != r || validatePythonEnvironment()) {
    QDialog::done(r);
  }
}

} // namespace tomviz
