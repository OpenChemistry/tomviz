/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorPython.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QtDebug>

#include "ActiveObjects.h"
#include "CustomPythonOperatorWidget.h"
#include "DataSource.h"
#include "EditOperatorWidget.h"
#include "OperatorResult.h"
#include "OperatorWidget.h"
#include "Pipeline.h"
#include "PythonUtilities.h"
#include "Utilities.h"
#include "pqPythonSyntaxHighlighter.h"

#include "vtkDataObject.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMProxy.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkTrivialProducer.h"

#include "ui_EditPythonOperatorWidget.h"

namespace {

class EditPythonOperatorWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT

public:
  EditPythonOperatorWidget(
    QWidget* p, tomviz::OperatorPython* o,
    tomviz::CustomPythonOperatorWidget* customWidget = nullptr)
    : tomviz::EditOperatorWidget(p), m_op(o), m_ui(),
      m_customWidget(customWidget), m_opWidget(nullptr)
  {
    m_ui.setupUi(this);
    m_ui.name->setText(o->label());
    if (!o->script().isEmpty()) {
      m_ui.script->setPlainText(o->script());
    }
    new pqPythonSyntaxHighlighter(m_ui.script, this);
    if (customWidget) {
      QVBoxLayout* layout = new QVBoxLayout();
      m_customWidget->setValues(m_op->arguments());
      layout->addWidget(m_customWidget);
      m_ui.argumentsWidget->setLayout(layout);
    } else {
      QVBoxLayout* layout = new QVBoxLayout();
      m_opWidget = new tomviz::OperatorWidget(this);
      m_opWidget->setupUI(m_op);
      layout->addWidget(m_opWidget);
      layout->addStretch();
      m_ui.argumentsWidget->setLayout(layout);
    }
  }
  void setViewMode(const QString& mode) override
  {
    if (mode == QStringLiteral("viewCode")) {
      m_ui.tabWidget->setCurrentWidget(m_ui.scriptTab);
    }
  }
  void applyChangesToOperator() override
  {
    if (m_op) {
      m_op->setLabel(m_ui.name->text());
      m_op->setScript(m_ui.script->toPlainText());
      if (m_customWidget) {
        QMap<QString, QVariant> args;
        m_customWidget->getValues(args);
        m_op->setArguments(args);
      } else if (m_opWidget) {
        QMap<QString, QVariant> args = m_opWidget->values();
        m_op->setArguments(args);
      }
    }
  }

private:
  QPointer<tomviz::OperatorPython> m_op;
  Ui::EditPythonOperatorWidget m_ui;
  tomviz::CustomPythonOperatorWidget* m_customWidget;
  tomviz::OperatorWidget* m_opWidget;
};

QMap<QString, QPair<bool, tomviz::OperatorPython::CustomWidgetFunction>>
  CustomWidgetMap;
} // namespace

namespace tomviz {

void OperatorPython::registerCustomWidget(const QString& key, bool needsData,
                                          CustomWidgetFunction func)
{
  CustomWidgetMap.insert(key,
                         QPair<bool, CustomWidgetFunction>(needsData, func));
}

class OperatorPython::OPInternals
{
public:
  Python::Module OperatorModule;
  Python::Module TransformModule;
  Python::Function TransformMethod;
  Python::Module InternalModule;
  Python::Function FindTransformFunction;
  Python::Function IsCancelableFunction;
  Python::Function DeleteModuleFunction;
};

OperatorPython::OperatorPython(DataSource* parentObject)
  : Operator(parentObject), d(new OperatorPython::OPInternals()),
    m_label("Python Operator")
{
  Python::initialize();

  {
    Python python;
    d->OperatorModule = python.import("tomviz.utils");
    if (!d->OperatorModule.isValid()) {
      qCritical() << "Failed to import tomviz.utils module.";
    }

    d->InternalModule = python.import("tomviz._internal");
    if (!d->InternalModule.isValid()) {
      qCritical() << "Failed to import tomviz._internal module.";
    }

    d->IsCancelableFunction = d->InternalModule.findFunction("is_cancelable");
    if (!d->IsCancelableFunction.isValid()) {
      qCritical() << "Unable to locate is_cancelable.";
    }

    d->FindTransformFunction =
      d->InternalModule.findFunction("find_transform_function");
    if (!d->FindTransformFunction.isValid()) {
      qCritical() << "Unable to locate find_transform_function.";
    }

    d->DeleteModuleFunction = d->InternalModule.findFunction("delete_module");
    if (!d->DeleteModuleFunction.isValid()) {
      qCritical() << "Unable to locate delete_module.";
    }
  }

  auto connectionType = Qt::BlockingQueuedConnection;
  if (dataSource() != nullptr && dataSource()->pipeline() != nullptr &&
      dataSource()->pipeline()->executionMode() ==
        Pipeline::ExecutionMode::Docker) {
    connectionType = Qt::DirectConnection;
  }
  // Needed so the worker thread can update data in the UI thread.
  connect(this, SIGNAL(childDataSourceUpdated(vtkSmartPointer<vtkDataObject>)),
          this, SLOT(updateChildDataSource(vtkSmartPointer<vtkDataObject>)),
          connectionType);

  // This connection is needed so we can create new child data sources in the UI
  // thread from a pipeline worker threads.
  connect(this, SIGNAL(newChildDataSource(const QString&,
                                          vtkSmartPointer<vtkDataObject>)),
          this, SLOT(createNewChildDataSource(const QString&,
                                              vtkSmartPointer<vtkDataObject>)),
          connectionType);
  connect(
    this,
    SIGNAL(newOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>)),
    this,
    SLOT(setOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>)));
}

