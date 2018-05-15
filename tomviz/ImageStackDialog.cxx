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

#include "LoadStackReaction.h"

#include <QFileDialog>
#include <QFileInfo>

namespace tomviz {

ImageStackDialog::ImageStackDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::ImageStackDialog)
{
  m_ui->setupUi(this);
  m_ui->tableView->setModel(&m_tableModel);
  QObject::connect(this, &ImageStackDialog::summaryChanged, &m_tableModel,
                   &ImageStackModel::onFilesInfoChanged);

  QObject::connect(this, &ImageStackDialog::stackTypeChanged, &m_tableModel,
                   &ImageStackModel::onStackTypeChanged);

  QObject::connect(m_ui->openFile, &QPushButton::clicked, this,
                   &ImageStackDialog::onOpenFileClick);

  QObject::connect(m_ui->openFolder, &QPushButton::clicked, this,
                   &ImageStackDialog::onOpenFolderClick);

  QObject::connect(&m_tableModel, &ImageStackModel::toggledSelected, this,
                   &ImageStackDialog::onImageToggled);

  m_ui->loadedContainer->hide();
  m_ui->stackTypeCombo->setDisabled(true);
  m_ui->stackTypeCombo->insertItem(DataSource::DataSourceType::Volume,
                                   QString("Volume"));
  m_ui->stackTypeCombo->insertItem(DataSource::DataSourceType::TiltSeries,
                                   QString("Tilt Series"));

  // Due to an overloaded signal I am force to use static_cast here.
  QObject::connect(m_ui->stackTypeCombo, static_cast<void (QComboBox::*)(int)>(
                                           &QComboBox::currentIndexChanged),
                   this, &ImageStackDialog::onStackTypeChanged);

  this->setAcceptDrops(true);
}

ImageStackDialog::~ImageStackDialog() = default;

void ImageStackDialog::setStackSummary(const QList<ImageInfo>& summary)
{
  m_summary = summary;
  std::sort(m_summary.begin(), m_summary.end(),
            [](const ImageInfo& a, const ImageInfo& b) -> bool {
              // Place the inconsistent images at the top, so the user will
              // notice them.
              if (a.consistent == b.consistent) {
                return a.pos < b.pos;
              } else {
                return !a.consistent;
              }
            });
  emit summaryChanged(m_summary);
  m_ui->emptyContainer->hide();
  m_ui->loadedContainer->show();
  m_ui->stackTypeCombo->setEnabled(true);
  m_ui->tableView->resizeColumnsToContents();
  m_ui->tableView->horizontalHeader()->setSectionResizeMode(
    1, QHeaderView::Stretch);
  this->setAcceptDrops(false);
}

void ImageStackDialog::setStackType(const DataSource::DataSourceType& stackType)
{
  if (m_stackType != stackType) {
    m_stackType = stackType;
    emit stackTypeChanged(m_stackType);
    m_ui->stackTypeCombo->setCurrentIndex(m_stackType);
  }
}

void ImageStackDialog::onOpenFileClick()
{
  openFileDialog(QFileDialog::ExistingFiles);
}

void ImageStackDialog::onOpenFolderClick()
{
  openFileDialog(QFileDialog::Directory);
}

void ImageStackDialog::openFileDialog(int mode)
{
  QStringList filters;
  filters << "TIFF Image files (*.tiff *.tif)";

  QFileDialog dialog(nullptr);
  if (mode == QFileDialog::ExistingFiles) {
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilters(filters);
  } else if (mode == QFileDialog::Directory) {
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
  }

  if (dialog.exec()) {
    QStringList fileNames;
    if (mode == QFileDialog::ExistingFiles) {
      processFiles(dialog.selectedFiles());
    } else if (mode == QFileDialog::Directory) {
      processDirectory(dialog.selectedFiles()[0]);
    }
  }
}

void ImageStackDialog::processDirectory(QString path)
{
  QStringList fileNames;
  QDir directory(path);
  foreach (auto file, directory.entryList(QDir::Files)) {
    fileNames << directory.absolutePath() + QDir::separator() + file;
  }
  processFiles(fileNames);
}

