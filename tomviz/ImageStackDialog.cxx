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
#include "LoadStackReaction.h"
#include "LoadDataReaction.h"

#include <iostream>
#include <QFileDialog>

namespace tomviz {

ImageStackDialog::ImageStackDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::ImageStackDialog)
{
  m_ui->setupUi(this);
  // m_summary = nullptr;
  ImageStackModel* tableModel = new ImageStackModel(nullptr);
  m_ui->tableView->setModel(tableModel);
  // m_ui->tableView->resizeColumnsToContents();
  m_ui->tableView->horizontalHeader()->setSectionResizeMode(
    1, QHeaderView::Stretch);
  QObject::connect(this, &ImageStackDialog::summaryChanged,
                   tableModel, &ImageStackModel::onFilesInfoChanged);

  QObject::connect(m_ui->openFile, &QPushButton::clicked,
                   this, &ImageStackDialog::onOpenFileClick);

  QObject::connect(m_ui->openFolder, &QPushButton::clicked,
                   this, &ImageStackDialog::onOpenFolderClick);

  QObject::connect(tableModel, &ImageStackModel::toggledSelected,
                   this, &ImageStackDialog::onImageToggled);

  m_ui->loadedContainer->hide();
  m_ui->stackType->setDisabled(true);
}

ImageStackDialog::~ImageStackDialog() = default;

void ImageStackDialog::setStackSummary(const QList<ImageInfo>& summary)
{
  std::cout << "Summary Changed: DIALOG" << std::endl;
  m_summary = summary;
  emit summaryChanged(m_summary);
  m_ui->emptyContainer->hide();
  m_ui->loadedContainer->show();
  m_ui->stackType->setEnabled(true);

}

void ImageStackDialog::onOpenFileClick()
{
  std::cout << "Open file clicked" << std::endl;
  openFileDialog(QString());
}

void ImageStackDialog::onOpenFolderClick()
{
  std::cout << "Open folder clicked" << std::endl;
}

void ImageStackDialog::openFileDialog(QString mode)
{
  QStringList filters;
  filters << "TIFF Image files (*.tiff *.tif)";

  QFileDialog dialog(nullptr);
  dialog.setFileMode(QFileDialog::ExistingFiles);
  dialog.setNameFilters(filters);

  if (dialog.exec()) {
    QStringList fileNames = dialog.selectedFiles();
    QList<ImageInfo> summary = LoadStackReaction::loadTiffStack(fileNames);
    this->setStackSummary(summary);
  }
}

QList<ImageInfo> ImageStackDialog::stackSummary() const
{
  return m_summary;
}

void ImageStackDialog::onImageToggled(int row, bool value)
{
  m_summary[row].selected = value;
  emit summaryChanged(m_summary);
}

} // namespace tomviz
