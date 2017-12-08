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

#include <QDebug>
#include <QMap>
#include <QTimer>

#include <cmath>
#include <sstream>

namespace tomviz {

class DataSource::DSInternals
{
public:
  vtkNew<vtkImageData> m_transfer2D;
  vtkNew<vtkPiecewiseFunction> GradientOpacityMap;
  vtkSmartPointer<vtkSMSourceProxy> DataSourceProxy;
  QList<Operator*> Operators;
  vtkSmartPointer<vtkSMProxy> ColorMap;
  DataSource::DataSourceType Type;
  vtkSmartPointer<vtkDataArray> TiltAngles;
  vtkSmartPointer<vtkStringArray> Units;
  vtkVector3d DisplayPosition;
  PersistenceState PersistState = PersistenceState::Saved;
  double m_scaleOriginalSpacingBy = 1;

  // Checks if the tilt angles data array exists on the given VTK data
  // and creates it if it does not exist.
  void ensureTiltAnglesArrayExists()
  {
    auto alg =
      vtkAlgorithm::SafeDownCast(this->DataSourceProxy->GetClientSideObject());
    Q_ASSERT(alg);
    auto data = alg->GetOutputDataObject(0);

    auto fd = data->GetFieldData();
    if (!this->TiltAngles) {
      int* extent = vtkImageData::SafeDownCast(data)->GetExtent();
      int numTiltAngles = extent[5] - extent[4] + 1;
      vtkNew<vtkDoubleArray> array;
      array->SetName("tilt_angles");
      array->SetNumberOfTuples(numTiltAngles);
      array->FillComponent(0, 0.0);
      if (!fd->HasArray("tilt_angles")) {
        fd->AddArray(array.GetPointer());
      }
      this->TiltAngles = array.Get();
    } else {
      if (!fd->HasArray("tilt_angles")) {
        fd->AddArray(this->TiltAngles);
      }
    }
  }
};

namespace {

// Converts the save state string back to a DataSource::DataSourceType
// Returns true if the type was successfully converted, false otherwise
// the result is stored in the output paremeter type.
bool stringToDataSourceType(const char* str, DataSource::DataSourceType& type)
{
  if (strcmp(str, "volume") == 0) {
    type = DataSource::Volume;
    return true;
  } else if (strcmp(str, "tilt-series") == 0) {
    type = DataSource::TiltSeries;
    return true;
  }
  return false;
}

void deserializeDataArray(const pugi::xml_node& ns, vtkDataArray* array)
{
  int components = ns.attribute("components").as_int(1);
  array->SetNumberOfComponents(components);
  int tuples = ns.attribute("tuples").as_int(array->GetNumberOfTuples());
  array->SetNumberOfTuples(tuples);
  const char* text = ns.child_value();
  std::istringstream stream(text);
  double* data = new double[components];
  for (int i = 0; i < tuples; ++i) {
    for (int j = 0; j < components; ++j) {
      stream >> data[j];
    }
    array->SetTuple(i, data);
  }
  delete[] data;
}
}

DataSource::DataSource(vtkSMSourceProxy* dataSource, DataSourceType dataType,
                       QObject* parentObject, PersistenceState persistState)
  : QObject(parentObject), Internals(new DataSource::DSInternals())
{
  this->Internals->DataSourceProxy = dataSource;
  this->Internals->Type = dataType;
  this->Internals->PersistState = persistState;
  for (int i = 0; i < 3; ++i) {
    this->Internals->DisplayPosition[i] = 0.0;
  }

  vtkNew<vtkSMParaViewPipelineController> controller;
  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);

