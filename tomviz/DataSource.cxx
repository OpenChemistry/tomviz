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
#include "DataSource.h"

#include "ActiveObjects.h"
#include "ModuleFactory.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "OperatorFactory.h"
#include "Pipeline.h"
#include "Utilities.h"

#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTrivialProducer.h>
#include <vtkTypeInt8Array.h>
#include <vtkVector.h>

#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>
#include <vtkSMCoreUtilities.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMViewProxy.h>

#include <QDebug>
#include <QJsonArray>
#include <QMap>
#include <QTimer>

#include <cmath>
#include <cstring>
#include <sstream>

namespace tomviz {

class DataSource::DSInternals
{
public:
  vtkNew<vtkImageData> m_transfer2D;
  vtkNew<vtkPiecewiseFunction> GradientOpacityMap;
//  vtkSmartPointer<vtkSMSourceProxy> DataSourceProxy;
  vtkSmartPointer<vtkSMSourceProxy> ProducerProxy;
  QList<Operator*> Operators;
  vtkSmartPointer<vtkSMProxy> ColorMap;
  DataSource::DataSourceType Type;
  vtkSmartPointer<vtkStringArray> Units;
  vtkVector3d DisplayPosition;
  PersistenceState PersistState = PersistenceState::Saved;
  bool UnitsModified = false;
  bool Forkable = true;

