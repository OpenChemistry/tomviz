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
    : tomviz::EditOperatorWidget(p), m_op(o), m_ui(), m_customWidget(customWidget),
      m_opWidget(nullptr)
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
}

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
  Python::Function FindTransformScalarsFunction;
  Python::Function IsCancelableFunction;
  Python::Function DeleteModuleFunction;
};

OperatorPython::OperatorPython(QObject* parentObject)
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

    d->FindTransformScalarsFunction =
      d->InternalModule.findFunction("find_transform_scalars");
    if (!d->FindTransformScalarsFunction.isValid()) {
      qCritical() << "Unable to locate find_transform_scalars.";
    }

    d->DeleteModuleFunction = d->InternalModule.findFunction("delete_module");
    if (!d->DeleteModuleFunction.isValid()) {
      qCritical() << "Unable to locate delete_module.";
    }
  }

  // Needed so the worker thread can update data in the UI thread.
  connect(this, SIGNAL(childDataSourceUpdated(vtkSmartPointer<vtkDataObject>)),
          this, SLOT(updateChildDataSource(vtkSmartPointer<vtkDataObject>)));

  // This connection is needed so we can create new child data sources in the UI
  // thread from a pipeline worker threads.
  connect(this, SIGNAL(newChildDataSource(const QString&,
                                          vtkSmartPointer<vtkDataObject>)),
          this, SLOT(createNewChildDataSource(const QString&,
                                              vtkSmartPointer<vtkDataObject>)));
  connect(
    this,
    SIGNAL(newOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>)),
    this,
    SLOT(setOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>)));
}

OperatorPython::~OperatorPython()
{
}

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
  m_childDataSourceNamesAndLabels.clear();

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
        QPair<QString, QString> nameLabelPair(QString(nameValue.toString()),
                                              QString(labelValue.toString()));
        m_childDataSourceNamesAndLabels.append(nameLabelPair);
      } else if (nameValue.isNull()) {
        qCritical() << "No name given for child DataSet";
      } else if (labelValue.isNull()) {
        qCritical() << "No label given for child DataSet";
      }
    }
  }
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

      d->TransformMethod = d->FindTransformScalarsFunction.call(findArgs);
      if (!d->TransformMethod.isValid()) {
        qCritical("Script doesn't have any 'transform_scalars' function.");
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

bool OperatorPython::applyTransform(vtkDataObject* data)
{
  if (m_script.isEmpty()) {
    return false;
  }
  if (!d->OperatorModule.isValid() || !d->TransformMethod.isValid()) {
    return false;
  }

  Q_ASSERT(data);

  Python::Object pydata = Python::VTK::GetObjectFromPointer(data);

  Python::Object result;
  {
    Python python;

    // Create child datasets in advance.
    for (int i = 0; i < m_childDataSourceNamesAndLabels.size(); ++i) {
      QPair<QString, QString> nameLabelPair =
        m_childDataSourceNamesAndLabels[i];
      QString name(nameLabelPair.first);
      QString label(nameLabelPair.second);

      // Create uninitialized data set as a placeholder for the data
      vtkSmartPointer<vtkImageData> childData = vtkSmartPointer<vtkImageData>::New();
      childData->DeepCopy(data);
      Q_ASSERT(childData->GetPointData()->GetScalars());

      if (childData) {
        emit newChildDataSource(label, childData);
      }
    }

    Python::Tuple args(1);
    args.set(0, pydata);

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
    // Results (tables, etc.)
    for (int i = 0; i < m_resultNames.size(); ++i) {
      Python::Object pyDataObject;
      pyDataObject = outputDict[m_resultNames[i]];

      if (!pyDataObject.isValid()) {
        errorEncountered = true;
        qCritical() << "No result named" << m_resultNames[i]
                    << "defined in output dictionary.\n";
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
  OperatorPython* newClone = new OperatorPython();
  newClone->setLabel(label());
  newClone->setScript(script());
  newClone->setJSONDescription(JSONDescription());
  return newClone;
}

bool OperatorPython::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("json_description")
    .set_value(JSONDescription().toLatin1().data());
  ns.append_attribute("label").set_value(label().toLatin1().data());
  ns.append_attribute("script").set_value(script().toLatin1().data());
  pugi::xml_node argsNode = ns.append_child("arguments");
  return tomviz::serialize(m_arguments, argsNode);
}

bool OperatorPython::deserialize(const pugi::xml_node& ns)
{
  setJSONDescription(ns.attribute("json_description").as_string());
  setLabel(ns.attribute("label").as_string());
  setScript(ns.attribute("script").as_string());
  m_arguments.clear();
  return tomviz::deserialize(m_arguments, ns.child("arguments"));
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

void OperatorPython::createNewChildDataSource(
  const QString& label, vtkSmartPointer<vtkDataObject> childData)
{
  DataSource* childDS = new DataSource(
    vtkImageData::SafeDownCast(childData), DataSource::Volume, this,
    DataSource::PersistenceState::Transient);

  childDS->setFileName(label);
  setChildDataSource(childDS);
  emit Operator::newChildDataSource(childDS);
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
}
#include "OperatorPython.moc"