  // If dataSource is null then we need to create the producer
  if (this->Internals->DataSourceProxy != nullptr) {
    // We add an annotation to the proxy so that it'll be easier for code to
    // locate registered pipeline proxies that are being treated as data
    // sources.
    const char* sourceFilename = nullptr;

    QByteArray fileNameBytes;
    if (vtkSMCoreUtilities::GetFileNameProperty(dataSource) != nullptr) {

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
        // Set annotation to override filename
        dataSource->SetAnnotation(Attributes::FILENAME, sourceFilename);
      } else {
        sourceFilename = helper.GetAsString();
      }

      QFileInfo info(helper.GetAsString());
      if (info.suffix() == "mrc") {
        // MRC format uses angstroms as default units, tomviz uses nanometers.
        // This handles scaling between the two.
        this->Internals->m_scaleOriginalSpacingBy = 0.1;
      }
    } else {
      auto image = vtkImageData::SafeDownCast(this->dataObject());
      if (image) {
        double spacing[3];
        image->GetSpacing(spacing);
        for (int i = 0; i < 3; ++i) {
          spacing[i] *= this->Internals->m_scaleOriginalSpacingBy;
        }
        image->SetSpacing(spacing);
      }
    }
    //    controller->RegisterPipelineProxy(source);
  }

  // Setup color map for this data-source.
  static unsigned int colorMapCounter = 0;
  ++colorMapCounter;

  vtkNew<vtkSMTransferFunctionManager> tfmgr;
  this->Internals->ColorMap = tfmgr->GetColorTransferFunction(
    QString("DataSourceColorMap%1").arg(colorMapCounter).toLatin1().data(),
    pxm);

  // every time the data changes, we should update the color map.
  connect(this, SIGNAL(dataChanged()), SLOT(updateColorMap()));

  connect(this, &DataSource::dataPropertiesChanged,
          [this]() { this->dataSourceProxy()->MarkModified(nullptr); });
}

DataSource::DataSource(const QString& label, DataSourceType dataType,
                       QObject* parent, PersistenceState persistState)
  : DataSource(nullptr, dataType, parent, persistState)
{

  vtkSMSessionProxyManager* pxm = ActiveObjects::instance().proxyManager();
  Q_ASSERT(pxm);
  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  Q_ASSERT(source != nullptr);
  Q_ASSERT(vtkSMSourceProxy::SafeDownCast(source));
  this->Internals->DataSourceProxy = vtkSMSourceProxy::SafeDownCast(source);

  if (!label.isNull()) {
    setFilename(label);
    QByteArray bytes = label.toLatin1();
    tomviz::annotateDataProducer(source, bytes.data());
  }
}

DataSource::~DataSource()
{
}

// TODO These should probably be rename, label or name?
void DataSource::setFilename(const QString& filename)
{
  vtkSMProxy* dataSource = dataSourceProxy();
  dataSource->SetAnnotation(Attributes::FILENAME,
                            filename.toStdString().c_str());
}

QString DataSource::filename() const
{
  vtkSMProxy* dataSource = dataSourceProxy();

  if (dataSource->HasAnnotation(Attributes::FILENAME)) {
    return dataSource->GetAnnotation(Attributes::FILENAME);
  } else {
    return vtkSMPropertyHelper(
             dataSource, vtkSMCoreUtilities::GetFileNameProperty(dataSource))
      .GetAsString();
  }
}

QString DataSource::label() const
{
  vtkSMProxy* dataSource = dataSourceProxy();

  if (dataSource->HasAnnotation(Attributes::LABEL)) {
    return dataSource->GetAnnotation(Attributes::LABEL);
  } else {
    return QFileInfo(filename()).baseName();
  }
}

bool DataSource::serialize(pugi::xml_node& ns) const
{
  pugi::xml_node node = ns.append_child("ColorMap");
  tomviz::serialize(colorMap(), node);

  node = ns.append_child("OpacityMap");
  tomviz::serialize(opacityMap(), node);

  node = ns.append_child("GradientOpacityMap");
  tomviz::serialize(gradientOpacityMap(), node);

  // ns.append_attribute("number_of_operators")
  //  .set_value(static_cast<int>(this->Internals->Operators.size()));

  pugi::xml_node scale_node = ns.append_child("Spacing");
  double spacing[3];
  getSpacing(spacing);
  scale_node.append_attribute("x").set_value(spacing[0]);
  scale_node.append_attribute("y").set_value(spacing[1]);
  scale_node.append_attribute("z").set_value(spacing[2]);

  if (this->Internals->Units) {
    pugi::xml_node unit_node = ns.append_child("Units");
    unit_node.append_attribute("x").set_value(
      this->Internals->Units->GetValue(0));
    unit_node.append_attribute("y").set_value(
      this->Internals->Units->GetValue(1));
    unit_node.append_attribute("z").set_value(
      this->Internals->Units->GetValue(2));
  }

  // foreach (Operator* op, this->Internals->Operators) {
  //  pugi::xml_node operatorNode = ns.append_child("Operator");
  //  operatorNode.append_attribute("operator_type")
  //    .set_value(OperatorFactory::operatorType(op));
  //  if (!op->serialize(operatorNode)) {
  //    qWarning("failed to serialize Operator. Skipping it.");
  //    ns.remove_child(operatorNode);
  //  }
  //}
  return true;
}

