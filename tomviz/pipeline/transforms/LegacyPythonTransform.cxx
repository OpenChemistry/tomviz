/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LegacyPythonTransform.h"

#include "CustomPythonNodeWidget.h"
#include "ExternalNodeExecutor.h"
#include "InputPort.h"
#include "NodeDefinitionValidator.h"
#include "OutputPort.h"
#include "ParameterBindingUtils.h"
#include "PythonNodeUtils.h"
#include "PythonNodeWrapper.h"
#include "VolumeOutputPort.h"
#include "PythonNodeEditorWidget.h"
#include "NodePropertiesWidget.h"
#include "data/LabelMapData.h"
#include "data/VolumeData.h"

// Qt defines 'slots' as a macro which conflicts with Python's object.h.
// We must undef it before including any pybind11/Python headers.
#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "pybind11/PybindVTKTypeCaster.h"
#pragma pop_macro("slots")

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPythonUtil.h>
#include <vtkTable.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

PYBIND11_VTK_TYPECASTER(vtkImageData)

namespace py = pybind11;

namespace tomviz {
namespace pipeline {

QString LegacyPythonTransform::customWidgetID() const
{
  return m_customWidgetID;
}

LegacyPythonTransform::LegacyPythonTransform(QObject* parent)
  : TransformNode(parent)
{
  // Always have volume in/out.  Use VolumeOutputPort so that
  // setIntermediateData() can deep-copy live updates from Python.
  // Going through addOutputPort(OutputPort*) bypasses
  // TransformNode::addOutput, so apply the persistence default
  // manually — otherwise legacy operators (GaussianFilter et al.)
  // ignore the application-wide setting.
  addInput("volume", PortType::ImageData);
  auto* volumeOutput =
    new VolumeOutputPort("volume", PortType::ImageData);
  addOutputPort(volumeOutput);
  applyDefaultPersistence(volumeOutput);
}

void LegacyPythonTransform::setJSONDescription(const QString& json)
{
  m_jsonDescription = json;
  parseJSON();

  // If the JSON carries a tomviz_pipeline_env key or declares
  // externalOnly and no executor is set, default to External execution.
  if (!nodeExecutor() && !json.isEmpty()) {
    QJsonDocument descDoc = QJsonDocument::fromJson(json.toUtf8());
    if (descDoc.isObject()) {
      auto obj = descDoc.object();
      auto envPath =
        obj.value(QStringLiteral("tomviz_pipeline_env")).toString();
      bool externalOnly =
        obj.value(QStringLiteral("externalOnly")).toBool(false);
      if (!envPath.isEmpty() || externalOnly) {
        setNodeExecutor(new ExternalNodeExecutor(envPath));
      }
    }
  }
}

QString LegacyPythonTransform::jsonDescription() const
{
  return m_jsonDescription;
}

QStringList LegacyPythonTransform::reconfigureDescription(const QString& json)
{
  auto previousValues = m_parameters;
  auto previousTypes = m_parameterTypes;

  m_jsonDescription = json;
  // parseJSON only ever writes over m_parameters, so clear it first or
  // parameters the new description drops would linger forever.
  m_parameters.clear();
  parseJSON(/*createPorts=*/false, /*applyLabel=*/false);

  QStringList reset;
  m_parameters =
    mergeParameterValues(previousValues, previousTypes, m_parameters,
                         m_parameterTypes, m_enumOptions, &reset);
  return reset;
}

void LegacyPythonTransform::setScript(const QString& script)
{
  m_script = script;

  // Detect cancel/completion support from operator base class usage.
  if (script.contains("CompletableOperator")) {
    setSupportsCancel(true);
    setSupportsCompletion(true);
  } else if (script.contains("CancelableOperator")) {
    setSupportsCancel(true);
    setSupportsCompletion(false);
  } else {
    setSupportsCancel(false);
    setSupportsCompletion(false);
  }
}

QString LegacyPythonTransform::scriptSource() const
{
  return m_script;
}

void LegacyPythonTransform::setParameter(const QString& name,
                                                  const QVariant& value)
{
  m_parameters[name] = value;
}

QVariant LegacyPythonTransform::parameter(const QString& name) const
{
  return m_parameters.value(name);
}

QMap<QString, QVariant> LegacyPythonTransform::parameters() const
{
  return m_parameters;
}

QString LegacyPythonTransform::operatorName() const
{
  return m_operatorName;
}

bool LegacyPythonTransform::hasPropertiesWidget() const
{
  // Always has a widget — at minimum the script editor tab.
  return true;
}

EditNodeWidget* LegacyPythonTransform::createPropertiesWidget(
  Pipeline* pipeline, QWidget* parent)
{
  // Build a factory closure rather than the widget directly — the
  // PythonNodeEditorWidget calls it (a) immediately if the custom
  // widget doesn't need data, or (b) later, once inputs have
  // materialized, if it does. collectInputs() runs at call time so it
  // sees the freshest data either way.
  PythonNodeEditorWidget::CustomWidgetFactory factory;
  bool customNeedsData = false;
  const CustomWidgetInfo* info = m_customWidgetID.isEmpty()
                                   ? nullptr
                                   : findCustomNodeWidget(m_customWidgetID);
  if (info && info->create) {
    customNeedsData = info->needsData;
    factory = [this, info, pipeline](QWidget* p) -> CustomPythonNodeWidget* {
      auto* w = info->create(collectInputs(), p);
      if (w) {
        w->setScript(m_script);
        w->setNodeContext(this, pipeline);
      }
      return w;
    };
  }

  // Seed the Execution tab from the current per-node executor (if any).
  // Only ExternalNodeExecutor is exposed in the UI today; any other
  // future type would round-trip through the JSON but would show as
  // "Internal" in the dropdown until the UI grows support for it.
  QString currentType;
  QString currentEnvPath;
  if (auto* ext = qobject_cast<ExternalNodeExecutor*>(nodeExecutor())) {
    currentType = ExternalNodeExecutor::typeString();
    currentEnvPath = ext->envPath();
  }

  auto* widget = new PythonNodeEditorWidget(
    this, pipeline, label(), m_script, m_jsonDescription, m_parameters,
    currentType, currentEnvPath, factory, customNeedsData, parent);

  connect(widget, &PythonNodeEditorWidget::applied, this,
          [this](const PythonNodeEdits& edits) {
            bool changed = false;

            // Description first: it decides which parameters exist, so
            // the values below have to land on the new declarations.
            if (m_jsonDescription != edits.jsonDescription) {
              reconfigureDescription(edits.jsonDescription);
              changed = true;
            }

            if (label() != edits.label) {
              setLabel(edits.label);
              changed = true;
            }

            if (m_script != edits.script) {
              setScript(edits.script);
              changed = true;
            }

            // Parameter values are already finalized by the editor —
            // pulled from the custom widget if present, else from the
            // auto-form, else empty (Parameters tab is the warning).
            for (auto it = edits.values.constBegin();
                 it != edits.values.constEnd(); ++it) {
              if (m_parameters.value(it.key()) != it.value()) {
                changed = true;
              }
              setParameter(it.key(), it.value());
            }

            // Apply executor-tab choice. Internal (empty type) clears
            // any per-node executor; External updates the env path on
            // an existing ExternalNodeExecutor in place, or attaches a
            // new one. Counts as a "changed" for re-execution purposes
            // when the effective type/env actually moved.
            auto* currentExternal =
              qobject_cast<ExternalNodeExecutor*>(nodeExecutor());
            if (edits.executorType.isEmpty()) {
              if (nodeExecutor() != nullptr) {
                setNodeExecutor(nullptr);
                changed = true;
              }
            } else if (edits.executorType ==
                       ExternalNodeExecutor::typeString()) {
              if (currentExternal) {
                if (currentExternal->envPath() != edits.executorEnvPath) {
                  currentExternal->setEnvPath(edits.executorEnvPath);
                  changed = true;
                }
              } else {
                setNodeExecutor(
                  new ExternalNodeExecutor(edits.executorEnvPath));
                changed = true;
              }
            }

            if (changed) {
              emit parametersApplied();
            }
          });

  // The controls may not exist yet (built once upstream data is in
  // memory) and are rebuilt on Apply; the connections live on the
  // controls themselves, so re-wiring after each build is safe.
  wireParameterBindings(this, widget, m_parameterBindings);
  connect(widget, &PythonNodeEditorWidget::parameterWidgetInstalled, this,
          [this, widget]() {
            wireParameterBindings(this, widget, m_parameterBindings);
          });

  return widget;
}

QJsonObject LegacyPythonTransform::serialize() const
{
  auto json = TransformNode::serialize();
  json["description"] = m_jsonDescription;
  json["script"] = m_script;
  if (!m_parameters.isEmpty()) {
    json["arguments"] = QJsonObject::fromVariantMap(m_parameters);
  }
  return json;
}

bool LegacyPythonTransform::deserialize(const QJsonObject& json)
{
  // setJSONDescription / setScript have side effects (parseJSON,
  // detecting CancelableOperator etc.), so we call the setters rather
  // than assigning members directly. Order: description first so
  // parseJSON can rename the output port and set up result ports,
  // then script, then arguments.
  if (json.contains("description")) {
    setJSONDescription(json.value("description").toString());
  }
  if (json.contains("script")) {
    setScript(json.value("script").toString());
  }
  // TransformNode::deserialize applies label last — call it after
  // parseJSON, which may have set a label from the JSON description's
  // "label" field, so the explicitly-saved label wins.
  if (!TransformNode::deserialize(json)) {
    return false;
  }
  auto args = json.value("arguments").toObject();
  for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
    const QString& key = it.key();
    const QString declType = m_parameterTypes.value(key);
    QVariant qv;
    if (declType == "enumeration") {
      qv = PythonNodeUtils::resolveEnumValue(it.value(), m_enumOptions.value(key));
    }
    if (!qv.isValid()) {
      qv = PythonNodeUtils::coerceJsonByDeclaredType(it.value(), declType);
    }
    if (!qv.isValid()) {
      // Unknown / complex declared type (select_scalars, xyz_header, …):
      // fall back to QJsonValue's own conversion.
      qv = it.value().toVariant();
    }
    setParameter(key, qv);
  }

