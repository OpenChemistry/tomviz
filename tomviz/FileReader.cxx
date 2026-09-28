/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "FileReader.h"

#include "ActiveObjects.h"
#include "DataExchangeFormat.h"
#include "EmdFormat.h"
#include "FileFormatManager.h"
#include "FxiFormat.h"
#include "GenericHDF5Format.h"
#include "LoadDataReaction.h"
#include "PythonReader.h"
#include "Utilities.h"
#include "vtkOMETiffReader.h"

#include <vtkImageData.h>
#include <vtkJPEGReader.h>
#include <vtkMRCReader.h>
#include <vtkNew.h>
#include <vtkPNGReader.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTIFFReader.h>
#include <vtkXMLImageDataReader.h>

#include <QFileInfo>

namespace tomviz {

namespace {

QString fileExtension(const QStringList& fileNames)
{
  if (fileNames.isEmpty()) {
    return {};
  }
  return QFileInfo(fileNames.first()).suffix().toLower();
}

/// Read .tif/.tiff/.png/.jpg/.jpeg/.vti/.mrc directly via VTK without
/// going through ParaView's file-load machinery — so no dialogs.
vtkSmartPointer<vtkImageData> readWithVTK(const QStringList& fileNames)
{
  if (fileNames.isEmpty()) {
    return nullptr;
  }
  QString ext = fileExtension(fileNames);
  std::string path = fileNames.first().toStdString();

  if (ext == "tif" || ext == "tiff") {
    auto reader = vtkSmartPointer<vtkTIFFReader>::New();
    if (fileNames.size() > 1) {
      auto sa = vtkSmartPointer<vtkStringArray>::New();
      sa->SetNumberOfValues(static_cast<vtkIdType>(fileNames.size()));
      for (int i = 0; i < fileNames.size(); ++i) {
        sa->SetValue(static_cast<vtkIdType>(i),
                     fileNames[i].toStdString());
      }
      reader->SetFileNames(sa);
    } else {
      reader->SetFileName(path.c_str());
    }
    reader->Update();
    return reader->GetOutput();
  }
  if (ext == "png") {
    auto reader = vtkSmartPointer<vtkPNGReader>::New();
    reader->SetFileName(path.c_str());
    reader->Update();
    return reader->GetOutput();
  }
  if (ext == "jpg" || ext == "jpeg") {
    auto reader = vtkSmartPointer<vtkJPEGReader>::New();
    reader->SetFileName(path.c_str());
    reader->Update();
    return reader->GetOutput();
  }
  if (ext == "vti") {
    auto reader = vtkSmartPointer<vtkXMLImageDataReader>::New();
    reader->SetFileName(path.c_str());
    reader->Update();
    return reader->GetOutput();
  }
  if (ext == "mrc") {
    auto reader = vtkSmartPointer<vtkMRCReader>::New();
    reader->SetFileName(path.c_str());
    reader->Update();
    return reader->GetOutput();
  }
  return nullptr;
}

QVariantMap hdf5OptionsFromJson(const QJsonObject& options)
{
  QVariantMap hdf5Options;
  if (options.contains("subsampleSettings")) {
    hdf5Options["subsampleStrides"] =
      options.value("subsampleSettings").toObject()["strides"].toVariant();
    hdf5Options["subsampleVolumeBounds"] =
      options.value("subsampleSettings")
        .toObject()["volumeBounds"]
        .toVariant();
    hdf5Options["askForSubsample"] = false;
  }
  return hdf5Options;
}

} // namespace

ReadResult readImageData(const QStringList& fileNames,
                         const QJsonObject& options)
{
  ReadResult result;
  if (fileNames.isEmpty()) {
    return result;
  }

  const QString fileName = fileNames.first();
  QFileInfo info(fileName);
  const QString suffix = info.suffix().toLower();
  const QString completeSuffix = info.completeSuffix().toLower();

  // .tvh5 — requires an internal node path.
  if (suffix == "tvh5") {
    QString path = options.value("tvh5NodePath").toString();
    if (path.isEmpty()) {
      return result;
    }
    QVariantMap emdOptions = { { "askForSubsample", false } };
    vtkNew<vtkImageData> image;
    if (EmdFormat::readNode(fileName.toStdString(), path.toStdString(),
                            image, emdOptions)) {
      result.imageData = image;
      result.tvh5NodePath = path;
    }
    return result;
  }

  // .emd — simple EMD reader.
  if (suffix == "emd") {
    QVariantMap emdOptions = hdf5OptionsFromJson(options);
    vtkNew<vtkImageData> imageData;
    if (EmdFormat::read(fileName.toLatin1().data(), imageData, emdOptions)) {
      result.imageData = imageData;
    }
    return result;
  }

  // .hspy / .h5 / .nxs — DataExchange / FXI / generic HDF5. NeXus
  // files are HDF5 underneath, so they go through the same readers.
  if (suffix == "hspy" || suffix == "h5" || suffix == "nxs") {
    QVariantMap hdf5Options = hdf5OptionsFromJson(options);
    const auto fnStd = fileName.toStdString();
    if (GenericHDF5Format::isDataExchange(fnStd)) {
      DataExchangeFormat format;
      auto r = format.readAll(fileName.toLatin1().data(), hdf5Options);
      if (r.imageData) {
        result.imageData = r.imageData;
        result.tiltAngles = r.tiltAngles;
        result.darkData = r.darkData;
        result.whiteData = r.whiteData;
      }
      return result;
    }
    if (GenericHDF5Format::isFxi(fnStd)) {
      FxiFormat format;
      auto r = format.readAll(fileName.toLatin1().data(), hdf5Options);
      if (r.imageData) {
        result.imageData = r.imageData;
        result.tiltAngles = r.tiltAngles;
        result.darkData = r.darkData;
        result.whiteData = r.whiteData;
      }
      return result;
    }
    vtkNew<vtkImageData> imageData;
    GenericHDF5Format hdf5Format;
    if (hdf5Format.read(fileName.toLatin1().data(), imageData, hdf5Options)) {
      result.imageData = imageData;
    }
    return result;
  }

  // *.ome.tif[f] — multi-channel OME-TIFF.
  if (completeSuffix.endsWith("ome.tif") ||
      completeSuffix.endsWith("ome.tiff")) {
    vtkNew<vtkOMETiffReader> reader;
    reader->SetFileName(fileName.toLocal8Bit().constData());
    reader->Update();
    result.imageData = reader->GetOutput();
    QJsonObject rprops;
    rprops["name"] = "OMETIFFReader";
    result.readerProperties = rprops;
    return result;
  }

  // Simple VTK-readable formats — direct, no dialog.
  if (suffix == "tif" || suffix == "tiff" || suffix == "png" ||
      suffix == "jpg" || suffix == "jpeg" || suffix == "vti" ||
      suffix == "mrc") {
    result.imageData = readWithVTK(fileNames);
    return result;
  }

  // Python-registered reader by extension.
  if (FileFormatManager::instance().pythonReaderFactory(suffix) != nullptr) {
    auto factory = FileFormatManager::instance().pythonReaderFactory(suffix);
    auto reader = factory->createReader();
    auto imageData = reader.read(fileNames[0]);
    if (imageData) {
      result.imageData = imageData;
    }
    return result;
  }

  // ParaView reader — requires an explicit descriptor from the caller
  // (no dialog here). The descriptor carries the reader's XML name and
  // property values, as emitted by LoadDataReaction::readerProperties().
  if (options.contains("reader")) {
    auto props = options.value("reader").toObject();
    auto name = props.value("name").toString();
    auto* pxm = ActiveObjects::instance().proxyManager();
    if (!pxm || name.isEmpty()) {
      return result;
    }
    vtkSmartPointer<vtkSMProxy> reader;
    reader.TakeReference(pxm->NewProxy("sources", name.toLatin1().data()));
    if (!reader) {
      return result;
    }
    tomviz::setProperties(props, reader);
    LoadDataReaction::setFileNameProperties(props, reader);
    reader->UpdateVTKObjects();
    if (auto* src = vtkSMSourceProxy::SafeDownCast(reader)) {
      src->UpdatePipelineInformation();
      src->UpdatePipeline();
      auto* algo = vtkAlgorithm::SafeDownCast(src->GetClientSideObject());
      if (algo) {
        auto* image =
          vtkImageData::SafeDownCast(algo->GetOutputDataObject(0));
        if (image) {
          result.imageData = image;
          result.readerProperties = props;
        }
      }
    }
    return result;
  }

  // No headless path — caller must fall back to interactive load.
  return result;
}

} // namespace tomviz
