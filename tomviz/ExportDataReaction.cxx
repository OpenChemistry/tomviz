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

#include "ExportDataReaction.h"

#include <pqActiveObjects.h>
#include <pqCoreUtilities.h>
#include <pqProxyWidgetDialog.h>
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMWriterFactory.h>
#include <vtkSmartPointer.h>
#include <vtkTIFFWriter.h>
#include <vtkTrivialProducer.h>

#include "ActiveObjects.h"
#include "ConvertToFloatOperator.h"
#include "EmdFormat.h"
#include "Module.h"
#include "Utilities.h"

#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>

namespace tomviz {

ExportDataReaction::ExportDataReaction(QAction* parentAction, Module* module)
  : pqReaction(parentAction), m_module(module)
{
  connect(&ActiveObjects::instance(), SIGNAL(moduleChanged(Module*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

void ExportDataReaction::updateEnableState()
{
  if (!m_module) {
    parentAction()->setEnabled(ActiveObjects::instance().activeModule() !=
                               nullptr);
  } else {
    parentAction()->setEnabled(true);
  }
}

void ExportDataReaction::onTriggered()
{
  Module* module = m_module;
  if (!module) {
    module = ActiveObjects::instance().activeModule();
  }
  if (!module) {
    return;
  }
  QString exportType = module->exportDataTypeString();
  QStringList filters;
  if (exportType == "Volume") {
    filters << "TIFF format (*.tiff)"
            << "EMD format (*.emd *.hdf5)"
            << "CSV File (*.csv)"
            << "Exodus II File (*.e *.ex2 *.ex2v2 *.exo *.exoII *.exoii *.g)"
            << "Legacy VTK Files (*.vtk)"
            << "Meta Image Files (*.mhd)"
            << "ParaView Data Files (*.pvd)"
            << "VTK ImageData Files (*.vti)"
            << "XDMF Data File (*.xmf)"
            << "JSON Image Files (*.json)";
  } else if (exportType == "Mesh") {
    filters << "STL Files (*.stl)"
            << "VTK PolyData files(*.vtp)";
  } else if (exportType == "Image") {
    filters << "PNG Files (*.png)"
            << "JPEG Files (*.jpg *.jpeg)"
            << "TIFF Files (*.tiff)"
            << "VTK ImageData Files (*.vti)";
  }

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
    exportData(filename);
  }
}

bool ExportDataReaction::exportData(const QString& filename)
{
  auto server = pqActiveObjects::instance().activeServer();

  vtkSmartPointer<vtkDataObject> data = this->m_module->getDataToExport();

  if (!server) {
    qCritical("No active server located.");
    return false;
  }

  QFileInfo info(filename);
  if (info.suffix() == "emd") {
    EmdFormat writer;
    auto image = vtkImageData::SafeDownCast(data);
    if (!image || !writer.write(filename.toLatin1().data(), image)) {
      qCritical() << "Failed to write out data.";
      return false;
    } else {
      return true;
    }
  }

  auto writerFactory = vtkSMProxyManager::GetProxyManager()->GetWriterFactory();

  vtkSMSessionProxyManager* pxm =
    vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();

  vtkSmartPointer<vtkSMSourceProxy> producer;
  producer.TakeReference(vtkSMSourceProxy::SafeDownCast(
    pxm->NewProxy("sources", "TrivialProducer")));
  auto trivialProducer =
    vtkTrivialProducer::SafeDownCast(producer->GetClientSideObject());
  trivialProducer->SetOutput(data);
  trivialProducer->UpdateInformation();
  trivialProducer->Update();
  producer->UpdatePipeline();

  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(
    writerFactory->CreateWriter(filename.toLatin1().data(), producer));
  auto writer = vtkSMSourceProxy::SafeDownCast(proxy);
  if (!writer) {
    qCritical() << "Failed to create writer for: " << filename;
    return false;
  }

  // Convert to float if the type is found to be a double.
  if (strcmp(writer->GetClientSideObject()->GetClassName(), "vtkTIFFWriter") ==
      0) {
    auto t = vtkTrivialProducer::SafeDownCast(producer->GetClientSideObject());
    auto imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData->GetPointData()->GetScalars()->GetDataType() == VTK_DOUBLE) {
      vtkNew<vtkImageData> fImage;
      fImage->DeepCopy(imageData);
      ConvertToFloatOperator convertFloat;
      convertFloat.applyTransform(fImage);

      vtkNew<vtkTIFFWriter> tiff;
      tiff->SetInputData(fImage);
      tiff->SetFileName(filename.toLatin1().data());
      tiff->Write();

      return true;
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
}