bool DataSource::deserialize(const pugi::xml_node& ns)
{

  // We don't save this anymore, but so that we can continue to read legacy
  // files....
  // It should either be in the original data or in the Operator pipeline.
  DataSourceType dstype;
  if (ns.attribute("type") &&
      stringToDataSourceType(ns.attribute("type").value(), dstype)) {
    setType(dstype);
  }

  // int num_operators = ns.attribute("number_of_operators").as_int(-1);
  // if (num_operators < 0) {
  //  return false;
  // }

  /// this->Internals->Operators.clear();

  // load the color map here to avoid resetData clobbering its range
  tomviz::deserialize(colorMap(), ns.child("ColorMap"));
  tomviz::deserialize(opacityMap(), ns.child("OpacityMap"));

  pugi::xml_node nodeGrad = ns.child("GradientOpacityMap");
  if (nodeGrad) {
    tomviz::deserialize(gradientOpacityMap(), nodeGrad);
  }

  vtkSMPropertyHelper(colorMap(), "ScalarOpacityFunction").Set(opacityMap());
  colorMap()->UpdateVTKObjects();

  // load tilt angles AFTER resetData call.  Again this is no longer saved and
  // the load code is for legacy support.  This should be saved by the
  // SetTiltAnglesOperator.
  if (type() == TiltSeries && ns.child("TiltAngles")) {
    deserializeDataArray(ns.child("TiltAngles"), this->Internals->TiltAngles);
  }

  if (ns.child("Spacing")) {
    pugi::xml_node scale_node = ns.child("Spacing");
    double spacing[3];
    spacing[0] = scale_node.attribute("x").as_double();
    spacing[1] = scale_node.attribute("y").as_double();
    spacing[2] = scale_node.attribute("z").as_double();
    setSpacing(spacing);
  }
  if (ns.child("Units")) {
    // Ensure the array exists
    pugi::xml_node unit_node = ns.child("Units");
    setUnits("nm");
    this->Internals->Units->SetValue(0, unit_node.attribute("x").value());
    this->Internals->Units->SetValue(1, unit_node.attribute("y").value());
    this->Internals->Units->SetValue(2, unit_node.attribute("z").value());
  }

  //  for (pugi::xml_node node = ns.child("Operator"); node;
  //       node = node.next_sibling("Operator")) {
  //    Operator* op(OperatorFactory::createOperator(
  //      node.attribute("operator_type").value(), this));
  //    if (op) {
  //      if (op->deserialize(node)) {
  //        addOperator(op);
  //      }
  //    }
  //  }
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
    new DataSource(this->Internals->DataSourceProxy, this->Internals->Type);

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

vtkSMSourceProxy* DataSource::dataSourceProxy() const
{
  return this->Internals->DataSourceProxy;
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

void DataSource::setSpacing(const double spacing[3])
{
  double mySpacing[3] = { spacing[0], spacing[1], spacing[2] };
  vtkAlgorithm* alg = algorithm();
  if (alg) {
    vtkImageData* data =
      vtkImageData::SafeDownCast(alg->GetOutputDataObject(0));
    if (data) {
      data->SetSpacing(mySpacing);
    }
  }
  alg = vtkAlgorithm::SafeDownCast(dataSourceProxy()->GetClientSideObject());
  if (alg) {
    vtkImageData* data =
      vtkImageData::SafeDownCast(alg->GetOutputDataObject(0));
    if (data) {
      data->SetSpacing(mySpacing);
    }
  }
  emit dataPropertiesChanged();
}

unsigned int DataSource::getNumberOfComponents()
{
  unsigned int numComponents = 0;
  vtkAlgorithm* tp = vtkAlgorithm::SafeDownCast(
    this->Internals->Producer->GetClientSideObject());
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

QString DataSource::getUnits(int axis)
{
  if (this->Internals->Units) {
    return QString(this->Internals->Units->GetValue(axis));
  } else {
    return "nm";
  }
}

void DataSource::setUnits(const QString& units)
{
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
  vtkTrivialProducer* tp = trivialProducer();
  if (tp == nullptr) {
    return;
  }

  tp->Modified();
  vtkDataObject* dObject = tp->GetOutputDataObject(0);
  dObject->Modified();
  this->Internals->DataSourceProxy->MarkModified(nullptr);

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
    this->Internals->DataSourceProxy->GetSessionProxyManager();
  vtkSMSourceProxy* filter =
    vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("filters", "PassThrough"));
  Q_ASSERT(filter);
  vtkSMPropertyHelper(filter, "Input").Set(this->Internals->DataSourceProxy, 0);
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
  this->Internals->DataSourceProxy->UpdatePipeline();
  vtkDataObject* data = dataObject();
  vtkDataObject* copy = data->NewInstance();
  copy->DeepCopy(data);

  return copy;
}

void DataSource::setData(vtkDataObject* newData)
{
  vtkSMSourceProxy* source = this->Internals->DataSourceProxy;
  Q_ASSERT(source != nullptr);

  vtkTrivialProducer* tp = trivialProducer();
  Q_ASSERT(tp);
  tp->SetOutput(newData);
  newData->FastDelete();
  vtkFieldData* fd = newData->GetFieldData();
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
  vtkTrivialProducer* tp = trivialProducer();
  vtkDataObject* data = tp->GetOutputDataObject(0);
  vtkFieldData* fd = data->GetFieldData();
  vtkTypeInt8Array* typeArray =
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
  vtkDataArray* tiltAngles = this->Internals->TiltAngles;
  return tiltAngles != nullptr;
}

QVector<double> DataSource::getTiltAngles() const
{
  QVector<double> result;
  vtkAlgorithm* tp = algorithm();
  vtkDataObject* data = tp->GetOutputDataObject(0);
  vtkFieldData* fd = data->GetFieldData();
  vtkDataArray* tiltAngles = this->Internals->TiltAngles;
  //  if (fd->HasArray("tilt_angles") && !useOriginalDataTiltAngles &&
  //      fd->GetArray("tilt_angles") != this->Internals->TiltAngles.Get()) {
  //    tiltAngles = fd->GetArray("tilt_angles");
  //  }
  if (tiltAngles) {
    result.resize(tiltAngles->GetNumberOfTuples());
    for (int i = 0; i < result.size(); ++i) {
      result[i] = tiltAngles->GetTuple1(i);
    }
  }
  return result;
}

void DataSource::setTiltAngles(const QVector<double>& angles)
{
  vtkDataArray* tiltAngles = this->Internals->TiltAngles;
  vtkAlgorithm* tp = algorithm();
  vtkDataObject* data = tp->GetOutputDataObject(0);
  vtkFieldData* fd = data->GetFieldData();
  if (fd->GetArray("tilt_angles") != this->Internals->TiltAngles) {
    tiltAngles = fd->GetArray("tilt_angles");
  }
  if (tiltAngles) {
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
  return this->Internals->GradientOpacityMap.GetPointer();
}

vtkImageData* DataSource::transferFunction2D() const
{
  return this->Internals->m_transfer2D.GetPointer();
}

bool DataSource::hasLabelMap()
{
  vtkSMSourceProxy* dataSource = dataSourceProxy();
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
  // rescale the color/opacity maps for the data source.
  tomviz::rescaleColorMap(colorMap(), this);
}


bool DataSource::isImageStack()
{
  vtkSMPropertyHelper helper(
    dataSourceProxy(),
    vtkSMCoreUtilities::GetFileNameProperty(dataSourceProxy()));

  return helper.GetNumberOfElements() > 1;
}

void DataSource::setPersistenceState(DataSource::PersistenceState state)
{
  this->Internals->PersistState = state;
}

DataSource::PersistenceState DataSource::persistenceState() const
{
  return this->Internals->PersistState;
}

vtkTrivialProducer* DataSource::trivialProducer()
{
  return vtkTrivialProducer::SafeDownCast(
    this->Internals->DataSourceProxy->GetClientSideObject());
}

vtkAlgorithm* DataSource::algorithm() const
{
  return vtkAlgorithm::SafeDownCast(
    this->Internals->DataSourceProxy->GetClientSideObject());
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
}
