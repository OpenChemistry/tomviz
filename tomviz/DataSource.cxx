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

#include "Operator.h"
#include "OperatorFactory.h"
#include "Utilities.h"
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkNew.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include <vtkSMCoreUtilities.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkTrivialProducer.h>
#include <vtkTypeInt8Array.h>
#include <vtkVector.h>

#include <vtkPVArrayInformation.h>
#include <vtkPVDataInformation.h>
#include <vtkPVDataSetAttributesInformation.h>

#include <vtk_pugixml.h>

#include <sstream>

namespace tomviz
{

class DataSource::DSInternals
{
public:
  vtkSmartPointer<vtkSMSourceProxy> OriginalDataSource;
  vtkWeakPointer<vtkSMSourceProxy> Producer;
  QList<QSharedPointer<Operator> > Operators;
  vtkSmartPointer<vtkSMProxy> ColorMap;
  DataSource::DataSourceType Type;
  vtkSmartPointer<vtkDataArray> TiltAngles;
  vtkVector3d DisplayPosition;

  // Checks if the tilt angles data array exists on the given VTK data
  // and creates it if it does not exist.
  void ensureTiltAnglesArrayExists()
  {
    vtkAlgorithm* tp = vtkAlgorithm::SafeDownCast(
      this->Producer->GetClientSideObject());
    vtkDataObject* data = tp->GetOutputDataObject(0);
    vtkFieldData* fd = data->GetFieldData();
    if (!this->TiltAngles)
    {
      int* extent = vtkImageData::SafeDownCast(data)->GetExtent();
      int num_tilt_angles = extent[5] - extent[4] + 1;
      vtkNew< vtkDoubleArray > array;
      array->SetName("tilt_angles");
      array->SetNumberOfTuples(num_tilt_angles);
      array->FillComponent(0,0.0);
      if (!fd->HasArray("tilt_angles"))
      {
        fd->AddArray(array.GetPointer());
      }
      this->TiltAngles = array.Get();
    }
    else
    {
      if (!fd->HasArray("tilt_angles"))
      {
        fd->AddArray(this->TiltAngles);
      }
    }
  }
};

namespace {

// Converts the data type to a string for writing to the save state
const char* dataSourceTypeToString(DataSource::DataSourceType type)
{
  switch (type)
  {
  case DataSource::Volume:
    return "volume";
  case DataSource::TiltSeries:
    return "tilt-series";
  default:
    assert("Unhandled data source type" && false);
    return "";
  }
}

// Converts the save state string back to a DataSource::DataSourceType
// Returns true if the type was successfully converted, false otherwise
// the result is stored in the output paremeter type.
bool stringToDataSourceType(const char* str, DataSource::DataSourceType& type)
{
  if (strcmp(str, "volume") == 0)
  {
    type = DataSource::Volume;
    return true;
  }
  else if (strcmp(str, "tilt-series") == 0)
  {
    type = DataSource::TiltSeries;
    return true;
  }
  return false;
}

void serializeDataArray(pugi::xml_node& ns, vtkDataArray* array)
{
  ns.append_attribute("components").set_value(array->GetNumberOfComponents());
  ns.append_attribute("tuples").set_value((int)array->GetNumberOfTuples());
  std::ostringstream stream;
  for (int i = 0; i < array->GetNumberOfTuples(); ++i)
  {
    double* tuple = array->GetTuple(i);
    for (int j = 0; j < array->GetNumberOfComponents(); ++j)
    {
      stream << tuple[j] << " ";
    }
    stream << "\n";
  }
  pugi::xml_text text = ns.text();
  text.set(stream.str().c_str());
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
  for (int i = 0; i < tuples; ++i)
  {
    for (int j = 0; j < components; ++j)
    {
      stream >> data[j];
    }
    array->SetTuple(i, data);
  }
  delete[] data;
}

}

DataSource::DataSource(vtkSMSourceProxy* dataSource, DataSourceType dataType,
                      QObject* parentObject)
  : Superclass(parentObject),
  Internals(new DataSource::DSInternals())
{
  Q_ASSERT(dataSource);
  this->Internals->OriginalDataSource = dataSource;
  this->Internals->Type = dataType;
  for (int i = 0; i < 3; ++i)
  {
    this->Internals->DisplayPosition[i] = 0.0;
  }

  vtkNew<vtkSMParaViewPipelineController> controller;
  vtkSMSessionProxyManager* pxm = dataSource->GetSessionProxyManager();
  Q_ASSERT(pxm);

  vtkSmartPointer<vtkSMProxy> source;
  source.TakeReference(pxm->NewProxy("sources", "TrivialProducer"));
  Q_ASSERT(source != nullptr);
  Q_ASSERT(vtkSMSourceProxy::SafeDownCast(source));

  // We add an annotation to the proxy so that it'll be easier for code to
  // locate registered pipeline proxies that are being treated as data sources.
  const char* sourceFilename = nullptr;

  if (vtkSMCoreUtilities::GetFileNameProperty(dataSource) != nullptr)
  {
    sourceFilename =
      vtkSMPropertyHelper(dataSource,
                          vtkSMCoreUtilities::GetFileNameProperty(dataSource)).GetAsString();
  }
  if (sourceFilename && strlen(sourceFilename))
  {
    tomviz::annotateDataProducer(source, sourceFilename);
  }
  else if (dataSource->HasAnnotation("filename"))
  {
    tomviz::annotateDataProducer(source, dataSource->GetAnnotation("filename"));
  }
  else
  {
    tomviz::annotateDataProducer(source, "No filename");
  }

  controller->RegisterPipelineProxy(source);
  this->Internals->Producer = vtkSMSourceProxy::SafeDownCast(source);

  // Setup color map for this data-source.
  static unsigned int colorMapCounter=0;
  colorMapCounter++;

  vtkNew<vtkSMTransferFunctionManager> tfmgr;
  this->Internals->ColorMap = tfmgr->GetColorTransferFunction(
    QString("DataSourceColorMap%1").arg(colorMapCounter).toLatin1().data(), pxm);

  // every time the data changes, we should update the color map.
  this->connect(this, SIGNAL(dataChanged()), SLOT(updateColorMap()));

  this->resetData();
}

DataSource::~DataSource()
{
  if (this->Internals->Producer)
  {
    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->UnRegisterProxy(this->Internals->Producer);
  }
}

QString DataSource::filename() const
{
  vtkSMProxy* dataSource = this->originalDataSource();
  if (vtkSMCoreUtilities::GetFileNameProperty(dataSource) != nullptr)
  {
    return vtkSMPropertyHelper(dataSource,
      vtkSMCoreUtilities::GetFileNameProperty(dataSource)).GetAsString();
  }
  else
  {
    return dataSource->GetAnnotation("filename");
  }
}

bool DataSource::serialize(pugi::xml_node& ns) const
{
  pugi::xml_node node = ns.append_child("ColorMap");
  tomviz::serialize(this->colorMap(), node);

  node = ns.append_child("OpacityMap");
  tomviz::serialize(this->opacityMap(), node);

  ns.append_attribute("number_of_operators").set_value(
    static_cast<int>(this->Internals->Operators.size()));

  ns.append_attribute("type").set_value(
    dataSourceTypeToString(this->type()));
  if (this->type() == TiltSeries)
  {
    vtkDataArray* tiltAngles = this->Internals->TiltAngles;
    node = ns.append_child("TiltAngles");
    serializeDataArray(node, tiltAngles);
  }

  foreach (QSharedPointer<Operator> op, this->Internals->Operators)
  {
    pugi::xml_node operatorNode = ns.append_child("Operator");
    operatorNode.append_attribute("operator_type").set_value(
        OperatorFactory::operatorType(op.data()));
    if (!op->serialize(operatorNode))
    {
      qWarning("failed to serialize Operator. Skipping it.");
      ns.remove_child(operatorNode);
    }
  }
  return true;
}

bool DataSource::deserialize(const pugi::xml_node& ns)
{
  tomviz::deserialize(this->colorMap(), ns.child("ColorMap"));
  tomviz::deserialize(this->opacityMap(), ns.child("OpacityMap"));
  vtkSMPropertyHelper(this->colorMap(),
                      "ScalarOpacityFunction").Set(this->opacityMap());
  this->colorMap()->UpdateVTKObjects();

  DataSourceType dstype;
  if (!stringToDataSourceType(ns.attribute("type").value(),dstype))
  {
    return false;
  }
  this->setType(dstype);

  int num_operators = ns.attribute("number_of_operators").as_int(-1);
  if (num_operators < 0)
  {
    return false;
  }

  this->Internals->Operators.clear();
  this->resetData();

  // load tilt angles AFTER resetData call.
  if (this->type() == TiltSeries)
  {
    deserializeDataArray(ns.child("TiltAngles"), this->Internals->TiltAngles);
  }

  for (pugi::xml_node node=ns.child("Operator"); node;
       node = node.next_sibling("Operator"))
  {
    QSharedPointer<Operator> op(OperatorFactory::createOperator(
          node.attribute("operator_type").value(), this));
    if (op)
    {
      if (op->deserialize(node))
      {
        this->addOperator(op);
      }
    }
  }
  return true;
}

DataSource* DataSource::clone(bool cloneOperators, bool cloneTransformed) const
{
  DataSource *newClone = nullptr;
  if (cloneTransformed)
  {
    if (vtkSMCoreUtilities::GetFileNameProperty(this->Internals->OriginalDataSource) != nullptr)
    {
      const char* originalFilename =
          vtkSMPropertyHelper(this->Internals->OriginalDataSource,
                              vtkSMCoreUtilities::GetFileNameProperty(
                               this->Internals->OriginalDataSource)).GetAsString();
      this->Internals->Producer->SetAnnotation("filename", originalFilename);
    }
    else
    {
      this->Internals->Producer->SetAnnotation(
          "filename", this->originalDataSource()->GetAnnotation("filename"));
    }
    newClone = new DataSource(this->Internals->Producer, this->Internals->Type);
  }
  else
  {
    newClone = new DataSource(this->Internals->OriginalDataSource,
                              this->Internals->Type);
  }
  if (this->Internals->Type == TiltSeries)
  {
    newClone->setTiltAngles(this->getTiltAngles(!cloneTransformed));
  }
  if (!cloneTransformed && cloneOperators)
  {
    // now, clone the operators.
    foreach (QSharedPointer<Operator> op, this->Internals->Operators)
    {
      QSharedPointer<Operator> opClone(op->clone());
      newClone->addOperator(opClone);
    }
    newClone->setTiltAngles(this->getTiltAngles());
  }
  return newClone;
}

vtkSMSourceProxy* DataSource::originalDataSource() const
{
  return this->Internals->OriginalDataSource;
}

vtkSMSourceProxy* DataSource::producer() const
{
  return this->Internals->Producer;
}

int DataSource::addOperator(QSharedPointer<Operator>& op)
{
  int index = this->Internals->Operators.count();
  this->Internals->Operators.push_back(op);
  this->connect(op.data(), SIGNAL(transformModified()),
                SLOT(operatorTransformModified()));
  emit this->operatorAdded(op.data());
  emit this->operatorAdded(op);
  this->operate(op.data());
  return index;
}

bool DataSource::removeOperator(const QSharedPointer<Operator>& op)
{
  if (op)
  {
    // We should emit that the operator was removed...
    this->Internals->Operators.removeAll(op);
    this->operatorTransformModified();
    foreach (QSharedPointer<Operator> opPtr, this->Internals->Operators)
    {
      cout << "Operator: " << opPtr->label().toLatin1().data() << endl;
    }

    return true;
  }
  return false;
}

void DataSource::operate(Operator* op)
{
  Q_ASSERT(op);

  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    this->Internals->Producer->GetClientSideObject());
  Q_ASSERT(tp);
  if (op->transform(tp->GetOutputDataObject(0)))
  {
    this->dataModified();
  }

