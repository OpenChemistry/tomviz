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

#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>

#include <algorithm>
#include <chrono>
#include <thread>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkTIFFReader.h>

extern "C" {
#include "vtk_tiff.h"
}

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

  QObject::connect(m_ui->checkSizes, &QPushButton::clicked, this,
                   &ImageStackDialog::onCheckSizesClick);

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

  setAcceptDrops(true);
}

ImageStackDialog::~ImageStackDialog() = default;

void ImageStackDialog::setStackSummary(const QList<ImageInfo>& summary,
                                       bool sort)
{
  m_summary = summary;
  if (sort) {
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
  }
  emit summaryChanged(m_summary);
  m_ui->emptyContainer->hide();
  m_ui->loadedContainer->show();
  m_ui->stackTypeCombo->setEnabled(true);
  m_ui->tableView->resizeColumnsToContents();
  m_ui->tableView->horizontalHeader()->setSectionResizeMode(
    1, QHeaderView::Stretch);
  setAcceptDrops(false);
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

void ImageStackDialog::onCheckSizesClick()
{
  checkStackSizes(m_summary);
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

void ImageStackDialog::processDirectory(const QString& path)
{
  QStringList fileNames;
  QDir directory(path);
  foreach (auto file, directory.entryList(QDir::Files)) {
    fileNames << directory.absolutePath() + QDir::separator() + file;
  }
  processFiles(fileNames);
}

void ImageStackDialog::processFiles(const QStringList& fileNames)
{
  QStringList fNames;
  foreach (auto file, fileNames) {
    if (file.endsWith(".tif") || file.endsWith(".tiff")) {
      fNames << file;
    }
  }
  QList<ImageInfo> summary = initStackSummary(fNames);

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
  setStackType(stackType);
  setStackSummary(summary, false);

  // Checking image size can take several seconds if there are thousands of
  // images in the stack.
  // Check the sizes automatically only for stacks smaller than maxImages
  const auto maxImages = 1000;
  if (summary.size() <= maxImages) {
    checkStackSizes(summary);
  }
}

void ImageStackDialog::checkStackSizes(QList<ImageInfo>& summary)
{
  m_ui->checkSizes->hide();
  QStringList fileNames;
  for (auto i = 0; i < summary.size(); ++i) {
    fileNames << summary[i].fileInfo.absoluteFilePath();
  }

  const unsigned int minCores = 1;
  const unsigned int nCores =
    std::max(std::thread::hardware_concurrency(), minCores);
  const unsigned int maxThreads = 4;
  const unsigned int nThreads = std::min(nCores, maxThreads);
  std::vector<std::thread> threads(nThreads);

  for (unsigned int i = 0; i < nThreads; ++i) {
    threads[i] = std::thread(getImageSize, fileNames, i, nThreads, &summary);
  }

  for (unsigned int i = 0; i < nThreads; ++i) {
    threads[i].join();
  }

  // check consistency
  if (summary.size() > 0) {
    const auto m = summary[0].m;
    const auto n = summary[1].n;
    for (auto i = 0; i < summary.size(); ++i) {
      if (summary[i].m == m && summary[i].n == n) {
        summary[i].consistent = true;
        summary[i].selected = true;
      } else {
        summary[i].consistent = false;
        summary[i].selected = false;
      }
    }
  }
  setStackSummary(summary, true);
}

void ImageStackDialog::getImageSize(QStringList fileNames, int iThread,
                                    int nThreads, QList<ImageInfo>* summary)
{
  TIFFSetWarningHandler(nullptr);
  for (auto i = iThread; i < fileNames.size(); i += nThreads) {
    uint32_t w = -1;
    uint32_t h = -1;
    TIFF* tif = TIFFOpen(fileNames[i].toLatin1().data(), "r");
    if (tif) {
      TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
      TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
      TIFFClose(tif);
    }
    (*summary)[i].m = w;
    (*summary)[i].n = h;
  }
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
  QString numStr;
  QString ext;

  QRegExp tiltRegExp("^.*([p+]|[n-])?(\\d+)(\\.(tif|tiff))$");

  for (int i = 0; i < fileNames.size(); ++i) {
    if (tiltRegExp.exactMatch(fileNames[i])) {
      sign = tiltRegExp.cap(1);
      numStr = tiltRegExp.cap(2);
      ext = tiltRegExp.cap(3);
      prefix = fileNames[i];
      prefix.replace(sign + numStr + ext, QString());
      if (i == 0) {
        thePrefix = prefix;
      }
      if (prefix == thePrefix || !matchPrefix) {
        if (sign == "p") {
          sign = "+";
        } else if (sign == "n") {
          sign = "-";
        }
        num = (sign + numStr).toInt();
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

QList<ImageInfo> ImageStackDialog::initStackSummary(
  const QStringList& fileNames)
{
  QList<ImageInfo> summary;
  int n = -1;
  int m = -1;
  bool consistent = true;
  foreach (QString file, fileNames) {
    summary.push_back(ImageInfo(file, 0, m, n, consistent));
  }
  return summary;
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