  // Checks if the tilt angles data array exists on the given VTK data
  // and creates it if it does not exist.
  void ensureTiltAnglesArrayExists()
  {
    auto alg =
      vtkAlgorithm::SafeDownCast(this->ProducerProxy->GetClientSideObject());
    Q_ASSERT(alg);
    auto data = alg->GetOutputDataObject(0);
    auto fd = data->GetFieldData();
    if (!fd->HasArray("tilt_angles")) {
      int* extent = vtkImageData::SafeDownCast(data)->GetExtent();
      int numTiltAngles = extent[5] - extent[4] + 1;
      vtkNew<vtkDoubleArray> array;
      array->SetName("tilt_angles");
      array->SetNumberOfTuples(numTiltAngles);
      array->FillComponent(0, 0.0);
      fd->AddArray(array);
    }
  }
};

DataSource::DataSource(vtkSMSourceProxy* dataSource,
                       DataSourceType dataType)
  : QObject(nullptr), Internals(new DSInternals)
{
  const char* sourceFilename = nullptr;

  QByteArray fileNameBytes;
  if (vtkSMCoreUtilities::GetFileNameProperty(dataSource)) {
    vtkSMPropertyHelper helper(
      dataSource, vtkSMCoreUtilities::GetFileNameProperty(dataSource));
    // If we are dealing with an image stack find the prefix to use
    // when displaying the data source.
    if (helper.GetNumberOfElements() > 1) {
      QStringList fileNames;
      for (unsigned int i = 0; i < helper.GetNumberOfElements(); i++) {
        fileNames << QString(helper.GetAsString(i));
      }
      QFileInfo fileInfo(fileNames[0]);
      QString fileName =
        QString("%1*.%2").arg(findPrefix(fileNames)).arg(fileInfo.suffix());
      fileNameBytes = fileName.toLatin1();
      sourceFilename = fileNameBytes.data();
    } else {
      sourceFilename = helper.GetAsString();
    }

    QFileInfo info(helper.GetAsString());
    if (info.suffix() == "mrc") {
      // MRC format uses angstroms as default units, tomviz uses nanometers.
      // This handles scaling between the two.
      m_scaleOriginalSpacingBy = 0.1;
    }
  }

  dataSource->UpdatePipeline();
  auto algo = vtkAlgorithm::SafeDownCast(dataSource->GetClientSideObject());
  Q_ASSERT(algo);
  auto data = algo->GetOutputDataObject(0);
  auto image = vtkImageData::SafeDownCast(data);

  // Initialize our object, and set the file name.
  init(image, dataType, PersistenceState::Saved);
  setFileName(sourceFilename);
}

DataSource::DataSource(vtkImageData* data, DataSourceType dataType,
                       QObject* parentObject, PersistenceState persistState)
  : QObject(parentObject), Internals(new DSInternals)
{
  init(data, dataType, persistState);
}

DataSource::DataSource(const QString& label, DataSourceType dataType,
                       QObject* parent, PersistenceState persistState,
                       const QJsonObject& sourceInfo)
  : QObject(parent), Internals(new DSInternals)
{
  init(nullptr, dataType, persistState);

  if (!label.isNull()) {
    setLabel(label);
  }
  if (!sourceInfo.isEmpty()) {
    m_json["sourceInformation"] = sourceInfo;
  }
}

DataSource::~DataSource()
{
  if (this->Internals->ProducerProxy) {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(this->Internals->ProducerProxy);
  }
}

bool DataSource::appendSlice(vtkImageData* slice)
{
  if (!slice) {
    return false;
  }
  if (std::string(slice->GetScalarTypeAsString()) != "unsigned char") {
    cout << "Only unsigned char is supported at present" << endl;
    return false;
  }

  int sliceExtents[6];
  slice->GetExtent(sliceExtents);
  auto tp = algorithm();
  if (tp) {
    auto data = vtkImageData::SafeDownCast(tp->GetOutputDataObject(0));
    if (data) {
      int extents[6];
      data->GetExtent(extents);
      cout << "The data is ";
      for (int i = 0; i < 6; ++i)
        cout << extents[i] << ", ";
      cout << endl;
      for (int i = 0; i < 4; ++i) {
        if (extents[i] != sliceExtents[i]) {
          cout << "Mismatch: " << extents[i] << " != " << sliceExtents[i]
               << endl;
          return false;
        }
      }

      // Now to append the slice onto our image data.
      auto dataArray = data->GetPointData()->GetScalars();

      int bufferSize = dataArray->GetNumberOfTuples();
      unsigned char* buffer = new unsigned char[bufferSize];
      void* ptr = dataArray->GetVoidPointer(0);
      std::memcpy(buffer, ptr, bufferSize);
      ++extents[5];
      data->SetExtent(extents);
      data->AllocateScalars(data->GetScalarType(),
                            data->GetNumberOfScalarComponents());
      ptr = data->GetScalarPointer();
      std::memcpy(ptr, buffer, bufferSize);
      delete[] buffer;
      buffer = 0;
      void* imagePtr = data->GetScalarPointer(0, 0, extents[5]);
      auto sliceArray = slice->GetPointData()->GetScalars();
      void* slicePtr = sliceArray->GetVoidPointer(0);
      std::memcpy(imagePtr, slicePtr, sliceArray->GetNumberOfTuples());

      // Let everyone know the data has changed, then re-execute the pipeline.
      data->Modified();
      emit dataChanged();
      emit dataPropertiesChanged();
      pipeline()->execute();
    }
  }
  return true;
}

void DataSource::setFileName(const QString& filename)
{
  auto reader = m_json.value("reader").toObject(QJsonObject());
  reader["fileName"] = filename;
  m_json["reader"] = reader;
}

QString DataSource::fileName() const
{
  auto reader = m_json.value("reader").toObject(QJsonObject());
  if (reader.contains("fileName")) {
    return reader["fileName"].toString();
  }
  return QString();
}

void DataSource::setFileNames(const QStringList fileNames)
{
  auto reader = m_json.value("reader").toObject(QJsonObject());
  QJsonArray files;
  foreach (QString file, fileNames) {
    files.append(file);
  }

  reader["fileNames"] = files;
  m_json["reader"] = reader;
}

QStringList DataSource::fileNames() const
{
  auto reader = m_json.value("reader").toObject(QJsonObject());
  QStringList files;
  if (reader.contains("fileNames") && isImageStack()) {
    QJsonArray fileArray = reader["fileNames"].toArray();
    foreach (QJsonValue file, fileArray) {
      files.append(file.toString());
    }
  }
  return files;
}

bool DataSource::isImageStack() const
{
  return m_json.contains("fileNames") && m_json["fileNames"].isArray() &&
    m_json["fileNames"].toArray().size() > 1;
}

void DataSource::setReaderProperties(const QVariantMap& properties)
{
  m_json["reader"] = QJsonObject::fromVariantMap(properties);
}

QVariantMap DataSource::readerProperties() const
{
  if (m_json.contains("reader") && m_json["reader"].isObject()) {
    return m_json["reader"].toObject().toVariantMap();
  } else {
    return QVariantMap();
  }
}

void DataSource::setLabel(const QString& label)
{
  m_json["label"] = label;
}

QString DataSource::label() const
{
  if (m_json.contains("label")) {
    return m_json["label"].toString();
  } else {
    return QFileInfo(fileName()).baseName();
  }
}

QJsonObject DataSource::serialize() const
{
  QJsonObject json = m_json;

  if (Internals->UnitsModified) {
    double spacing[3];
    getSpacing(spacing);
    QJsonArray jsonSpacing;
    for (int i = 0; i < 3; ++i) {
      jsonSpacing.append(spacing[i]);
    }

    json["spacing"] = jsonSpacing;
    if (this->Internals->Units) {
      json["units"] = this->Internals->Units->GetValue(0).c_str();
    }
  }

  // Serialize the color map, opacity map, and others if needed.
  json["colorOpacityMap"] = tomviz::serialize(colorMap());
  json["gradientOpacityMap"] = tomviz::serialize(gradientOpacityMap());

  // Serialize the operators...
  QJsonArray jOperators;
  foreach (Operator* op, this->Internals->Operators) {
    QJsonObject jOperator = op->serialize();
    jOperator["type"] = OperatorFactory::operatorType(op);
    jOperators.append(jOperator);
  }
  if (!jOperators.isEmpty()) {
    json["operators"] = jOperators;
  }

  // Serialize the modules...
  auto modules = ModuleManager::instance().findModulesGeneric(this, nullptr);
  QJsonArray jModules;
  foreach (Module* module, modules) {
    QJsonObject jModule = module->serialize();
    jModule["type"] = ModuleFactory::moduleType(module);
    jModule["viewId"] = static_cast<int>(module->view()->GetGlobalID());

    jModules.append(jModule);
  }
  if (!jModules.isEmpty()) {
    json["modules"] = jModules;
  }

  return json;
}

bool DataSource::deserialize(const QJsonObject& state)
{
  if (state.contains("colorOpacityMap")) {
    tomviz::deserialize(colorMap(), state["colorOpacityMap"].toObject());
  }
  if (state.contains("gradientOpacityMap")) {
    tomviz::deserialize(gradientOpacityMap(),
                        state["gradientOpacityMap"].toObject());
  }

  if (state.contains("spacing")) {
    auto spacingArray = state["spacing"].toArray();
    double spacing[3];
    for (int i = 0; i < 3; i++) {
      spacing[i] = spacingArray.at(i).toDouble();
    }
    setSpacing(spacing);
  }

  if (state.contains("units")) {
    auto units = state["units"].toString();
    setUnits(units);
  }

  // Check for modules on the data source first.
  if (state.contains("modules") && state["modules"].isArray()) {
    auto moduleArray = state["modules"].toArray();
    for (int i = 0; i < moduleArray.size(); ++i) {
      auto moduleObj = moduleArray[i].toObject();
      auto viewId = moduleObj["viewId"].toInt();
      auto viewProxy = ModuleManager::instance().lookupView(viewId);
      auto type = moduleObj["type"].toString();
      auto m =
        ModuleManager::instance().createAndAddModule(type, this, viewProxy);
      m->deserialize(moduleObj);
    }
  }
  // Now check for operators on the data source.
  if (state.contains("operators") && state["operators"].isArray()) {
    pipeline()->pause();
    Operator* op = nullptr;
    QJsonObject operatorObj;
    auto operatorArray = state["operators"].toArray();
    for (int i = 0; i < operatorArray.size(); ++i) {
      operatorObj = operatorArray[i].toObject();
      op =
        OperatorFactory::createOperator(operatorObj["type"].toString(), this);
      if (op && op->deserialize(operatorObj)) {
        addOperator(op);
      }
    }

    // If we have a child data source we need to restore it once the data source
    // has been create by the first execution of the pipeline.
    if (op != nullptr && operatorObj.contains("dataSources")) {
      // We currently support a single child data source.
      auto dataSourcesState = operatorObj["dataSources"].toArray();
      connect(pipeline(), &Pipeline::finished, op, [dataSourcesState, op]() {
        auto childDataSource = op->childDataSource();
        if (childDataSource != nullptr) {
          childDataSource->deserialize(dataSourcesState[0].toObject());
        }
      });
    }

    pipeline()->resume(true);
  }
  return true;
}

DataSource* DataSource::clone(bool cloneOperators) const
{
  // TODO I don't think this is necessary
  //    if (vtkSMCoreUtilities::GetFileNameProperty(
  //          this->Internals->OriginalDataSource) != nullptr) {
  //      const char* originalFilename =
  //        vtkSMPropertyHelper(this->Internals->OriginalDataSource,
  //                            vtkSMCoreUtilities::GetFileNameProperty(
  //                              this->Internals->OriginalDataSource))
  //          .GetAsString();
  //      this->Internals->Producer->SetAnnotation(Attributes::FILENAME,
  //                                               originalFilename);
  //    } else {
  //      this->Internals->Producer->SetAnnotation(
  //        Attributes::FILENAME,
  //        originalDataSource()->GetAnnotation(Attributes::FILENAME));
  //    }

  auto newClone =
    new DataSource(this->Internals->ProducerProxy, this->Internals->Type);

  if (this->persistenceState() == PersistenceState::Transient ||
      this->persistenceState() == PersistenceState::Modified) {
    newClone->setPersistenceState(PersistenceState::Modified);
  }

  if (this->Internals->Type == TiltSeries) {
    newClone->setTiltAngles(getTiltAngles());
  }
  if (cloneOperators) {
    // now, clone the operators.
    foreach (Operator* op, this->Internals->Operators) {
      Operator* opClone(op->clone());
      newClone->addOperator(opClone);
    }
    newClone->setTiltAngles(getTiltAngles());
  }
  return newClone;
}

vtkSMSourceProxy* DataSource::proxy() const
{
  return this->Internals->ProducerProxy;
}

void DataSource::getExtent(int extent[6])
{
  vtkAlgorithm* tp = algorithm();
  if (tp) {
    vtkImageData* data = vtkImageData::SafeDownCast(tp->GetOutputDataObject(0));
    if (data) {
      data->GetExtent(extent);
      return;
    }
  }
  for (int i = 0; i < 6; ++i) {
    extent[i] = 0;
  }
}

void DataSource::getBounds(double bounds[6])
{
  vtkAlgorithm* tp = algorithm();
  if (tp) {
    vtkImageData* data = vtkImageData::SafeDownCast(tp->GetOutputDataObject(0));
    if (data) {
      data->GetBounds(bounds);
      return;
    }
  }
  for (int i = 0; i < 6; ++i) {
    bounds[i] = 0.0;
  }
}

void DataSource::getSpacing(double spacing[3]) const
{
  vtkAlgorithm* tp = algorithm();
  if (tp) {
    vtkImageData* data = vtkImageData::SafeDownCast(tp->GetOutputDataObject(0));
    if (data) {
      data->GetSpacing(spacing);
      return;
    }
  }
  for (int i = 0; i < 3; ++i) {
    spacing[i] = 1;
  }
}

void DataSource::setSpacing(const double spacing[3], bool markModified)
{
  if (markModified) {
    Internals->UnitsModified = true;
  }

  double mySpacing[3] = { spacing[0], spacing[1], spacing[2] };
  vtkAlgorithm* alg = algorithm();
  if (alg) {
    vtkImageData* data =
      vtkImageData::SafeDownCast(alg->GetOutputDataObject(0));
    if (data) {
      data->SetSpacing(mySpacing);
    }
  }
  alg = vtkAlgorithm::SafeDownCast(proxy()->GetClientSideObject());
  if (alg) {
    vtkImageData* data =
      vtkImageData::SafeDownCast(alg->GetOutputDataObject(0));
    if (data) {
      data->SetSpacing(mySpacing);
    }
  }
  emit dataPropertiesChanged();
}

void DataSource::setActiveScalars(const QString& arrayName)
{
  vtkAlgorithm* alg = algorithm();
  if (alg) {
    vtkImageData* data =
      vtkImageData::SafeDownCast(alg->GetOutputDataObject(0));
    if (data) {
      data->GetPointData()->SetActiveScalars(arrayName.toLatin1().data());
    }
  }

  dataModified();

  emit activeScalarsChanged();
  emit dataPropertiesChanged();
}

QString DataSource::activeScalars() const
{
  QString returnValue;
  vtkAlgorithm* alg = algorithm();
  if (alg) {
    vtkImageData* data =
      vtkImageData::SafeDownCast(alg->GetOutputDataObject(0));
    if (data) {
      vtkDataArray* scalars = data->GetPointData()->GetScalars();
      if (scalars) {
        returnValue = scalars->GetName();
      }
    }
  }

  return returnValue;
}

unsigned int DataSource::getNumberOfComponents()
{
  unsigned int numComponents = 0;
  vtkAlgorithm* tp = this->algorithm();
  if (tp) {
    vtkImageData* data = vtkImageData::SafeDownCast(tp->GetOutputDataObject(0));
    if (data) {
      if (data->GetPointData() && data->GetPointData()->GetScalars()) {
        numComponents = static_cast<unsigned int>(data->GetPointData()->GetScalars()->GetNumberOfComponents());
      }
    }
  }

  return numComponents;
}

QString DataSource::getUnits()
{
  if (this->Internals->Units) {
    return QString(this->Internals->Units->GetValue(0));
  } else {
    return "nm";
  }
}

void DataSource::setUnits(const QString& units, bool markModified)
{
  if (markModified) {
    Internals->UnitsModified = true;
  }

  if (!this->Internals->Units) {
    this->Internals->Units = vtkSmartPointer<vtkStringArray>::New();
    this->Internals->Units->SetName("units");
    this->Internals->Units->SetNumberOfValues(3);
    this->Internals->Units->SetValue(0, "nm");
    this->Internals->Units->SetValue(1, "nm");
    this->Internals->Units->SetValue(2, "nm");
    vtkAlgorithm* alg = algorithm();
    if (alg) {
      vtkDataObject* data = alg->GetOutputDataObject(0);
      vtkFieldData* fd = data->GetFieldData();
      fd->AddArray(this->Internals->Units);
    }
  }
  for (int i = 0; i < 3; ++i) {
    this->Internals->Units->SetValue(i, units.toStdString().c_str());
  }
  emit dataPropertiesChanged();
}

int DataSource::addOperator(Operator* op)
{
  op->setParent(this);
  int index = this->Internals->Operators.count();
  this->Internals->Operators.push_back(op);
  emit operatorAdded(op);

  return index;
}

bool DataSource::removeOperator(Operator* op)
{
  if (op) {
    // We should emit that the operator was removed...
    this->Internals->Operators.removeAll(op);

    emit this->operatorRemoved(op);

    op->deleteLater();

    foreach (Operator* opPtr, this->Internals->Operators) {
      cout << "Operator: " << opPtr->label().toLatin1().data() << endl;
    }

    return true;
  }
  return false;
}

bool DataSource::removeAllOperators()
{
  // TODO - return false if the pipeline is running
  bool success = true;

  while (this->Internals->Operators.size() > 0) {
    Operator* lastOperator = this->Internals->Operators.takeLast();

    if (lastOperator->childDataSource() != nullptr) {
      DataSource* childDataSource = lastOperator->childDataSource();

      // Recurse on the child data source
      success = childDataSource->removeAllOperators();
      if (!success) {
        break;
      }
    }

    lastOperator->deleteLater();
  }

  if (success) {
    ModuleManager::instance().removeAllModules(this);
  }

  return success;
}

void DataSource::dataModified()
{
  vtkTrivialProducer* tp = producer();
  if (tp == nullptr) {
    return;
  }

  tp->Modified();
  vtkDataObject* dObject = tp->GetOutputDataObject(0);
  dObject->Modified();
  this->Internals->ProducerProxy->MarkModified(nullptr);

  vtkFieldData* fd = dObject->GetFieldData();
  if (fd->HasArray("tomviz_data_source_type")) {
    vtkTypeInt8Array* typeArray =
      vtkTypeInt8Array::SafeDownCast(fd->GetArray("tomviz_data_source_type"));

    // Casting is a bit hacky here, but it *should* work
    setType((DataSourceType)(int)typeArray->GetTuple1(0));
  } else {
    vtkNew<vtkTypeInt8Array> typeArray;
    typeArray->SetNumberOfComponents(1);
    typeArray->SetNumberOfTuples(1);
    typeArray->SetName("tomviz_data_source_type");
    typeArray->SetTuple1(0, this->Internals->Type);
    fd->AddArray(typeArray.Get());
  }

  // This indirection is necessary to overcome a bug in VTK/ParaView when
  // explicitly calling UpdatePipeline(). The extents don't reset to the whole
  // extent. Until a  proper fix makes it into VTK, this is needed.
  vtkSMSessionProxyManager* pxm =
    this->Internals->ProducerProxy->GetSessionProxyManager();
  vtkSMSourceProxy* filter =
    vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("filters", "PassThrough"));
  Q_ASSERT(filter);
  vtkSMPropertyHelper(filter, "Input").Set(this->Internals->ProducerProxy, 0);
  filter->UpdateVTKObjects();
  filter->UpdatePipeline();
  filter->Delete();