  emit this->dataChanged();
}

vtkSmartPointer<vtkImageData> DataSource::getCopyOfImagePriorTo(QSharedPointer<Operator>& op)
{
  vtkSmartPointer<vtkImageData> result = vtkSmartPointer<vtkImageData>::New();
  if (this->Internals->Operators.contains(op))
  {
    vtkAlgorithm* alg = vtkAlgorithm::SafeDownCast(
      this->Internals->OriginalDataSource->GetClientSideObject());
    result->DeepCopy(alg->GetOutputDataObject(0));
    for (int i = 0; i < this->Internals->Operators.size() && this->Internals->Operators[i] != op; ++i)
    {
      this->Internals->Operators[i]->transform(result);
    }
  }
  else
  {
    vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
      this->Internals->Producer->GetClientSideObject());
    result->DeepCopy(tp->GetOutputDataObject(0));
  }
  return result;
}

void DataSource::dataModified()
{
  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    this->Internals->Producer->GetClientSideObject());
  Q_ASSERT(tp);
  tp->Modified();
  vtkDataObject *dObject = tp->GetOutputDataObject(0);
  dObject->Modified();
  this->Internals->Producer->MarkModified(nullptr);

  vtkFieldData *fd = dObject->GetFieldData();
  if (fd->HasArray("tomviz_data_source_type"))
  {
    vtkTypeInt8Array *typeArray = vtkTypeInt8Array::SafeDownCast(
        fd->GetArray("tomviz_data_source_type"));

    // Casting is a bit hacky here, but it *should* work
    this->setType((DataSourceType)(int)typeArray->GetTuple1(0));
  }
  else
  {
    vtkSmartPointer< vtkTypeInt8Array> typeArray =
      vtkSmartPointer<vtkTypeInt8Array>::New();
    typeArray->SetNumberOfComponents(1);
    typeArray->SetNumberOfTuples(1);
    typeArray->SetName("tomviz_data_source_type");
    typeArray->SetTuple1(0,this->Internals->Type);
    fd->AddArray(typeArray);
  }

  // This indirection is necessary to overcome a bug in VTK/ParaView when
  // explicitly calling UpdatePipeline(). The extents don't reset to the whole
  // extent. Until a  proper fix makes it into VTK, this is needed.
  vtkSMSessionProxyManager* pxm =
      this->Internals->Producer->GetSessionProxyManager();
  vtkSMSourceProxy* filter =
    vtkSMSourceProxy::SafeDownCast(pxm->NewProxy("filters", "PassThrough"));
  Q_ASSERT(filter);
  vtkSMPropertyHelper(filter, "Input").Set(this->Internals->Producer, 0);
  filter->UpdateVTKObjects();
  filter->UpdatePipeline();
  filter->Delete();

  emit this->dataChanged();
}

