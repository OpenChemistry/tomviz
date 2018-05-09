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
#include "LoadStackReaction.h"

#include "DataSource.h"
// #include "ImageStackModel.h"
#include "ImageStackDialog.h"
#include "LoadDataReaction.h"
#include "Utilities.h"

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkTIFFReader.h>
// #include <iostream>

namespace tomviz {

LoadStackReaction::LoadStackReaction(QAction* parentObject)
  : pqReaction(parentObject)
{}

LoadStackReaction::~LoadStackReaction() = default;

void LoadStackReaction::onTriggered()
{
  loadData();
}

DataSource* LoadStackReaction::loadData(QStringList fileNames)
{
  QList<ImageInfo> summary = loadTiffStack(fileNames);
  // ImageStackModel imageStackModel(0);
  ImageStackDialog dialog(tomviz::mainWidget());
  dialog.setStackSummary(summary);
  int result = dialog.exec();
  if (result == QDialog::Accepted) {
    QStringList fNames;
    fNames = summaryToFileNames(dialog.stackSummary());
    if (fNames.size() < 1) {
      return nullptr;
    }
    return LoadDataReaction::loadData(fNames);
  } else {
    return nullptr;
  }
}

DataSource* LoadStackReaction::loadData()
{
  ImageStackDialog dialog(tomviz::mainWidget());
  int result = dialog.exec();
  if (result == QDialog::Accepted) {
    QStringList fNames;
    fNames = summaryToFileNames(dialog.stackSummary());
    if (fNames.size() < 1) {
      return nullptr;
    }
    return LoadDataReaction::loadData(fNames);
  } else {
    return nullptr;
  }
}

QStringList LoadStackReaction::summaryToFileNames(const QList<ImageInfo>& summary)
{
  QStringList fileNames;
  foreach(auto image, summary){
    if (image.selected){
      fileNames << image.fileInfo.absoluteFilePath();
    }
  }
  return fileNames;
}

QList<ImageInfo> LoadStackReaction::loadTiffStack(const QStringList& fileNames)
{
  QList<ImageInfo> summary;
  vtkNew<vtkTIFFReader> reader;
  int n = -1;
  int m = -1;
  int dims[3];
  int i = -1;
  bool consistent;
  foreach (QString file, fileNames) {
    i++;
    consistent = true;
    reader->SetFileName(file.toLatin1().data());
    reader->Update();
    reader->GetOutput()->GetDimensions(dims);
    if (n == -1 && m == -1) {
      n = dims[0];
      m = dims[1];
    } else {
      if (n != dims[0] || m != dims[1]) {
        consistent = false;
      }
    }
    summary.push_back(ImageInfo(file, dims[0], dims[1], consistent));
  }
  return summary;
}

}
