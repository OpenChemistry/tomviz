/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PortDataWriter.h"

#include "EmdFormat.h"
#include "FileFormatManager.h"
#include "PythonWriter.h"
#include "Utilities.h"

#include "pipeline/PortData.h"
#include "pipeline/data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkImageCast.h>
#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMWriterFactory.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkTrivialProducer.h>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace tomviz {

using pipeline::PortData;
using pipeline::PortType;
using pipeline::VolumeDataPtr;

namespace {

bool isEmdExtension(const QString& ext)
{
  return ext == "emd" || ext == "hdf5" || ext == "h5";
}

/// Reduce @a image to the named point-data arrays, keeping geometry and
/// field data (tilt angles, scan ids, ...) intact. Returns @a image
/// untouched when the selection already covers everything it carries.
vtkSmartPointer<vtkImageData> filterArrays(vtkImageData* image,
                                           const QStringList& arrayNames)
{
  auto* pointData = image->GetPointData();
  int total = pointData->GetNumberOfArrays();
  if (arrayNames.isEmpty() || arrayNames.size() >= total) {
    return image;
  }

  QList<vtkDataArray*> selected;
  for (const auto& name : arrayNames) {
    if (auto* array = pointData->GetArray(name.toUtf8().data())) {
      selected.append(array);
    }
  }
  if (selected.isEmpty()) {
    return image;
  }

  vtkNew<vtkImageData> filtered;
  filtered->CopyStructure(image);
  filtered->GetFieldData()->ShallowCopy(image->GetFieldData());
  for (auto* array : selected) {
    filtered->GetPointData()->AddArray(array);
  }

  // Keep the original active array active when it survived the filter,
  // so writers that only look at the scalars pick the expected one.
  auto* scalars = pointData->GetScalars();
  const char* activeName =
    scalars && selected.contains(scalars) ? scalars->GetName() : nullptr;
  filtered->GetPointData()->SetActiveScalars(
    activeName ? activeName : selected.first()->GetName());

  return filtered;
}

bool writeWithProxyWriter(vtkImageData* image, const QString& path)
{
  auto* pxm =
    vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
  if (!pxm) {
    qCritical() << "No active session proxy manager, cannot write" << path;
    return false;
  }

  vtkSmartPointer<vtkSMSourceProxy> producerProxy;
  producerProxy.TakeReference(vtkSMSourceProxy::SafeDownCast(
    pxm->NewProxy("sources", "TrivialProducer")));

  auto* tp =
    vtkTrivialProducer::SafeDownCast(producerProxy->GetClientSideObject());
  tp->SetOutput(image);
  producerProxy->UpdateVTKObjects();
  producerProxy->UpdatePipeline();

  auto* writerFactory =
    vtkSMProxyManager::GetProxyManager()->GetWriterFactory();
  vtkSmartPointer<vtkSMProxy> writerProxy;
  writerProxy.TakeReference(
    writerFactory->CreateWriter(path.toUtf8().data(), producerProxy, 0));

  if (!writerProxy) {
    qCritical() << "No suitable writer found for:" << path;
    return false;
  }

  writerProxy->UpdateVTKObjects();
  vtkSMSourceProxy::SafeDownCast(writerProxy)->UpdatePipeline();

  return true;
}

bool writeVolume(const VolumeDataPtr& volume, const QStringList& arrayNames,
                 const QString& path)
{
  if (!volume || !volume->isValid()) {
    qCritical() << "Invalid volume data, cannot write" << path;
    return false;
  }

  auto image = filterArrays(volume->imageData(), arrayNames);
  QString ext = QFileInfo(path).suffix().toLower();

  if (isEmdExtension(ext)) {
    return EmdFormat::write(path.toStdString(), image,
                            volume->units().toStdString());
  }

  if (auto* factory = FileFormatManager::instance().pythonWriterFactory(ext)) {
    auto writer = factory->createWriter();
    return writer.write(path, image);
  }

  if (ext == "tiff" || ext == "tif") {
    auto* scalars = image->GetPointData()->GetScalars();
    if (scalars && scalars->GetDataType() == VTK_DOUBLE) {
      vtkNew<vtkImageCast> cast;
      cast->SetInputData(image);
      cast->SetOutputScalarTypeToFloat();
      cast->Update();
      image = cast->GetOutput();
    }
  }

  return writeWithProxyWriter(image, path);
}

bool writeTextFile(const QByteArray& contents, const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qCritical() << "Error opening file for writing:" << path;
    return false;
  }
  bool ok = file.write(contents) == contents.size();
  file.close();
  return ok;
}