OperatorPython::~OperatorPython() {}

void OperatorPython::setLabel(const QString& txt)
{
  m_label = txt;
  emit labelModified();
}

QIcon OperatorPython::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqProgrammableFilter24.png");
}

void OperatorPython::setJSONDescription(const QString& str)
{
  if (m_jsonDescription == str) {
    return;
  }

  m_jsonDescription = str;

  auto document = QJsonDocument::fromJson(m_jsonDescription.toLatin1());
  if (!document.isObject()) {
    qCritical() << "Failed to parse operator JSON";
    qCritical() << m_jsonDescription;
    return;
  }

  QJsonObject root = document.object();

  // Get the label for the operator
  QJsonValueRef labelNode = root["label"];
  if (!labelNode.isUndefined() && !labelNode.isNull()) {
    setLabel(labelNode.toString());
  }

  QJsonValueRef widgetNode = root["widget"];
  if (!widgetNode.isUndefined() && !widgetNode.isNull()) {
    m_customWidgetID = widgetNode.toString();
  }

  m_resultNames.clear();

  // Get the number of results
  QJsonValueRef resultsNode = root["results"];
  if (!resultsNode.isUndefined() && !resultsNode.isNull()) {
    QJsonArray resultsArray = resultsNode.toArray();
    QJsonObject::size_type numResults = resultsArray.size();
    setNumberOfResults(numResults);

    for (QJsonObject::size_type i = 0; i < numResults; ++i) {
      OperatorResult* oa = resultAt(i);
      if (!oa) {
        Q_ASSERT(oa != nullptr);
      }
      QJsonObject resultNode = resultsArray[i].toObject();
      QJsonValueRef nameValue = resultNode["name"];
      if (!nameValue.isUndefined() && !nameValue.isNull()) {
        oa->setName(nameValue.toString());
        m_resultNames.append(nameValue.toString());
      }
      QJsonValueRef labelValue = resultNode["label"];
      if (!labelValue.isUndefined() && !labelValue.isNull()) {
        oa->setLabel(labelValue.toString());
      }
    }
  }

  // Get the number of parameters
  QJsonValueRef parametersNode = root["parameters"];
  if (!parametersNode.isUndefined() && !parametersNode.isNull()) {
    QJsonArray parametersArray = parametersNode.toArray();
    QJsonObject::size_type numParameters = parametersArray.size();
    setNumberOfParameters(numParameters);
  }

  // Get child dataset information
  QJsonValueRef childDatasetNode = root["children"];
  if (!childDatasetNode.isUndefined() && !childDatasetNode.isNull()) {
    QJsonArray childDatasetArray = childDatasetNode.toArray();
    QJsonObject::size_type size = childDatasetArray.size();
    if (size != 1) {
      qCritical() << "Only one child dataset is supported for now. Found"
                  << size << " but only the first will be used";
    }
    if (size > 0) {
      setHasChildDataSource(true);
      QJsonObject dataSourceNode = childDatasetArray[0].toObject();
      QJsonValueRef nameValue = dataSourceNode["name"];
      QJsonValueRef labelValue = dataSourceNode["label"];
      if (!nameValue.isUndefined() && !nameValue.isNull() &&
          !labelValue.isUndefined() && !labelValue.isNull()) {
        m_childDataSourceName = nameValue.toString();
        m_childDataSourceLabel = labelValue.toString();
      } else if (nameValue.isNull()) {
        qCritical() << "No name given for child DataSet";
      } else if (labelValue.isNull()) {
        qCritical() << "No label given for child DataSet";
      }
    }
  }

  setHelpFromJson(root);
}