void ImageStackDialog::processFiles(QStringList fileNames)
{
  QStringList fNames;
  foreach (auto file, fileNames) {
    if (file.endsWith(".tif") || file.endsWith(".tiff")) {
      fNames << file;
    }
  }
  QList<ImageInfo> summary = LoadStackReaction::loadTiffStack(fNames);

  bool isVolume = false;
  bool isTilt = false;
  bool isNumbered = false;
  DataSource::DataSourceType stackType = DataSource::DataSourceType::Volume;

  isVolume = detectVolume(fNames, summary);
  if (!isVolume) {
    isTilt = detectTilt(fNames, summary);
    if (isTilt) {
      stackType = DataSource::DataSourceType::TiltSeries;
    } else {
      isNumbered = detectVolume(fNames, summary, false);
      if (!isNumbered) {
        defaultOrder(fNames, summary);
      }
    }
  }
  this->setStackType(stackType);
  this->setStackSummary(summary);
}

bool ImageStackDialog::detectVolume(QStringList fileNames,
                                    QList<ImageInfo>& summary, bool matchPrefix)
{
  if (fileNames.size() < 1) {
    return false;
  }
  QString thePrefix;
  QString prefix;
  int num;
  QRegExp volRegExp("^(.*[^\\d]{1})(\\d+)(\\.(tif|tiff))$");

  for (int i = 0; i < fileNames.size(); ++i) {
    if (volRegExp.exactMatch(fileNames[i])) {
      prefix = volRegExp.cap(1);
      if (i == 0) {
        thePrefix = prefix;
      }
      if (prefix == thePrefix || !matchPrefix) {
        num = volRegExp.cap(2).toInt();
        summary[i].pos = num;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

bool ImageStackDialog::detectTilt(QStringList fileNames,
                                  QList<ImageInfo>& summary, bool matchPrefix)
{
  if (fileNames.size() < 1) {
    return false;
  }
  QString thePrefix;
  QString prefix;
  int num;
  QString sign;
  QString num_;
  QString ext;

  QRegExp tiltRegExp("^.*([p+]|[n-])?(\\d+)(\\.(tif|tiff))$");

  for (int i = 0; i < fileNames.size(); ++i) {
    if (tiltRegExp.exactMatch(fileNames[i])) {
      sign = tiltRegExp.cap(1);
      num_ = tiltRegExp.cap(2);
      ext = tiltRegExp.cap(3);
      prefix = fileNames[i];
      prefix.replace(sign + num_ + ext, QString());
      if (i == 0) {
        thePrefix = prefix;
      }
      if (prefix == thePrefix || !matchPrefix) {
        if (sign == "p") {
          sign = "+";
        } else if (sign == "n") {
          sign = "-";
        }
        num = (sign + num_).toInt();
        summary[i].pos = num;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

void ImageStackDialog::defaultOrder(QStringList fileNames,
                                    QList<ImageInfo>& summary)
{
  if (fileNames.size() != summary.size()) {
    return;
  }
  for (int i = 0; i < fileNames.size(); ++i) {
    summary[i].pos = i;
  }
}

void ImageStackDialog::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void ImageStackDialog::dropEvent(QDropEvent* event)
{
  if (event->mimeData()->hasUrls()) {
    QStringList pathList;
    QString path;
    QList<QUrl> urlList = event->mimeData()->urls();
    bool openDirs = true;
    for (int i = 0; i < urlList.size(); ++i) {
      path = urlList.at(i).toLocalFile();
      QFileInfo fileInfo(path);
      if (fileInfo.exists()) {
        if (fileInfo.isDir() && openDirs) {
          processDirectory(path);
          return;
        } else if (fileInfo.isFile()) {
          pathList.append(path);
        }
        // only open the first directory being dropped
        openDirs = false;
      }
    }
    processFiles(pathList);
  }
}

QList<ImageInfo> ImageStackDialog::getStackSummary() const
{
  return m_summary;
}

DataSource::DataSourceType ImageStackDialog::getStackType() const
{
  return m_stackType;
}

void ImageStackDialog::onImageToggled(int row, bool value)
{
  m_summary[row].selected = value;
  emit summaryChanged(m_summary);
}

void ImageStackDialog::onStackTypeChanged(int stackType)
{
  if (stackType == DataSource::DataSourceType::Volume) {
    setStackType(DataSource::DataSourceType::Volume);
  } else if (stackType == DataSource::DataSourceType::TiltSeries) {
    setStackType(DataSource::DataSourceType::TiltSeries);
  }
}

} // namespace tomviz
