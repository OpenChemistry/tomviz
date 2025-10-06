/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ConformVolumeReaction.h"

#include "vtkImageChangeInformation.h"
#include "vtkImageData.h"
#include "vtkImageResize.h"
#include "vtkNew.h"

#include "ConformVolumeDialog.h"
#include "DataSource.h"
#include "LoadDataReaction.h"

namespace tomviz {

ConformVolumeReaction::ConformVolumeReaction(QAction* parentObject)
  : pqReaction(parentObject)
{
  updateEnableState();
}

void ConformVolumeReaction::onTriggered()
{
  // Check which volume should be the conforming one
  ConformVolumeDialog dialog;
  dialog.setVolumes(m_dataSources.values());

  if (dialog.exec() == QDialog::Rejected) {
    return;
  }

  m_conformingVolume = dialog.selectedVolume();

  auto newSource = createConformedVolume();
  if (newSource) {
    LoadDataReaction::dataSourceAdded(newSource);
  }
}

void ConformVolumeReaction::updateDataSources(QSet<DataSource*> sources)
{
  m_dataSources = sources;
  updateEnableState();
}

void ConformVolumeReaction::updateEnableState()
{
  updateVisibleState();
}

void ConformVolumeReaction::updateVisibleState()
{
  parentAction()->setVisible(false);

  if (m_dataSources.size() != 2) {
    return;
  }

  QList<DataSource*> sourceList = m_dataSources.values();
  // Check that both DataSources are volumes
  for (auto ds : m_dataSources) {
    if (ds->type() != DataSource::Volume) {
      return;
    }
  }

  // Make sure the names are not the same...
  if (sourceList[0]->label() == sourceList[1]->label()) {
    return;
  }

  parentAction()->setVisible(true);
}

DataSource* ConformVolumeReaction::createConformedVolume()
{
  if (m_dataSources.size() != 2 ||
      !m_dataSources.contains(m_conformingVolume)) {
    return nullptr;
  }

  auto* conformingVolume = m_conformingVolume;

  DataSource* conformToVolume = nullptr;
  for (auto it = m_dataSources.cbegin(); it != m_dataSources.cend(); ++it) {
    if (*it != conformingVolume) {
      conformToVolume = *it;
      break;
    }
  }

  if (!conformToVolume) {
    return nullptr;
  }

  // Begin the volume conforming
  vtkNew<vtkImageResize> resize;
  resize->SetInputData(conformingVolume->imageData());
  resize->SetOutputDimensions(conformToVolume->imageData()->GetDimensions());
  resize->Update();

  vtkNew<vtkImageChangeInformation> changeInfo;
  changeInfo->SetInputData(resize->GetOutputDataObject(0));
  changeInfo->SetInformationInputData(conformToVolume->imageData());
  changeInfo->Update();

  auto* output = vtkImageData::SafeDownCast(changeInfo->GetOutputDataObject(0));
  if (!output) {
    return nullptr;
  }

  auto* newSource = new DataSource(output);
  newSource->setFileName("Conformed Volume");
  // Make the display position match as well
  newSource->setDisplayPosition(conformToVolume->displayPosition());
  return newSource;
}

} // namespace tomviz