const QString& OperatorPython::JSONDescription() const
{
  return m_jsonDescription;
}

void OperatorPython::setScript(const QString& str)
{
  if (m_script != str) {
    m_script = str;

    Python::Object result;
    {
      Python python;
      QString moduleName = QString("tomviz_%1").arg(label());
      d->TransformModule = python.import(this->m_script, label(), moduleName);
      if (!d->TransformModule.isValid()) {
        qCritical("Failed to create module.");
        return;
      }

      // Delete the module from sys.module so we don't reuse it
      Python::Tuple delArgs(1);
      Python::Object name(moduleName);
      delArgs.set(0, name);
      auto delResult = d->DeleteModuleFunction.call(delArgs);
      if (!delResult.isValid()) {
        qCritical("An error occurred deleting module.");
        return;
      }

      // Create capsule to hold the pointer to the operator in the python world
      Python::Tuple findArgs(2);
      Python::Capsule op(this);

      findArgs.set(0, d->TransformModule);
      findArgs.set(1, op);

      d->TransformMethod = d->FindTransformFunction.call(findArgs);
      if (!d->TransformMethod.isValid()) {
        qCritical("Script doesn't have any 'transform' function.");
        return;
      }

      Python::Tuple isArgs(1);
      isArgs.set(0, d->TransformModule);

      result = d->IsCancelableFunction.call(isArgs);
      if (!result.isValid()) {
        qCritical("Error calling is_cancelable.");
        return;
      }
    }

    setSupportsCancel(result.toBool());

    emit transformModified();
  }
}

void OperatorPython::createChildDataSource()
{
  // Create child datasets in advance. Keep a map from DataSource to name
  // so that we can match Python script return dictionary values containing
  // child data after the script finishes.
  if (hasChildDataSource() && !childDataSource()) {
    // Create uninitialized data set as a placeholder for the data
    vtkSmartPointer<vtkImageData> childData =
      vtkSmartPointer<vtkImageData>::New();
    childData->ShallowCopy(
      vtkImageData::SafeDownCast(dataSource()->dataObject()));
    emit newChildDataSource(m_childDataSourceLabel, childData);
    }
}

bool OperatorPython::updateChildDataSource(Python::Dict outputDict)
{
  if (hasChildDataSource()) {
    Python::Object pyDataObject;
    pyDataObject = outputDict[m_childDataSourceName];
    if (!pyDataObject.isValid()) {
      qCritical() << "No child dataset named " << m_childDataSourceName
                  << "defined in dictionary returned from Python script.\n";
      return false;
    } else {
      vtkObjectBase* vtkobject = Python::VTK::convertToDataObject(pyDataObject);
      vtkDataObject* dataObject = vtkDataObject::SafeDownCast(vtkobject);
      if (dataObject) {
        TemporarilyReleaseGil releaseMe;
        emit childDataSourceUpdated(dataObject);
      }
    }
  }

  return true;
}

bool OperatorPython::updateChildDataSource(
  QMap<QString, vtkSmartPointer<vtkDataObject>> output)
{
  if (hasChildDataSource()) {
    for (QMap<QString, vtkSmartPointer<vtkDataObject>>::const_iterator iter =
           output.begin();
         iter != output.end(); ++iter) {

      if (iter.key() != m_childDataSourceName) {
        qCritical() << "No child dataset named " << m_childDataSourceName
                    << "defined in dictionary returned from Python script.\n";
        return false;
      }

      emit childDataSourceUpdated(iter.value());
    }
  }

  return true;
}

