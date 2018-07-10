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

#include "ActiveObjects.h"
#include "ConvertToFloatOperator.h"
#include "EmdFormat.h"
#include "Module.h"
#include "Utilities.h"

#include <pqActiveObjects.h>
#include <pqProxyWidgetDialog.h>
#include <pqSettings.h>
#include <vtkArrayCalculator.h>
#include <vtkDataArray.h>
#include <vtkImageCast.h>
#include <vtkImageData.h>
#include <vtkImageMapToColors.h>
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMWriterFactory.h>
#include <vtkScalarsToColors.h>
#include <vtkSmartPointer.h>
#include <vtkTIFFWriter.h>
#include <vtkTrivialProducer.h>
#include <vtkUnsignedCharArray.h>

#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
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
    // Default to png if we apply the colormap, tiff if exporting raw data
    if (module->areScalarsMapped()) {
      filters << "PNG Files (*.png)"
              << "TIFF Files (*.tiff)";
    } else {
      filters << "TIFF Files (*.tiff)"
              << "PNG Files (*.png)";
    }
    filters << "JPEG Files (*.jpg *.jpeg)"
            << "VTK ImageData Files (*.vti)";
  } else if (exportType == "Molecule") {
    moleculeToFile(vtkMolecule::SafeDownCast(module->getDataToExport()));
    return;
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

namespace {

template <typename FromType, typename ToType>
void convert(vtkDataArray* outArray, int nComps, int nTuples, void* data)
{
  FromType* d = static_cast<FromType*>(data);
  ToType* a = static_cast<ToType*>(outArray->GetVoidPointer(0));
  for (int i = 0; i < nComps * nTuples; ++i) {
    a[i] = static_cast<ToType>(d[i]);
  }
}

template <typename FromType>
void convertToUnsignedChar(vtkDataArray* outArray, int nComps, int nTuples,
                           void* data)
{
  convert<FromType, unsigned char>(outArray, nComps, nTuples, data);
}
} // namespace

bool ExportDataReaction::exportColoredSlice(vtkImageData* imageData,
                                            vtkSMSourceProxy* proxy,
                                            const QString& filename)
{
  auto colorMap = m_module->colorMap();
  auto stc = vtkScalarsToColors::SafeDownCast(colorMap->GetClientSideObject());

  vtkNew<vtkImageMapToColors> imageSource;
  imageSource->SetLookupTable(stc);
  imageSource->SetInputData(imageData);

  vtkNew<vtkImageCast> castFilter;
  castFilter->SetOutputScalarTypeToUnsignedChar();
  castFilter->SetInputConnection(imageSource->GetOutputPort());
  castFilter->Update();

  auto writer = vtkImageWriter::SafeDownCast(proxy->GetClientSideObject());
  if (!writer) {
    return false;
  }
  writer->GetClassName();
  writer->SetFileName(filename.toLatin1().data());
  writer->SetInputConnection(castFilter->GetOutputPort());
  writer->Write();
  return true;
}

bool ExportDataReaction::exportData(const QString& filename)
{
  auto server = pqActiveObjects::instance().activeServer();

  auto data = m_module->getDataToExport();

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
  vtkSmartPointer<vtkTrivialProducer> trivialProducer =
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

  // Convert to a data format the file type supports
  const char* writerName = writer->GetClientSideObject()->GetClassName();
  auto imageData =
    vtkImageData::SafeDownCast(trivialProducer->GetOutputDataObject(0));
  QSettings* settings = pqApplicationCore::instance()->settings();
  if (imageData) {
    // If we are exporting a slice colored with the colormap to an image
    // file format, there is no need for type conversions or warning the user.
    if (m_module->areScalarsMapped() &&
        m_module->exportDataTypeString() == "Image") {
      bool res = exportColoredSlice(imageData, writer, filename);
      if (res) {
        return true;
      }
    }
    auto imageType = imageData->GetPointData()->GetScalars()->GetDataType();
    if (strcmp(writerName, "vtkTIFFWriter") == 0 && imageType == VTK_DOUBLE) {
      vtkNew<vtkImageData> fImage;
      fImage->DeepCopy(imageData);
      ConvertToFloatOperator convertFloat;
      convertFloat.applyTransform(fImage);

      trivialProducer->SetOutput(fImage.Get());
      trivialProducer->UpdateInformation();
      trivialProducer->Update();
      producer->UpdatePipeline();
    }

    if ((strcmp(writerName, "vtkPNGWriter") == 0 &&
         (imageType != VTK_UNSIGNED_CHAR || imageType != VTK_UNSIGNED_SHORT)) ||
        (strcmp(writerName, "vtkJPEGWriter") == 0 &&
         imageType != VTK_UNSIGNED_CHAR)) {

      // Warn the user about the conversion
      if (settings->value("tomviz/export/ShowFileTypeWarning", QVariant(true))
            .toBool()) {
        QMessageBox messageBox(QMessageBox::Warning, "tomviz",
                               "The requested file type does not support the "
                               "current data type, converting to unsigned "
                               "char.",
                               QMessageBox::Ok);
        QCheckBox* checkBox = new QCheckBox;
        checkBox->setText("Show this message again");
        checkBox->setChecked(true);
        connect(checkBox, &QCheckBox::stateChanged, [settings](int state) {
          settings->setValue("tomviz/export/ShowFileTypeWarning",
                             QVariant(state != 0));
        });
        messageBox.setCheckBox(checkBox);
        messageBox.exec();
      }

      vtkNew<vtkImageData> newImage;
      newImage->DeepCopy(imageData);
      vtkSmartPointer<vtkDataArray> scalars =
        imageData->GetPointData()->GetScalars();
      double range[2];
      scalars->GetRange(range);

      if ((imageType == VTK_FLOAT || imageType == VTK_DOUBLE) &&
          (range[0] >= 0 && range[1] <= 1)) {

        // Warn the user about the conversion
        if (settings
              ->value("tomviz/export/ShowNormalizedFloatWarning",
                      QVariant(true))
              .toBool()) {
          QMessageBox messageBox(QMessageBox::Warning, "tomviz",
                                 "Converting normalized floating point values "
                                 "to integers in the range 0-255.",
                                 QMessageBox::Ok);
          QCheckBox* checkBox = new QCheckBox;
          checkBox->setText("Show this message again");
          checkBox->setChecked(true);
          connect(checkBox, &QCheckBox::stateChanged, [settings](int state) {
            settings->setValue("tomviz/export/ShowNormalizedFloatWarning",
                               QVariant(state != 0));
          });
          messageBox.setCheckBox(checkBox);
          messageBox.exec();
        }

        vtkNew<vtkArrayCalculator> calc;
        calc->AddScalarVariable("scalars", scalars->GetName());
        calc->SetFunction("floor(scalars*255 + 0.5)");
        calc->SetResultArrayName("result");
        calc->SetInputData(imageData);
        calc->Update();
        scalars = calc->GetDataSetOutput()->GetPointData()->GetArray("result");
        scalars->GetRange(range);
      }
      vtkNew<vtkUnsignedCharArray> charArray;
      charArray->SetNumberOfComponents(scalars->GetNumberOfComponents());
      charArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
      charArray->SetName(scalars->GetName());
      switch (scalars->GetDataType()) {
        vtkTemplateMacro(convertToUnsignedChar<VTK_TT>(
          charArray.Get(), scalars->GetNumberOfComponents(),
          scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0)));
      }
      newImage->GetPointData()->RemoveArray(scalars->GetName());
      newImage->GetPointData()->SetScalars(charArray.Get());

      trivialProducer->SetOutput(newImage.Get());
      trivialProducer->UpdateInformation();
      trivialProducer->Update();
      producer->UpdatePipeline();
    }
  }

  pqProxyWidgetDialog dialog(writer, tomviz::mainWidget());
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
} // namespace tomviz
