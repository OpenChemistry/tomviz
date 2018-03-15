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

namespace tomviz {

StartServerDialog::StartServerDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::StartServerDialog)
{
  m_ui->setupUi(this);

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
  this->setPythonExecutablePath(
    settings->value("pythonExecutablePath").toString());
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
  QFontMetrics metrics(this->m_ui->pythonExecutablPathLabel->font());
  QString elidedText = metrics.elidedText(
    path, Qt::ElideRight, this->m_ui->pythonExecutablPathLabel->width());
  this->m_ui->pythonExecutablPathLabel->setText(elidedText);
}
}