bool writeTable(vtkTable* table, const QString& path)
{
  if (!table) {
    qCritical() << "Invalid table data, cannot write" << path;
    return false;
  }

  QString ext = QFileInfo(path).suffix().toLower();
  if (ext == "json") {
    return writeTextFile(tableToJson(table).toJson(), path);
  }

  return writeTextFile(tableToCsv(table).toUtf8(), path);
}

} // namespace

namespace PortDataWriter {

PortType formatGroup(PortType type)
{
  if (pipeline::isVolumeType(type)) {
    return PortType::ImageData;
  }
  return type;
}

QList<PortFormat> formats(PortType type)
{
  switch (formatGroup(type)) {
    case PortType::ImageData: {
      // EMD stores the extra arrays under /tomviz_scalars; the VTK
      // writers serialize every point-data array. The rest keep only
      // the active scalars, so they are flagged single-array and the
      // caller splits multi-array volumes across files instead of
      // silently dropping data.
      QList<PortFormat> result{
        { "emd", "EMD", "emd", true },
        { "hdf5", "HDF5", "h5", true },
        { "tiff", "TIFF", "tiff", false },
        { "vti", "VTK ImageData", "vti", true },
        { "mhd", "Meta Image", "mhd", false },
        { "vtk", "Legacy VTK", "vtk", true },
        { "csv", "CSV", "csv", false },
        { "xmf", "XDMF", "xmf", false },
        { "json", "JSON Image", "json", false },
      };
      // Python writers are discovered at runtime (NumPy, MRC, MATLAB,
      // ...). They all go through tomviz.internal_utils.get_array, which
      // returns the active scalars only.
      for (auto* factory :
           FileFormatManager::instance().pythonWriterFactories()) {
        auto extensions = factory->getExtensions();
        if (extensions.isEmpty()) {
          continue;
        }
        result.append({ extensions.first(), factory->getDescription(),
                        extensions.first(), false });
      }
      return result;
    }
    case PortType::Table:
      return { { "csv", "CSV", "csv", true },
               { "json", "JSON", "json", true } };
    case PortType::Molecule:
      return { { "xyz", "XYZ", "xyz", true } };
    default:
      return {};
  }
}

PortFormat formatById(PortType type, const QString& id)
{
  auto available = formats(type);
  for (const auto& format : available) {
    if (format.id == id) {
      return format;
    }
  }
  return available.isEmpty() ? PortFormat() : available.first();
}

QStringList arrayNames(const PortData& data)
{
  if (!data.isValid()) {
    return {};
  }

  if (pipeline::isVolumeType(data.type())) {
    auto volume = data.value<VolumeDataPtr>();
    if (!volume || !volume->isValid()) {
      return {};
    }
    auto names = volume->scalarNames();
    return names.isEmpty() ? QStringList{ QString() } : names;
  }

  return { QString() };
}

bool write(const PortData& data, const QStringList& arrayNames,
           const QString& path)
{
  if (!data.isValid()) {
    qCritical() << "No data to write to" << path;
    return false;
  }

  if (pipeline::isVolumeType(data.type())) {
    return writeVolume(data.value<VolumeDataPtr>(), arrayNames, path);
  }

  if (data.type() == PortType::Table) {
    return writeTable(data.value<vtkSmartPointer<vtkTable>>(), path);
  }

  if (data.type() == PortType::Molecule) {
    return moleculeToXyzFile(data.value<vtkSmartPointer<vtkMolecule>>(), path);
  }

  qCritical() << "No writer for port type"
              << pipeline::portTypeToString(data.type());
  return false;
}

} // namespace PortDataWriter
} // namespace tomviz