  // Legacy compatibility: an operator JSON description may carry a
  // `tomviz_pipeline_env` key that pre-dates the schema-v2 per-node
  // executor block. If no explicit executor was set by Node::deserialize
  // above, synthesize an ExternalNodeExecutor here so the operator runs
  // in the configured Python env. Re-saving the file converges to the
  // schema-v2 form (the executor block in serialize() takes over).
  if (!nodeExecutor()) {
    QJsonDocument descDoc =
      QJsonDocument::fromJson(m_jsonDescription.toUtf8());
    if (descDoc.isObject()) {
      auto obj = descDoc.object();
      auto envPath =
        obj.value(QStringLiteral("tomviz_pipeline_env")).toString();
      bool externalOnly =
        obj.value(QStringLiteral("externalOnly")).toBool(false);
      if (!envPath.isEmpty() || externalOnly) {
        setNodeExecutor(new ExternalNodeExecutor(envPath));
      }
    }
  }
  return true;
}

void LegacyPythonTransform::parseJSON(bool createPorts, bool applyLabel)
{
  m_inheritColorMap = true;
  QJsonDocument doc = QJsonDocument::fromJson(m_jsonDescription.toUtf8());
  if (!doc.isObject()) {
    return;
  }

  QJsonObject obj = doc.object();

  m_operatorName = obj.value("name").toString();
  m_inheritColorMap = obj.value("inheritColorMap").toBool(true);

  if (applyLabel && obj.contains("label")) {
    setLabel(obj.value("label").toString());
  }

  // Custom widget ID (e.g. "RotationAlignWidget")
  if (obj.contains("widget")) {
    m_customWidgetID = obj.value("widget").toString();
  }

  // Parse parameters with defaults
  m_datasetInputNames.clear();
  m_parameterTypes.clear();
  m_enumOptions.clear();
  m_parameterBindings = parseParameterBindings(obj);
  if (obj.contains("parameters")) {
    QJsonArray params = obj.value("parameters").toArray();
    for (const auto& paramVal : params) {
      QJsonObject param = paramVal.toObject();
      QString name = param.value("name").toString();
      QString type = param.value("type").toString();
      QJsonValue defaultVal = param.value("default");

      // Dataset parameters become additional input ports, not parameters.
      if (type == "dataset") {
        if (createPorts) {
          addInput(name, PortType::ImageData);
        }
        m_datasetInputNames.append(name);
        continue;
      }

      // Record the declared type so deserialize can coerce saved
      // arguments correctly (Qt6 JSON parses every number into a
      // QVariant<double>, regardless of whether it was an int).
      if (!name.isEmpty()) {
        m_parameterTypes[name] = type;
      }

      // For enumerations, cache the options list so deserialize can
      // map a saved index to its value. Resolve the default to the
      // option value here so m_parameters always holds option values,
      // never indices — serialization then writes values uniformly.
      if (type == "enumeration") {
        QJsonArray options = param.value("options").toArray();
        m_enumOptions[name] = options;
        QVariant resolved = PythonNodeUtils::resolveEnumValue(defaultVal, options);
        if (resolved.isValid()) {
          m_parameters[name] = resolved;
          continue;
        }
      }

      QVariant value = PythonNodeUtils::coerceJsonByDeclaredType(defaultVal, type);
      if (!value.isValid()) {
        // Complex / unknown declared type (select_scalars, xyz_header,
        // label_map, reconstruction, …): with no explicit default, let
        // the operator's own Python default (usually None) win. With
        // an explicit default we fall back to QJsonValue::toVariant()
        // — historically this branch coerced via toDouble(), but that
        // produced 0.0 for non-numeric defaults; toVariant() preserves
        // the JSON-native type (string, list, …) instead.
        if (defaultVal.isUndefined() || defaultVal.isNull()) {
          continue;
        }
        value = defaultVal.toVariant();
      }

      m_parameters[name] = value;
    }
  }

  // Parse results → additional output ports
  m_resultNames.clear();
  m_resultTypes.clear();
  if (obj.contains("results")) {
    QJsonArray results = obj.value("results").toArray();
    for (const auto& resultVal : results) {
      QJsonObject result = resultVal.toObject();
      QString name = result.value("name").toString();
      QString type = result.value("type").toString();

      m_resultNames.append(name);
      m_resultTypes.append(type);

      PortType portType = PortType::None;
      if (type == "table") {
        portType = PortType::Table;
      } else if (type == "molecule") {
        portType = PortType::Molecule;
      }

      if (portType != PortType::None && createPorts) {
        addOutput(name, portType);
      }
    }
  }

  // If the operator declares a child dataset, remap the primary output port
  // to use the child's name and mark it as persistent.
  if (obj.contains("children")) {
    QJsonArray children = obj.value("children").toArray();
    if (!children.isEmpty()) {
      QJsonObject child = children.first().toObject();
      m_childName = child.value("name").toString();
      QString childLabel = child.value("label").toString();

      auto* out = outputPort(m_primaryOutputName);
      if (out && !m_childName.isEmpty()) {
        out->setName(m_childName);
        m_primaryOutputName = m_childName;
        out->setPersistent(true);
      }
    }
  }

  // Override the primary volume port types if specified
  if (obj.contains("inputType")) {
    PortType pt = portTypeFromString(obj.value("inputType").toString());
    if (pt != PortType::None) {
      inputPort("volume")->setAcceptedTypes(pt);
    }
  }
  if (obj.contains("outputType")) {
    PortType pt = portTypeFromString(obj.value("outputType").toString());
    auto* out = outputPort(m_primaryOutputName);
    // Only when the declared type actually moves. setDeclaredType() also
    // resets the effective type, so re-applying an unchanged description
    // (what the Definition tab does on every Apply) would otherwise throw
    // away a type inferred from the upstream connection — an ImageData
    // output fed by a Volume drops back to plain ImageData. Node's own
    // deserialize guards this the same way.
    if (pt != PortType::None && out && out->declaredType() != pt) {
      out->setDeclaredType(pt);
    }
  }
}

QMap<QString, PortData> LegacyPythonTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  // Get the input volume
  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  // Deep copy the vtkImageData (operators modify in-place)
  vtkNew<vtkImageData> outputImage;
  outputImage->DeepCopy(inputVolume->imageData());