bool OperatorPython::applyTransform(vtkDataObject* data)
{
  if (m_script.isEmpty()) {
    return false;
  }
  if (!d->OperatorModule.isValid() || !d->TransformMethod.isValid()) {
    return false;
  }

  Q_ASSERT(data);

  createChildDataSource();

  Python::Object result;
  {
    Python python;

    Python::Tuple args(1);

    // Get the name of the function
    auto name = d->TransformMethod.getAttr("__name__").toString();
    if (name == "transform_scalars") {
      // Use the arguments for transform_scalars()
      Python::Object pydata = Python::VTK::GetObjectFromPointer(data);
      args.set(0, pydata);
    } else if (name == "transform") {
      // Use the arguments for transform()
      Python::Object pydata = Python::createDataset(data, *dataSource());
      args.set(0, pydata);
    } else {
      qDebug() << "Unknown TransformMethod name: " << name;
      return false;
    }

    Python::Dict kwargs;
    foreach (QString key, m_arguments.keys()) {
      Variant value = toVariant(m_arguments[key]);
      kwargs.set(key, value);
    }

    result = d->TransformMethod.call(args, kwargs);
    if (!result.isValid()) {
      qCritical("Failed to execute the script.");
      return false;
    }
  }

  // Look for additional outputs from the filter returned in a dictionary
  int check = 0;
  {
    Python python;
    check = result.isDict();
  }

  bool errorEncountered = false;
  if (check) {
    Python python;
    Python::Dict outputDict = result.toDict();

    // Support setting child data from the output dictionary
    errorEncountered = !updateChildDataSource(outputDict);

    // Results (tables, etc.)
    for (int i = 0; i < m_resultNames.size(); ++i) {
      Python::Object pyDataObject;
      pyDataObject = outputDict[m_resultNames[i]];

      if (!pyDataObject.isValid()) {
        errorEncountered = true;
        qCritical() << "No result named" << m_resultNames[i]
                    << "defined in dictionary returned from Python script.\n";
        continue;
      }
      vtkObjectBase* vtkobject =
        Python::VTK::GetPointerFromObject(pyDataObject, "vtkDataObject");
      vtkDataObject* dataObject = vtkDataObject::SafeDownCast(vtkobject);
      if (dataObject) {
        // Emit signal so we switch back to UI thread
        emit newOperatorResult(m_resultNames[i], dataObject);
      } else {
        qCritical() << "Result named '" << m_resultNames[i]
                    << "' is not a vtkDataObject";
        continue;
      }
    }

    if (errorEncountered) {

      qCritical() << "Dictionary return from Python script is:\n"
                  << outputDict.toString();
    }
  }

  return !errorEncountered;
}

Operator* OperatorPython::clone() const
{
  OperatorPython* newClone =
    new OperatorPython(qobject_cast<DataSource*>(parent()));
  newClone->setLabel(label());
  newClone->setScript(script());
  newClone->setJSONDescription(JSONDescription());
  return newClone;
}

QJsonObject OperatorPython::serialize() const
{
  auto json = Operator::serialize();
  json["description"] = JSONDescription();
  json["label"] = label();
  json["script"] = script();
  if (!m_arguments.isEmpty()) {
    json["arguments"] = QJsonObject::fromVariantMap(m_arguments);
    // If we have no description we still need to save the types of
    // the arguments
    if (JSONDescription().isEmpty() && !m_typeInfo.isEmpty()) {
      QJsonObject typeObj;
      for (auto itr = m_typeInfo.begin(); itr != m_typeInfo.end(); ++itr) {
        typeObj[itr.key()] = itr.value();
      }
      json["argumentTypeInformation"] = typeObj;
    }
  }

  if (!helpUrl().isEmpty()) {
    json["help"] = QJsonObject();
    json["help"].toObject()["url"] = helpUrl();
  }

  return json;
}

namespace {
QJsonObject findJsonObject(const QJsonArray& array, const QString& name)
{
  for (int i = 0; i < array.size(); ++i) {
    auto object = array[i].toObject();
    if (object["name"].toString() == name) {
      return object;
    }
  }
  return QJsonObject();
}

QVariant castJsonArg(const QJsonValue& arg, const QString& type)
{
  if (arg.isArray()) {
    auto arr = arg.toArray();
    QVariantList arrayList;
    if (type == "int" || type == "enumeration") {
      for (int i = 0; i < arr.size(); ++i) {
        arrayList << arr[i].toInt();
      }
    } else if (type == "double") {
      for (int i = 0; i < arr.size(); ++i) {
        arrayList << arr[i].toDouble();
      }
    }
    return arrayList;
  } else if (arg.isDouble()) {
    QVariant variant;
    if (type == "int" || type == "enumeration") {
      variant = arg.toInt();
    } else if (type == "double") {
      variant = arg.toDouble();
    }
    return variant;
  }
  return QVariant();
}
} // namespace

