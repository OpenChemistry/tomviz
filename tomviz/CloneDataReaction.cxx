/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CloneDataReaction.h"

#include "ActiveObjects.h"
#include "LoadDataReaction.h"
#include "MainWindow.h"
#include "Utilities.h"

#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineUtils.h"
#include "pipeline/PortData.h"
#include "pipeline/PortType.h"
#include "pipeline/PortUtils.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/LabelMapData.h"
#include "pipeline/data/VolumeData.h"

#include <vtkImageData.h>
#include <vtkNew.h>

#include <QApplication>
#include <QInputDialog>

namespace tomviz {

CloneDataReaction::CloneDataReaction(QAction* parentObject)
  : Reaction(parentObject)
{
}

DataSource* CloneDataReaction::clone(DataSource* toClone)
{
  // TODO: This still uses DataSource for the clone source. Once the pipeline
  // migration is complete, this should work entirely with SourceNode.
  // For now, we get the active SourceNode's VolumeData and deep-copy it.
  Q_UNUSED(toClone);

  // Get the VolumeData from the active pipeline (via MainWindow)
  auto* mainWindow = MainWindow::instance();
  if (!mainWindow) {
    return nullptr;
  }

  auto* pip = mainWindow->pipeline();
  if (!pip) {
    return nullptr;
  }

  // Clone the selected dataset: the source feeding whatever the user
  // picked. Only with no selection at all does the first source in the
  // pipeline stand in.
  auto& active = ActiveObjects::instance();
  auto* sourcePort = pipeline::feedingSourcePort(active.activeNode());
  if (!sourcePort) {
    // A selected port, or the tip left by the last selection
    sourcePort = pipeline::feedingSourcePort(active.activeTipOutputPort());
  }
  if (!sourcePort) {
    for (auto* node : pip->nodes()) {
      auto* src = qobject_cast<pipeline::SourceNode*>(node);
      if (src && !src->outputPorts().isEmpty()) {
        sourcePort = src->outputPorts().first();
        break;
      }
    }
  }

  if (!sourcePort) {
    return nullptr;
  }
  auto* activeSource = sourcePort->node();
  auto vol = pipeline::getOutputData<pipeline::VolumeDataPtr>(
    activeSource, sourcePort->name());
  if (!vol || !vol->imageData()) {
    return nullptr;
  }

  QStringList items;
  items << "Data only"
        << "Data with transformations";

  bool userOkayed;
  QString selection =
    QInputDialog::getItem(tomviz::mainWidget(), "Clone Data Options",
                          "Select what should be cloned", items,
                          /*current=*/0,
                          /*editable=*/false,
                          /*ok*/ &userOkayed);

  if (userOkayed) {
    // Deep-copy the vtkImageData
    vtkNew<vtkImageData> clonedImage;
    clonedImage->DeepCopy(vol->imageData());

    auto* newSource = new pipeline::SourceNode();
    newSource->setLabel(activeSource->label() + " (clone)");
    // Keep the port's type, so a label map stays one (with its label
    // table: names, colors, visibility). Only a generic ImageData port is
    // narrowed, as loading does, since it would disable every operator
    // declaring Volume or TiltSeries.
    auto dataType = sourcePort->type();
    if (dataType == pipeline::PortType::ImageData) {
      dataType = vol->hasTiltAngles() ? pipeline::PortType::TiltSeries
                                      : pipeline::PortType::Volume;
    }
    auto newVol = pipeline::makeVolumeData(clonedImage, dataType);
    if (auto labels = pipeline::labelMapData(newVol)) {
      if (auto original = pipeline::labelMapData(vol)) {
        labels->adoptLabelsFrom(*original);
      }
    }
    newVol->setLabel(newSource->label());
    newSource->addOutput("volume", dataType);
    newSource->setOutputData("volume", pipeline::PortData(newVol, dataType));

    LoadDataReaction::sourceNodeAdded(newSource);

    // TODO: operator cloning not yet supported in new pipeline
    return nullptr;
  }
  return nullptr;
}
} // namespace tomviz