  // Use the previous output's active scalar (if any) as the merge
  // target so user selections persist across re-runs. The override is
  // on the deep copy — upstream input is untouched.
  if (auto* outPort = outputPort(m_primaryOutputName)) {
    if (outPort->hasData() && isVolumeType(outPort->data().type())) {
      try {
        auto prevVol = outPort->data().value<VolumeDataPtr>();
        if (prevVol && prevVol->imageData()) {
          if (auto* prevScalars =
                prevVol->imageData()->GetPointData()->GetScalars()) {
            if (auto* name = prevScalars->GetName()) {
              if (outputImage->GetPointData()->HasArray(name)) {
                outputImage->GetPointData()->SetActiveScalars(name);
              }
            }
          }
        }
      } catch (const std::bad_any_cast&) {
      }
    }
  }

  // Ensure Python is initialized. Never finalize — C extension modules
  // like numpy cannot be re-loaded after finalize_interpreter().
  if (!Py_IsInitialized()) {
    py::initialize_interpreter();
  }

  try {
    py::gil_scoped_acquire gil;

    // Some operator scripts reference tomviz.utils.* without
    // importing it; preload it as legacy/OperatorPython.cxx does.
    py::module_::import("tomviz.utils");

    // Wrap the copied vtkImageData in a numpy-backed LegacyDataset
    // (tomviz_pipeline) — v1 operators expect the
    // dataset.create_child_dataset() API. The wrapper's arrays are
    // views over the VTK buffers; replaced arrays and metadata are
    // flushed back after the transform runs.
    py::module_ boundary = py::module_::import("tomviz._boundary");
    py::object dataset = boundary.attr("wrap_vtk_image")(
      py::cast(static_cast<vtkImageData*>(outputImage.Get()),
               py::return_value_policy::reference),
      /*legacy=*/true);

    py::object scriptModule =
      PythonNodeUtils::loadScriptAsModule(m_operatorName, m_script);

    // Find the transform function: try module-level transform() first, then
    // transform_scalars(), then look for an Operator subclass.
    py::object transformFunc = py::none();
    if (py::hasattr(scriptModule, "transform")) {
      transformFunc = scriptModule.attr("transform");
    } else if (py::hasattr(scriptModule, "transform_scalars")) {
      transformFunc = scriptModule.attr("transform_scalars");
    }

    // If no module-level function, look for an Operator subclass
    if (transformFunc.is_none()) {
      py::module_ internalMod =
        py::module_::import("tomviz._internal");
      py::object findOpClass = internalMod.attr("find_operator_class");
      py::object opClass = findOpClass(scriptModule);

      if (!opClass.is_none()) {
        // Build the standard _operator_wrapper (progress / cancel /
        // completion / progress_data API) — shared with future Python-
        // bearing node types.
        py::object wrapper = createNodeWrapper(this, m_primaryOutputName);

        // Instantiate following the same pattern as find_transform_function:
        // cls.__new__(cls) → set _operator_wrapper → cls.__init__(o)
        py::object instance = opClass.attr("__new__")(opClass);
        instance.attr("_operator_wrapper") = wrapper;
        opClass.attr("__init__")(instance);

        // Check which transform method was actually implemented
        py::object implCheck =
          internalMod.attr("_operator_method_was_implemented");
        if (implCheck(instance, "transform").cast<bool>()) {
          transformFunc = instance.attr("transform");
        } else if (implCheck(instance, "transform_scalars")
                     .cast<bool>()) {
          transformFunc = instance.attr("transform_scalars");
        }
      }
    }

    if (transformFunc.is_none()) {
      qWarning("LegacyPythonTransform: No transform function found "
               "in script");
      return result;
    }

    // Build kwargs dict from parameters
    py::dict kwargs;
    for (auto it = m_parameters.constBegin(); it != m_parameters.constEnd();
         ++it) {
      kwargs[py::str(it.key().toStdString())] =
        PythonNodeUtils::qvariantToPython(it.value());
    }

    // Wrap dataset input ports as LegacyDataset kwargs. As before the
    // numpy switch, these wrap the port's image directly (no copy);
    // they are flushed after the call so replaced arrays land in the
    // same image in-place mutations already reached through the views.
    py::list wrappedInputs;
    for (const auto& dsName : m_datasetInputNames) {
      auto it = inputs.find(dsName);
      if (it == inputs.end()) {
        continue;
      }
      auto dsVolume = it.value().template value<VolumeDataPtr>();
      if (!dsVolume || !dsVolume->isValid()) {
        continue;
      }
      py::object dsObj = boundary.attr("wrap_vtk_image")(
        py::cast(dsVolume->imageData(),
                 py::return_value_policy::reference),
        /*legacy=*/true);
      kwargs[py::str(dsName.toStdString())] = dsObj;
      wrappedInputs.append(dsObj);
    }

    // Route through transform_method_wrapper so that
    // tomviz_pipeline_env (external execution) is honored.
    py::object wrapperFunc =
      py::module_::import("tomviz._internal")
        .attr("transform_method_wrapper");

    QJsonObject opJson;
    opJson["type"] = QStringLiteral("Python");
    opJson["description"] = m_jsonDescription;
    opJson["label"] = label();
    opJson["script"] = m_script;
    if (!m_parameters.isEmpty()) {
      opJson["arguments"] =
        QJsonObject::fromVariantMap(m_parameters);
    }
    QString opSerialized =
      QString::fromUtf8(QJsonDocument(opJson).toJson());

    py::object pyResult = wrapperFunc(
      transformFunc, py::str(opSerialized.toStdString()),
      dataset, **kwargs);

    // Flush replaced arrays / metadata from the numpy datasets back
    // into their backing vtkImageData (in-place mutations are already
    // there via the views).
    boundary.attr("flush_dataset")(dataset);
    for (auto wrapped : wrappedInputs) {
      boundary.attr("flush_dataset")(
        py::reinterpret_borrow<py::object>(wrapped));
    }

    // Determine the output vtkImageData.
    // Default: outputImage (deep copy of input, modified in-place by Python).
    vtkSmartPointer<vtkImageData> outputData = outputImage.Get();

    // If a child dataset was declared and returned in the dict, use its
    // vtkImageData as the primary output instead.
    if (!m_childName.isEmpty() && !pyResult.is_none() &&
        py::isinstance<py::dict>(pyResult)) {
      py::dict outputDict = pyResult.cast<py::dict>();
      std::string childKey = m_childName.toStdString();
      if (outputDict.contains(childKey)) {
        py::object childObj =
          boundary.attr("payload_to_vtk")(outputDict[py::str(childKey)]);
        if (!childObj.is_none()) {
          auto* childImage = vtkImageData::SafeDownCast(
            vtkPythonUtil::GetPointerFromObject(
              childObj.ptr(), "vtkObjectBase"));
          if (childImage) {
            outputData = childImage;
          }
        }
      }
    }

    auto* outPort = outputPort(m_primaryOutputName);
    PortType outType =
      outPort ? outPort->declaredType() : PortType::ImageData;

    auto volume = makeVolumeData(outputData, outType);
    volume->setLabel(inputVolume->label());
    volume->setUnits(inputVolume->units());

    result[m_primaryOutputName] = PortData(std::any(volume), outType);

    // Extract result outputs (tables, molecules) from Python return dict
    if (!pyResult.is_none() && py::isinstance<py::dict>(pyResult)) {
      py::dict outputDict = pyResult.cast<py::dict>();
      for (int i = 0; i < m_resultNames.size(); ++i) {
        std::string key = m_resultNames[i].toStdString();
        if (!outputDict.contains(key)) {
          qWarning("LegacyPythonTransform: No result named '%s' in "
                   "Python output dict",
                   qPrintable(m_resultNames[i]));
          continue;
        }
        py::object pyObj =
          boundary.attr("payload_to_vtk")(outputDict[py::str(key)]);
        if (pyObj.is_none()) {
          continue;
        }

        if (m_resultTypes[i] == "table") {
          auto* raw = vtkTable::SafeDownCast(
            vtkPythonUtil::GetPointerFromObject(
              pyObj.ptr(), "vtkObjectBase"));
          if (raw) {
            vtkSmartPointer<vtkTable> table = raw;
            result[m_resultNames[i]] =
              PortData(std::any(table), PortType::Table);
          } else {
            qWarning("LegacyPythonTransform: Result '%s' is not a "
                     "vtkTable",
                     qPrintable(m_resultNames[i]));
          }
        } else if (m_resultTypes[i] == "molecule") {
          auto* raw = vtkMolecule::SafeDownCast(
            vtkPythonUtil::GetPointerFromObject(
              pyObj.ptr(), "vtkObjectBase"));
          if (raw) {
            vtkSmartPointer<vtkMolecule> molecule = raw;
            result[m_resultNames[i]] =
              PortData(std::any(molecule), PortType::Molecule);
          } else {
            qWarning("LegacyPythonTransform: Result '%s' is not a "
                     "vtkMolecule",
                     qPrintable(m_resultNames[i]));
          }
        }
      }
    }

  } catch (const py::error_already_set& e) {
    std::string full = e.what();

    // Strip the "At:\n  ..." traceback that pybind11 appends.
    std::string msg = full;
    auto atPos = msg.find("\n\nAt:");
    if (atPos != std::string::npos) {
      msg.erase(atPos);
    }

    // Extract script line number from the "At:" block. pybind11 formats
    // frames as "  <string>(748): transform\n" — find the last one from
    // "<string>" to get the deepest operator-script frame.
    int scriptLine = -1;
    const std::string marker = "<string>(";
    auto searchPos = full.find("At:");
    while (searchPos != std::string::npos) {
      auto hit = full.find(marker, searchPos);
      if (hit == std::string::npos) break;
      auto numStart = hit + marker.size();
      auto numEnd = full.find(')', numStart);
      if (numEnd != std::string::npos) {
        try {
          scriptLine = std::stoi(full.substr(numStart, numEnd - numStart));
        } catch (...) {}
      }
      searchPos = numEnd;
    }

    if (scriptLine >= 0) {
      qWarning("LegacyPythonTransform Python error (line %d): %s",
               scriptLine, msg.c_str());
    } else {
      qWarning("LegacyPythonTransform Python error: %s", msg.c_str());
    }
  } catch (const std::exception& e) {
    qWarning("LegacyPythonTransform error: %s", e.what());
  }

  return result;
}

} // namespace pipeline
} // namespace tomviz