bool OperatorPython::deserialize(const QJsonObject& json)
{
  setJSONDescription(json["description"].toString());
  setLabel(json["label"].toString());
  setScript(json["script"].toString());
  m_arguments.clear();
  // We use the JSON description to ensure things have the correct type.
  if (json.contains("arguments")) {
    if (!m_jsonDescription.isEmpty()) {
      auto document = QJsonDocument::fromJson(m_jsonDescription.toLatin1());
      if (!document.isObject()) {
        qCritical() << "Failed to parse operator JSON";
        qCritical() << m_jsonDescription;
        return false;
      }
      auto args = json["arguments"].toObject();
      auto params = document.object()["parameters"].toArray();
      foreach (const QString& key, args.keys()) {
        auto param = findJsonObject(params, key);
        if (!param.isEmpty()) {
          m_arguments[key] = castJsonArg(args[key], param["type"].toString());
        }
      }
    } else if (json.contains("argumentTypeInformation")) {
      auto args = json["arguments"].toObject();
      auto typeInfo = json["argumentTypeInformation"].toObject();
      foreach (const QString& key, args.keys()) {
        if (!typeInfo.contains(key)) {
          qCritical() << "Deserializing operator " << m_label
                      << " found argument " << key << " with unknown type.";
          return false;
        }
        auto type = typeInfo[key].toString();
        m_arguments[key] = castJsonArg(args[key], type);
      }
    }
  }

  setHelpFromJson(json);
  return true;
}

EditOperatorWidget* OperatorPython::getEditorContents(QWidget* p)
{
  CustomPythonOperatorWidget* widget = nullptr;
  if (m_customWidgetID != "" && CustomWidgetMap.contains(m_customWidgetID)) {
    if (!CustomWidgetMap[m_customWidgetID].first) {
      vtkSmartPointer<vtkImageData> nullPtr;
      widget = CustomWidgetMap[m_customWidgetID].second(p, this, nullPtr);
    } else {
      // return nullptr so the caller knows to get the input data and
      // use the getEditorContentsWithData method
      return nullptr;
    }
  }
  return new EditPythonOperatorWidget(p, this, widget);
}

EditOperatorWidget* OperatorPython::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> displayImage)
{
  // Should only be called if there is a custom widget that needs input data
  Q_ASSERT(m_customWidgetID != "");
  Q_ASSERT(CustomWidgetMap[m_customWidgetID].first);
  auto widget = CustomWidgetMap[m_customWidgetID].second(p, this, displayImage);
  return new EditPythonOperatorWidget(p, this, widget);
}

void OperatorPython::updateChildDataSource(vtkSmartPointer<vtkDataObject> data)
{
  // Check to see if a child data source has already been created. If not,
  // create it here.
  auto dataSource = childDataSource();
  Q_ASSERT(dataSource);

  // Now deep copy the new data to the child source data if needed
  dataSource->copyData(data);
  emit dataSource->dataChanged();
  emit dataSource->dataPropertiesChanged();
  ActiveObjects::instance().renderAllViews();
}

void OperatorPython::setOperatorResult(const QString& name,
                                       vtkSmartPointer<vtkDataObject> result)
{
  bool resultWasSet = setResult(name.toLatin1().data(), result);
  if (!resultWasSet) {
    qCritical() << "Could not set result '" << name << "'";
  }
}

void OperatorPython::setArguments(QMap<QString, QVariant> args)
{
  if (args != m_arguments) {
    m_arguments = args;
    emit transformModified();
  }
}

QMap<QString, QVariant> OperatorPython::arguments() const
{
  return m_arguments;
}

void OperatorPython::setTypeInfo(const QMap<QString, QString>& typeInfo)
{
  m_typeInfo = typeInfo;
}

const QMap<QString, QString>& OperatorPython::typeInfo() const
{
  return m_typeInfo;
}

void OperatorPython::setHelpFromJson(const QJsonObject& json)
{
  // Clear before trying to read
  setHelpUrl("");
  auto helpNode = json["help"];
  if (!helpNode.isUndefined() && !helpNode.isNull()) {
    auto helpNodeUrl = helpNode.toObject()["url"];
    if (!helpNodeUrl.isUndefined() && !helpNodeUrl.isNull()) {
      setHelpUrl(helpNodeUrl.toString());
    }
  }
}

void OperatorPython::setChildDataSource(DataSource* source)
{
  if (source != nullptr) {
    source->setLabel(m_childDataSourceLabel);
  }
  Operator::setChildDataSource(source);
}

} // namespace tomviz
#include "OperatorPython.moc"
