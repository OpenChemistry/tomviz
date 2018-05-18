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
#include "PipelineSettingsDialog.h"
#include "ui_PipelineSettingsDialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QCheckBox>
#include <QCloseEvent>
#include <QDebug>
#include <QMetaEnum>
#include <QPushButton>

#include "PipelineManager.h"

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

  readSettings();

  m_ui->dockerGroupBox->setHidden(
    m_executorTypeMetaEnum.keyToValue(
      m_ui->modeComboBox->currentText().toLatin1().data()) !=
    Pipeline::ExecutionMode::Docker);

  connect(m_ui->dockerImageLineEdit, &QLineEdit::textChanged,
          [this](const QString& text) {
            Q_UNUSED(text);
            checkEnableOk();
          });

  connect(m_ui->modeComboBox, &QComboBox::currentTextChanged,
          [this](const QString& text) {
            auto executionMode =
              m_executorTypeMetaEnum.keyToValue(text.toLatin1().data());
            m_ui->dockerGroupBox->setHidden(executionMode !=
                                            Pipeline::ExecutionMode::Docker);
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
}

void PipelineSettingsDialog::checkEnableOk()
{

  bool enabled = true;

  if (m_executorTypeMetaEnum.keyToValue(
        m_ui->modeComboBox->currentText().toLatin1().data()) ==
      Pipeline::ExecutionMode::Docker) {
    enabled = !m_ui->dockerImageLineEdit->text().isEmpty();
  }

  m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

} // namespace tomviz
