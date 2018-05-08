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

#include "ImageStackDialog.h"

#include "ui_ImageStackDialog.h"

#include "ImageStackModel.h"

namespace tomviz {

ImageStackDialog::ImageStackDialog(QWidget* parent, ImageStackModel* tableModel)
  : QDialog(parent), m_ui(new Ui::ImageStackDialog)
{
  m_ui->setupUi(this);
  m_ui->tableView->setModel(tableModel);
  m_ui->tableView->resizeColumnsToContents();
  m_ui->tableView->horizontalHeader()->setSectionResizeMode(
    1, QHeaderView::Stretch);
}

ImageStackDialog::~ImageStackDialog() = default;

} // namespace tomviz
