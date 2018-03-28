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

#include "StartServerDialog.h"
#include "ui_StartServerDialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QFileDialog>
#include <QFontMetrics>

const char* PYTHON_PATH_DEFAULT = "Enter Python path ...";

namespace tomviz {

StartServerDialog::StartServerDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::StartServerDialog)
{
  m_ui->setupUi(this);

  connect(m_ui->pythonPathLineEdit, &QLineEdit::textChanged, [this]() {
    auto currentText = this->m_ui->pythonPathLineEdit->text();
    auto valid = !currentText.isEmpty() && currentText != PYTHON_PATH_DEFAULT;
    this->m_ui->startButton->setEnabled(valid);
    if (valid) {
      this->setPythonExecutablePath(currentText);
    }
  });

  this->readSettings();

  connect(m_ui->browseButton, &QPushButton::clicked, [this]() {
    QFileInfo info(this->m_pythonExecutablePath);
    auto pythonExecutablePath = QFileDialog::getOpenFileName(
      this, "Select Python Executable", info.dir().path());

    if (!pythonExecutablePath.isEmpty()) {
      this->setPythonExecutablePath(pythonExecutablePath);
      this->writeSettings();
    }
  });

  connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_ui->startButton, &QPushButton::clicked, this, &QDialog::accept);
}

StartServerDialog::~StartServerDialog() = default;

void StartServerDialog::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  if (!settings->contains("pythonExecutablePath")) {
    this->setPythonExecutablePath(PYTHON_PATH_DEFAULT);
  } else {
    this->setPythonExecutablePath(
      settings->value("pythonExecutablePath").toString());
  }
  settings->endGroup();
}

void StartServerDialog::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  settings->setValue("pythonExecutablePath", this->m_pythonExecutablePath);
  settings->endGroup();
}

void StartServerDialog::setPythonExecutablePath(const QString& path)
{
  this->m_pythonExecutablePath = path;
  this->m_ui->pythonPathLineEdit->setText(path);
}
}