  emit dataChanged();
}

const QList<Operator*>& DataSource::operators() const
{
  return this->Internals->Operators;
}

void DataSource::translate(const double deltaPosition[3])
{
  for (int i = 0; i < 3; ++i) {
    this->Internals->DisplayPosition[i] += deltaPosition[i];
  }
  emit displayPositionChanged(this->Internals->DisplayPosition[0],
                              this->Internals->DisplayPosition[1],
                              this->Internals->DisplayPosition[2]);
}

const double* DataSource::displayPosition()
{
  return this->Internals->DisplayPosition.GetData();
}

void DataSource::setDisplayPosition(const double newPosition[3])
{
  for (int i = 0; i < 3; ++i) {
    this->Internals->DisplayPosition[i] = newPosition[i];
  }
  emit displayPositionChanged(this->Internals->DisplayPosition[0],
                              this->Internals->DisplayPosition[1],
                              this->Internals->DisplayPosition[2]);
}

vtkDataObject* DataSource::copyData()
{
  this->Internals->ProducerProxy->UpdatePipeline();
  vtkDataObject* data = dataObject();
  vtkDataObject* copy = data->NewInstance();
  copy->DeepCopy(data);

  return copy;
}

void DataSource::setData(vtkDataObject* newData)
{
  auto tp = producer();
  Q_ASSERT(tp);
  tp->SetOutput(newData);
  auto fd = newData->GetFieldData();
  vtkSmartPointer<vtkTypeInt8Array> typeArray =
    vtkTypeInt8Array::SafeDownCast(fd->GetArray("tomviz_data_source_type"));
  if (typeArray && typeArray->GetTuple1(0) == TiltSeries) {
    this->Internals->ensureTiltAnglesArrayExists();
    this->Internals->Type = TiltSeries;
  } else {
    this->Internals->Type = Volume;
  }
  if (fd->HasArray("units")) {
    this->Internals->Units =
      vtkStringArray::SafeDownCast(fd->GetAbstractArray("units"));
  } else if (this->Internals->Units) {
    fd->AddArray(this->Internals->Units);
  }
  if (!typeArray) {
    typeArray = vtkSmartPointer<vtkTypeInt8Array>::New();
    typeArray->SetNumberOfComponents(1);
    typeArray->SetNumberOfTuples(1);
    typeArray->SetName("tomviz_data_source_type");
    fd->AddArray(typeArray);
  }
  typeArray->SetTuple1(0, this->Internals->Type);
}

