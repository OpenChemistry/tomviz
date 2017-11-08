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
#include "MergeImagesDialog.h"
#include "ui_MergeImagesDialog.h"

namespace tomviz {

MergeImagesDialog::MergeImagesDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::MergeImagesDialog)
{
  m_ui->setupUi(this);
  m_ui->MergeImageArraysRadioButton->setChecked(true);
  m_ui->MergeImageArraysRadioButton->setChecked(false);
  m_ui->MergeArrayComponentsWidget->hide();
}

MergeImagesDialog::~MergeImagesDialog()
{
}

MergeImagesDialog::MergeMode MergeImagesDialog::getMode()
{
  return (m_ui->MergeImageArraysRadioButton->isChecked() ? Arrays : Components);
}

}