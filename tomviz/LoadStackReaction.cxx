/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LoadStackReaction.h"

#include "ImageStackDialog.h"
#include "LoadDataReaction.h"
#include "Utilities.h"

#include "pipeline/Pipeline.h"
#include "pipeline/PortType.h"
#include "pipeline/PortUtils.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/transforms/SetTiltAnglesTransform.h"

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkTIFFReader.h>

namespace tomviz {

LoadStackReaction::LoadStackReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
}

LoadStackReaction::~LoadStackReaction() = default;

void LoadStackReaction::onTriggered()
{
  loadData();
}

pipeline::SourceNode* LoadStackReaction::loadData(
  const QStringList& fileNames)
{
  ImageStackDialog dialog(tomviz::mainWidget());
  dialog.processFiles(fileNames);
  return execStackDialog(dialog);
}

pipeline::SourceNode* LoadStackReaction::loadData(const QString& directory)
{
  ImageStackDialog dialog(tomviz::mainWidget());
  dialog.processDirectory(directory);
  return execStackDialog(dialog);
}

pipeline::SourceNode* LoadStackReaction::loadData()
{
  ImageStackDialog dialog(tomviz::mainWidget());
  return execStackDialog(dialog);
}

pipeline::SourceNode* LoadStackReaction::execStackDialog(
  ImageStackDialog& dialog)
{
  int result = dialog.exec();
  if (result == QDialog::Accepted) {
    QStringList fNames;
    QList<ImageInfo> summary = dialog.getStackSummary();
    fNames = summaryToFileNames(summary);
    if (fNames.size() < 1) {
      return nullptr;
    }
    auto* source = LoadDataReaction::loadData(fNames);
    if (!source) {
      return nullptr;
    }

    pipeline::PortType stackType = dialog.getStackType();
    if (stackType == pipeline::PortType::TiltSeries) {
      // Build tilt angles from the stack summary
      QMap<size_t, double> angles;
      int j = 0;
      for (int i = 0; i < summary.size(); ++i) {
        if (summary[i].selected) {
          angles[j++] = summary[i].pos;
        }
      }

      // Set tilt angles directly on the VolumeData
      auto vol =
        pipeline::getOutputData<pipeline::VolumeDataPtr>(source);
      if (vol) {
        QVector<double> tiltAngles(angles.size(), 0.0);
        for (auto it = angles.constBegin(); it != angles.constEnd();
             ++it) {
          if (static_cast<int>(it.key()) < tiltAngles.size()) {
            tiltAngles[static_cast<int>(it.key())] = it.value();
          }
        }
        vol->setTiltAngles(tiltAngles);
        source->setProperty("dataType", "tiltSeries");
      }
    }

    return source;
  } else {
    return nullptr;
  }
}

QStringList LoadStackReaction::summaryToFileNames(
  const QList<ImageInfo>& summary)
{
  QStringList fileNames;
  foreach (auto image, summary) {
    if (image.selected) {
      fileNames << image.fileInfo.absoluteFilePath();
    }
  }
  return fileNames;
}

QList<ImageInfo> LoadStackReaction::loadTiffStack(
  const QStringList& fileNames)
{
  QList<ImageInfo> summary;
  vtkNew<vtkTIFFReader> reader;
  int n = -1;
  int m = -1;
  int dims[3];
  bool consistent;
  foreach (QString file, fileNames) {
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
    summary.push_back(ImageInfo(file, 0, dims[0], dims[1], consistent));
  }
  return summary;
}
}