const QList<QSharedPointer<Operator> >& DataSource::operators() const
{
  return this->Internals->Operators;
}

void DataSource::translate(const double deltaPosition[3])
{
  for (int i = 0; i < 3; ++i)
  {
    this->Internals->DisplayPosition[i] += deltaPosition[i];
  }
  emit displayPositionChanged(this->Internals->DisplayPosition[0],
                              this->Internals->DisplayPosition[1],
                              this->Internals->DisplayPosition[2]);
}

const double *DataSource::displayPosition()
{
  return this->Internals->DisplayPosition.GetData();
}

void DataSource::setDisplayPosition(const double newPosition[3])
{
  for (int i = 0; i < 3; ++i)
  {
    this->Internals->DisplayPosition[i] = newPosition[i];
  }
  emit displayPositionChanged(this->Internals->DisplayPosition[0],
                              this->Internals->DisplayPosition[1],
                              this->Internals->DisplayPosition[2]);
}

void DataSource::resetData()
{
  vtkSMSourceProxy* dataSource = this->Internals->OriginalDataSource;
  Q_ASSERT(dataSource);

  dataSource->UpdatePipeline();
  vtkAlgorithm* vtkalgorithm = vtkAlgorithm::SafeDownCast(
    dataSource->GetClientSideObject());
  Q_ASSERT(vtkalgorithm);

  vtkSMSourceProxy* source = this->Internals->Producer;
  Q_ASSERT(source != nullptr);

  // Create a clone and release the reader data.
  vtkDataObject* data = vtkalgorithm->GetOutputDataObject(0);
  vtkDataObject* dataClone = data->NewInstance();
  dataClone->DeepCopy(data);
  //data->ReleaseData();  FIXME: how it this supposed to work? I get errors on
  //attempting to re-execute the reader pipeline in clone().

  vtkTrivialProducer* tp = vtkTrivialProducer::SafeDownCast(
    source->GetClientSideObject());
  Q_ASSERT(tp);
  tp->SetOutput(dataClone);
  dataClone->FastDelete();
  if (this->Internals->Type == TiltSeries)
  {
    this->Internals->ensureTiltAnglesArrayExists();
  }
  vtkFieldData *fd = dataClone->GetFieldData();
  vtkSmartPointer<vtkTypeInt8Array> typeArray = vtkTypeInt8Array::SafeDownCast(
      fd->GetArray("tomviz_data_source_type"));
  if (!typeArray)
  {
    typeArray = vtkSmartPointer<vtkTypeInt8Array>::New();
    typeArray->SetNumberOfComponents(1);
    typeArray->SetNumberOfTuples(1);
    typeArray->SetName("tomviz_data_source_type");
    fd->AddArray(typeArray);
  }
  typeArray->SetTuple1(0,this->Internals->Type);
  emit this->dataChanged();
}