void DataSource::copyData(vtkDataObject* newData)
{
  auto tp = producer();
  Q_ASSERT(tp);
  auto oldData = tp->GetOutputDataObject(0);
  Q_ASSERT(oldData);

  oldData->DeepCopy(newData);

  dataModified();
}

vtkSMProxy* DataSource::colorMap() const
{
  return this->Internals->ColorMap;
}

DataSource::DataSourceType DataSource::type() const
{
  return this->Internals->Type;
}

void DataSource::setType(DataSourceType t)
{
  this->Internals->Type = t;
  auto tp = producer();
  auto data = tp->GetOutputDataObject(0);
  auto fd = data->GetFieldData();
  auto typeArray =
    vtkTypeInt8Array::SafeDownCast(fd->GetArray("tomviz_data_source_type"));
  assert(typeArray);
  typeArray->SetTuple1(0, t);
  if (t == TiltSeries) {
    this->Internals->ensureTiltAnglesArrayExists();
  }
  emit dataChanged();
}

bool DataSource::hasTiltAngles()
{
  vtkDataObject* data = this->dataObject();
  vtkFieldData* fd = data->GetFieldData();

  return fd->HasArray("tilt_angles");
}

QVector<double> DataSource::getTiltAngles() const
{
  QVector<double> result;
  auto data = this->dataObject();
  auto fd = data->GetFieldData();

  if (fd->HasArray("tilt_angles")) {
    auto tiltAngles = fd->GetArray("tilt_angles");
    result.resize(tiltAngles->GetNumberOfTuples());
    for (int i = 0; i < result.size(); ++i) {
      result[i] = tiltAngles->GetTuple1(i);
    }
  }
  return result;
}

