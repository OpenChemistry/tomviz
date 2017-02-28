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
#include "SaveDataReaction.h"

#include "EmdFormat.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"
#include "pqPipelineSource.h"
#include "pqProxyWidgetDialog.h"
#include "pqSaveDataReaction.h"
#include "vtkDataArray.h"
#include "vtkDataObject.h"
#include "vtkImageData.h"
#include "vtkPointData.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMWriterFactory.h"
#include "vtkTrivialProducer.h"

#include <cassert>

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringList>

namespace tomviz {

SaveDataReaction::SaveDataReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

SaveDataReaction::~SaveDataReaction()
{
}

void SaveDataReaction::updateEnableState()
{
  parentAction()->setEnabled(ActiveObjects::instance().activeDataSource() !=
                             nullptr);
}

void SaveDataReaction::onTriggered()
{
  QStringList filters;
  filters << "TIFF format (*.tiff)"
          << "EMD format (*.emd *.hdf5)"
          << "CSV File (*.csv)"
          << "Exodus II File (*.e *.ex2 *.ex2v2 *.exo *.exoII *.exoii *.g)"
          << "Legacy VTK Files (*.vtk)"
          << "Meta Image Files (*.mhd)"
          << "ParaView Data Files (*.pvd)"
          << "VTK ImageData Fiels (*.vti)"
          << "XDMF Data File (*.xmf)"
          << "JSON Image Files (*.json)";

  QFileDialog dialog(nullptr);
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setNameFilters(filters);
  dialog.setObjectName("FileOpenDialog-tomviz"); // avoid name collision?
  dialog.setAcceptMode(QFileDialog::AcceptSave);

  if (dialog.exec() == QDialog::Accepted) {
    QStringList filenames = dialog.selectedFiles();
    QString format = dialog.selectedNameFilter();
    QString filename = filenames[0];
    int startPos = format.indexOf("(") + 1;
    int n = format.indexOf(")") - startPos;
    QString extensionString = format.mid(startPos, n);
    QStringList extensions = extensionString.split(QRegularExpression(" ?\\*"),
                                                   QString::SkipEmptyParts);
    bool hasExtension = false;
    for (QString& str : extensions) {
      if (filename.endsWith(str)) {
        hasExtension = true;
      }
    }
    if (!hasExtension) {
      filename = QString("%1%2").arg(filename, extensions[0]);
    }
    saveData(filename);
  }
}

bool SaveDataReaction::saveData(const QString& filename)
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  DataSource* source = ActiveObjects::instance().activeDataSource();
  if (!server || !source) {
    qCritical("No active source located.");
    return false;
  }

  QFileInfo info(filename);
  if (info.suffix() == "emd") {
    EmdFormat writer;
    if (!writer.write(filename.toLatin1().data(), source)) {
      qCritical() << "Failed to write out data.";
      return false;
    } else {
      return true;
    }
  }

  vtkSMWriterFactory* writerFactory =
    vtkSMProxyManager::GetProxyManager()->GetWriterFactory();
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(writerFactory->CreateWriter(filename.toLatin1().data(),
                                                  source->producer()));
  vtkSMSourceProxy* writer = vtkSMSourceProxy::SafeDownCast(proxy);
  if (!writer) {
    qCritical() << "Failed to create writer for: " << filename;
    return false;
  }
  if (strcmp(writer->GetClientSideObject()->GetClassName(), "vtkTIFFWriter") ==
      0) {
    vtkTrivialProducer* t = vtkTrivialProducer::SafeDownCast(
      source->producer()->GetClientSideObject());
    vtkImageData* imageData =
      vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData->GetPointData()->GetScalars()->GetDataType() == VTK_DOUBLE) {
      QMessageBox messageBox;
      messageBox.setWindowTitle("Unsupported data type");
      messageBox.setText(
        "Tiff files do not support writing data of type double.");
      messageBox.setIcon(QMessageBox::Critical);
      messageBox.exec();
      return false;
    }
  }

  pqProxyWidgetDialog dialog(writer, pqCoreUtilities::mainWidget());
  dialog.setObjectName("WriterSettingsDialog");
  dialog.setEnableSearchBar(true);
  dialog.setWindowTitle(
    QString("Configure Writer (%1)").arg(writer->GetXMLLabel()));

  // Check to see if this writer has any properties that can be configured by
  // the user. If it does, display the dialog.
  if (dialog.hasVisibleWidgets()) {
    dialog.exec();
    if (dialog.result() == QDialog::Rejected) {
      // The user pressed Cancel so don't write
      return false;
    }
  }
  writer->UpdateVTKObjects();
  writer->UpdatePipeline();
  return true;
}

} // end of namespace tomviz