void DataSource::operatorTransformModified()
{
  bool prev = this->blockSignals(true);

  this->resetData();
  foreach (QSharedPointer<Operator> op, this->Internals->Operators)
  {
    this->operate(op.data());
  }
  this->blockSignals(prev);
  this->dataModified();
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
  vtkTrivialProducer *tp = vtkTrivialProducer::SafeDownCast(
      this->Internals->Producer->GetClientSideObject());
  vtkDataObject *data = tp->GetOutputDataObject(0);
  vtkFieldData *fd = data->GetFieldData();
  vtkTypeInt8Array *typeArray = vtkTypeInt8Array::SafeDownCast(
      fd->GetArray("tomviz_data_source_type"));
  assert(typeArray);
  typeArray->SetTuple1(0,t);
  if (t == TiltSeries)
  {
    this->Internals->ensureTiltAnglesArrayExists();
  }
  emit this->dataChanged();
}

bool DataSource::hasTiltAngles()
{
  vtkDataArray* tiltAngles = this->Internals->TiltAngles;
  return tiltAngles != nullptr;
}

QVector<double> DataSource::getTiltAngles(bool useOriginalDataTiltAngles) const
{
  QVector<double> result;
  vtkAlgorithm* tp = vtkAlgorithm::SafeDownCast(
    this->Internals->Producer->GetClientSideObject());
  vtkDataObject* data = tp->GetOutputDataObject(0);
  vtkFieldData *fd = data->GetFieldData();
  vtkDataArray* tiltAngles = this->Internals->TiltAngles;
  if (fd->HasArray("tilt_angles") && !useOriginalDataTiltAngles &&
        fd->GetArray("tilt_angles") != this->Internals->TiltAngles.Get())
  {
    tiltAngles = fd->GetArray("tilt_angles");
  }
  if (tiltAngles)
  {
    result.resize(tiltAngles->GetNumberOfTuples());
    for (int i = 0; i < result.size(); ++i)
    {
      result[i] = tiltAngles->GetTuple1(i);
    }
  }
  return result;
}