void DataSource::setTiltAngles(const QVector<double>& angles)
{
  auto data = this->dataObject();
  auto fd = data->GetFieldData();
  if (fd->HasArray("tilt_angles")) {
    auto tiltAngles = fd->GetArray("tilt_angles");
    for (int i = 0; i < tiltAngles->GetNumberOfTuples() && i < angles.size();
         ++i) {
      tiltAngles->SetTuple1(i, angles[i]);
    }
  }
  emit dataChanged();
}

vtkSMProxy* DataSource::opacityMap() const
{
  return this->Internals->ColorMap
           ? vtkSMPropertyHelper(this->Internals->ColorMap,
                                 "ScalarOpacityFunction")
               .GetAsProxy()
           : nullptr;
}

vtkPiecewiseFunction* DataSource::gradientOpacityMap() const
{
  return this->Internals->GradientOpacityMap;
}

vtkImageData* DataSource::transferFunction2D() const
{
  return this->Internals->m_transfer2D;
}

bool DataSource::hasLabelMap()
{
  auto dataSource = proxy();
  if (!dataSource) {
    return false;
  }

  // We could just as easily go to the client side VTK object to get this info,
  // but we'll go the ParaView route for now.
  vtkPVDataInformation* dataInfo = dataSource->GetDataInformation();
  vtkPVDataSetAttributesInformation* pointDataInfo =
    dataInfo->GetPointDataInformation();
  vtkPVArrayInformation* labelMapInfo =
    pointDataInfo->GetArrayInformation("LabelMap");

  return labelMapInfo != nullptr;
}

