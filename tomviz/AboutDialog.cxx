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

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"
#include "tomvizConfig.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <vtkPVConfig.h>

namespace tomviz {

AboutDialog::AboutDialog(MainWindow* mw)
  : QDialog(mw), m_ui(new Ui::AboutDialog)
{
  m_ui->setupUi(this);

  QString version(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0) {
    version.append("-").append(TOMVIZ_VERSION_EXTRA);
  }
  // versions += "<br />ParaView: " + QString(PARAVIEW_VERSION_FULL);

  auto string = m_ui->version->text().replace("#VERSION", version);
  m_ui->version->setText(string);
}

AboutDialog::~AboutDialog() = default;
}