void DataSource::setTiltAngles(const QVector<double> &angles)
{
  vtkDataArray* tiltAngles = this->Internals->TiltAngles;
  vtkAlgorithm* tp = vtkAlgorithm::SafeDownCast(
    this->Internals->Producer->GetClientSideObject());
  vtkDataObject* data = tp->GetOutputDataObject(0);
  vtkFieldData *fd = data->GetFieldData();
  if (fd->GetArray("tilt_angles") != this->Internals->TiltAngles)
  {
    tiltAngles = fd->GetArray("tilt_angles");
  }
  if (tiltAngles)
  {
    for (int i = 0; i < tiltAngles->GetNumberOfTuples() && i < angles.size(); ++i)
    {
      tiltAngles->SetTuple1(i, angles[i]);
    }
  }
  emit this->dataChanged();
}

vtkSMProxy* DataSource::opacityMap() const
{
  return this->Internals->ColorMap?
  vtkSMPropertyHelper(this->Internals->ColorMap,
                      "ScalarOpacityFunction").GetAsProxy() : nullptr;
}

bool DataSource::hasLabelMap()
{
  vtkSMSourceProxy* dataSource = this->producer();
  if (!dataSource)
  {
    return false;
  }

  // We could just as easily go to the client side VTK object to get this info,
  // but we'll go the ParaView route for now.
  vtkPVDataInformation* dataInfo = dataSource->GetDataInformation();
  vtkPVDataSetAttributesInformation* pointDataInfo = dataInfo->GetPointDataInformation();
   vtkPVArrayInformation* labelMapInfo = pointDataInfo->GetArrayInformation("LabelMap");
 
  return labelMapInfo != nullptr;
}

void DataSource::updateColorMap()
{
  // rescale the color/opacity maps for the data source.
  tomviz::rescaleColorMap(this->colorMap(), this);
}

}