void DataSource::updateColorMap()
{
  rescaleColorMap(colorMap(), this);
}

void DataSource::setPersistenceState(DataSource::PersistenceState state)
{
  this->Internals->PersistState = state;
}

DataSource::PersistenceState DataSource::persistenceState() const
{
  return this->Internals->PersistState;
}

vtkTrivialProducer* DataSource::producer() const
{
  Q_ASSERT(vtkTrivialProducer::SafeDownCast(proxy()->GetClientSideObject()));
  return vtkTrivialProducer::SafeDownCast(proxy()->GetClientSideObject());
}

void DataSource::init(vtkImageData* data, DataSourceType dataType,
                      PersistenceState persistState)
{
  this->Internals->Type = dataType;
  this->Internals->PersistState = persistState;
  this->Internals->DisplayPosition.Set(0, 0, 0);

  vtkNew<vtkSMParaViewPipelineController> controller;
  auto pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  // If dataSource is null then we need to create the producer
  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  Q_ASSERT(source != nullptr);
  Q_ASSERT(vtkSMSourceProxy::SafeDownCast(source));
  this->Internals->ProducerProxy = vtkSMSourceProxy::SafeDownCast(source);
  controller->RegisterPipelineProxy(this->Internals->ProducerProxy);

  if (data) {
    auto copy = data->NewInstance();
    copy->DeepCopy(data);
    auto image = vtkImageData::SafeDownCast(copy);
    auto tp = vtkTrivialProducer::SafeDownCast(source->GetClientSideObject());
    tp->SetOutput(image);
    image->Delete();

    // This is a little hackish, currently special cased for the MRC format.
    // It would probably be best to move this to the file read/write classes.
    if (image && m_scaleOriginalSpacingBy != 1.0) {
      double spacing[3];
      image->GetSpacing(spacing);
      for (int i = 0; i < 3; ++i) {
        spacing[i] *= m_scaleOriginalSpacingBy;
      }
      image->SetSpacing(spacing);
    }
  }

  // Setup color map for this data-source.
  static unsigned int colorMapCounter = 0;
  ++colorMapCounter;

  vtkNew<vtkSMTransferFunctionManager> tfmgr;
  this->Internals->ColorMap = tfmgr->GetColorTransferFunction(
    QString("DataSourceColorMap%1").arg(colorMapCounter).toLatin1().data(),
    pxm);
  updateColorMap();

  // Every time the data changes, we should update the color map.
  connect(this, SIGNAL(dataChanged()), SLOT(updateColorMap()));

  connect(this, &DataSource::dataPropertiesChanged,
          [this]() { this->proxy()->MarkModified(nullptr); });
}

vtkAlgorithm* DataSource::algorithm() const
{
  return vtkAlgorithm::SafeDownCast(proxy()->GetClientSideObject());
}

vtkDataObject* DataSource::dataObject() const
{
  auto alg = algorithm();
  Q_ASSERT(alg);
  return alg->GetOutputDataObject(0);
}

Pipeline* DataSource::pipeline()
{
  return qobject_cast<Pipeline*>(parent());
}

bool DataSource::unitsModified()
{
  return Internals->UnitsModified;
}

bool DataSource::isTransient() const
{
  return Internals->PersistState == PersistenceState::Transient;
}

bool DataSource::forkable()
{
  return Internals->Forkable;
}

void DataSource::setForkable(bool forkable)
{
  Internals->Forkable = forkable;
}
}
