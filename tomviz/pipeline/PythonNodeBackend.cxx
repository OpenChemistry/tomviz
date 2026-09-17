/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeBackend.h"

#include "InputPort.h"
#include "Node.h"
#include "NodeDefinitionValidator.h"
#include "OutputPort.h"
#include "PythonNodeUtils.h"
#include "PythonNodeWrapper.h"
#include "data/VolumeData.h"

#pragma push_macro("slots")
#undef slots
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "pybind11/PybindVTKTypeCaster.h"
#pragma pop_macro("slots")

// Python transforms run on pipeline worker threads (ThreadedExecutor)
// while the main thread may enter Python through VTK/ParaView, which use
// PyGILState_Ensure. pybind11's default GIL management keeps its own
// thread-state accounting that diverges from CPython's gilstate under
// the app's external Py_Initialize, corrupting the worker's
// PyThreadState (SIGSEGV). The build defines PYBIND11_SIMPLE_GIL_-
// MANAGEMENT (top-level CMakeLists.txt) to route gil_scoped_* through
// PyGILState_Ensure/Release. Fail loudly if that define is ever dropped.
#ifndef PYBIND11_SIMPLE_GIL_MANAGEMENT
#  error "tomviz requires PYBIND11_SIMPLE_GIL_MANAGEMENT (see CMakeLists.txt): \
threaded Python execution corrupts CPython thread state otherwise."
#endif

#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkNew.h>
#include <vtkPythonUtil.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>

#include <QJsonDocument>

PYBIND11_VTK_TYPECASTER(vtkImageData)

namespace py = pybind11;

namespace tomviz {
namespace pipeline {

namespace {

// Names of the user-facing Python base classes in tomviz.nodes. The
// backend imports these to hand to PythonNodeUtils::findNodeClass when
// dispatching to the user's class.
constexpr const char* kSourceBaseAttr = "SourceNode";
constexpr const char* kTransformBaseAttr = "TransformNode";

/// Convert a PortData payload into a Python object suitable for the
/// inputs dict. ImageData/Volume/TiltSeries get wrapped in a numpy-
/// backed tomviz_pipeline Dataset (around a deep copy of the input
/// vtkImageData, so in-place mutations by the user's transform don't
/// corrupt the upstream port's payload). Tables/molecules convert to
/// the library's pure Table/Molecule payloads.
py::object portDataToPython(const PortData& data, py::object boundary)
{
  if (!data.isValid()) {
    return py::none();
  }
  PortType type = data.type();
  if (isVolumeType(type)) {
    auto vol = data.value<VolumeDataPtr>();
    if (!vol || !vol->isValid()) {
      return py::none();
    }
    // Deep copy for isolation, then wrap in a numpy-backed
    // tomviz_pipeline Dataset whose arrays are views over the copy.
    vtkNew<vtkImageData> copy;
    copy->DeepCopy(vol->imageData());
    return boundary.attr("wrap_vtk_image")(
      py::cast(static_cast<vtkImageData*>(copy.Get()),
               py::return_value_policy::reference),
      /*legacy=*/false);
  }
  if (type == PortType::Table) {
    auto sp = data.value<vtkSmartPointer<vtkTable>>();
    if (!sp) {
      return py::none();
    }
    return boundary.attr("vtk_table_to_table")(
      py::reinterpret_steal<py::object>(
        vtkPythonUtil::GetObjectFromPointer(sp.GetPointer())));
  }
  if (type == PortType::Molecule) {
    auto sp = data.value<vtkSmartPointer<vtkMolecule>>();
    if (!sp) {
      return py::none();
    }
    return boundary.attr("vtk_molecule_to_molecule")(
      py::reinterpret_steal<py::object>(
        vtkPythonUtil::GetObjectFromPointer(sp.GetPointer())));
  }
  return py::none();
}

/// Copy the instance's `self.state` dict back onto the host node's
/// user-state bag. A rebound non-dict state is rejected with a
/// warning rather than clobbering the existing bag.
void harvestUserState(Node* host, py::object instance)
{
  if (!py::hasattr(instance, "state")) {
    return;
  }
  py::object state = instance.attr("state");
  if (!py::isinstance<py::dict>(state)) {
    qWarning("PythonNodeBackend: self.state must be a dict; ignoring "
             "the non-dict value it was rebound to");
    return;
  }
  host->setUserState(
    PythonNodeUtils::pyDictToVariantMap(state.cast<py::dict>()));
}

/// Give the kernel instance what `self.set_parameter` /
/// `self.parameter` need: the declared parameter specs (validation and
/// coercion) and the current values. Nothing is shared with the
/// backend — updates are collected afterwards by
/// harvestParameterUpdates. Mirrors the Python runtime's
/// PythonNodeBackend._inject_parameter_api.
void injectParameterApi(py::object instance,
                        const QMap<QString, QJsonObject>& specs,
                        const QMap<QString, QVariant>& values)
{
  py::dict spec;
  for (auto it = specs.constBegin(); it != specs.constEnd(); ++it) {
    spec[py::str(it.key().toStdString())] =
      PythonNodeUtils::variantMapToPyDict(it.value().toVariantMap());
  }
  instance.attr("_parameter_spec") = spec;
  instance.attr("_parameter_values") =
    PythonNodeUtils::variantMapToPyDict(values);
  instance.attr("_parameter_updates") = py::dict();
}

/// Install the parameter values the kernel changed through
/// `self.set_parameter` on the host node — quietly, through
/// Node::applyParameterUpdates. A kernel base class predating the
/// feature has no `_parameter_updates`; nothing to do then.
void harvestParameterUpdates(Node* host, py::object instance)
{
  if (!py::hasattr(instance, "_parameter_updates")) {
    return;
  }
  py::object updates = instance.attr("_parameter_updates");
  if (!py::isinstance<py::dict>(updates)) {
    return;
  }
  QVariantMap map =
    PythonNodeUtils::pyDictToVariantMap(updates.cast<py::dict>());
  if (!map.isEmpty()) {
    host->applyParameterUpdates(map);
  }
}

} // namespace

PythonNodeBackend::PythonNodeBackend() = default;

void PythonNodeBackend::setJSONDescription(const QString& json)
{
  QMutexLocker locker(&m_parametersMutex);
  m_jsonDescription = json;
  parseDescriptionLocked();
}

QString PythonNodeBackend::jsonDescription() const
{
  return m_jsonDescription;
}

QStringList PythonNodeBackend::reconfigure(const QString& json)
{
  // One lock across the reparse and the merge, so a run snapshotting
  // the parameters on a worker thread never sees the cleared or
  // defaults-only intermediate state.
  QMutexLocker locker(&m_parametersMutex);
  auto previousValues = m_parameters;
  auto previousTypes = m_parameterTypes;

  m_jsonDescription = json;
  parseDescriptionLocked();

  QStringList reset;
  m_parameters =
    mergeParameterValues(previousValues, previousTypes, m_parameters,
                         m_parameterTypes, m_enumOptions, &reset);
  return reset;
}

void PythonNodeBackend::setScript(const QString& script)
{
  QMutexLocker locker(&m_parametersMutex);
  m_script = script;
}

QString PythonNodeBackend::scriptSource() const
{
  QMutexLocker locker(&m_parametersMutex);
  return m_script;
}

QString PythonNodeBackend::operatorName() const { return m_operatorName; }
QString PythonNodeBackend::defaultLabel() const { return m_defaultLabel; }
QString PythonNodeBackend::descriptionText() const { return m_description; }
QString PythonNodeBackend::helpText() const { return m_help; }
QString PythonNodeBackend::customWidgetID() const { return m_customWidgetID; }
bool PythonNodeBackend::supportsCancel() const { return m_supportsCancel; }
bool PythonNodeBackend::supportsComplete() const { return m_supportsComplete; }
bool PythonNodeBackend::isTransformShape() const { return !m_inputs.isEmpty(); }

QString PythonNodeBackend::externalPythonEnvPath() const
{
  return m_externalPythonEnvPath;
}

bool PythonNodeBackend::externalOnly() const
{
  return m_externalOnly;
}

void PythonNodeBackend::setParameter(const QString& name,
                                     const QVariant& value)
{
  QMutexLocker locker(&m_parametersMutex);
  m_parameters[name] = value;
}

QVariant PythonNodeBackend::parameter(const QString& name) const
{
  QMutexLocker locker(&m_parametersMutex);
  return m_parameters.value(name);
}

QMap<QString, QVariant> PythonNodeBackend::parameters() const
{
  QMutexLocker locker(&m_parametersMutex);
  return m_parameters;
}

QMap<QString, QVariant> PythonNodeBackend::applyParameterUpdates(
  const QMap<QString, QVariant>& updates)
{
  QMap<QString, QVariant> changed;
  QMutexLocker locker(&m_parametersMutex);
  for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
    auto existing = m_parameters.constFind(it.key());
    if (existing != m_parameters.constEnd() && existing.value() == it.value()) {
      continue;
    }
    m_parameters[it.key()] = it.value();
    changed[it.key()] = it.value();
  }
  return changed;
}

QMap<QString, ParameterBinding> PythonNodeBackend::parameterBindings() const
{
  return m_parameterBindings;
}

QStringList PythonNodeBackend::inputNames() const
{
  QStringList names;
  for (const auto& p : m_inputs) {
    names.append(p.name);
  }
  return names;
}

QStringList PythonNodeBackend::outputNames() const
{
  QStringList names;
  for (const auto& p : m_outputs) {
    names.append(p.name);
  }
  return names;
}

QString PythonNodeBackend::primaryOutputName() const
{
  return m_outputs.isEmpty() ? QString() : m_outputs.first().name;
}

// Caller must hold m_parametersMutex: this rewrites the parameter map,
// specs, and script metadata that runs snapshot from worker threads.
void PythonNodeBackend::parseDescriptionLocked()
{
  m_operatorName.clear();
  m_defaultLabel.clear();
  m_description.clear();
  m_help.clear();
  m_customWidgetID.clear();
  m_supportsCancel = false;
  m_supportsComplete = false;
  m_externalPythonEnvPath.clear();
  m_externalOnly = false;
  m_inputs.clear();
  m_outputs.clear();
  m_parameters.clear();
  m_parameterTypes.clear();
  m_enumOptions.clear();
  m_parameterSpecs.clear();
  m_parameterBindings.clear();

  QJsonDocument doc = QJsonDocument::fromJson(m_jsonDescription.toUtf8());
  if (!doc.isObject()) {
    return;
  }
  QJsonObject obj = doc.object();
  m_parameterBindings = parseParameterBindings(obj);

  m_operatorName = obj.value(QStringLiteral("name")).toString();
  m_defaultLabel = obj.value(QStringLiteral("label")).toString();
  m_description = obj.value(QStringLiteral("description")).toString();
  m_help = obj.value(QStringLiteral("help")).toString();
  m_customWidgetID = obj.value(QStringLiteral("widget")).toString();
  m_supportsCancel =
    obj.value(QStringLiteral("supportsCancel")).toBool(false);
  m_supportsComplete =
    obj.value(QStringLiteral("supportsComplete")).toBool(false);
  m_externalPythonEnvPath =
    obj.value(QStringLiteral("tomviz_pipeline_env")).toString();
  m_externalOnly = obj.value(QStringLiteral("externalOnly")).toBool(false);
  if (m_externalOnly &&
      !obj.value(QStringLiteral("externalCompatible")).toBool(true)) {
    qWarning("PythonNodeBackend: operator %s declares externalOnly with "
             "externalCompatible=false; treating as externalOnly.",
             qPrintable(m_operatorName));
  }

  // inputs / outputs are arrays of {name, type[, persistent]}. Missing
  // section → empty list (per the agreed schema-v2 convention).
  for (const auto& v : obj.value(QStringLiteral("inputs")).toArray()) {
    QJsonObject entry = v.toObject();
    PortSpec spec;
    spec.name = entry.value(QStringLiteral("name")).toString();
    spec.type =
      portTypeFromString(entry.value(QStringLiteral("type")).toString());
    if (!spec.name.isEmpty() && spec.type != PortType::None) {
      m_inputs.append(spec);
    }
  }
  for (const auto& v : obj.value(QStringLiteral("outputs")).toArray()) {
    QJsonObject entry = v.toObject();
    PortSpec spec;
    spec.name = entry.value(QStringLiteral("name")).toString();
    spec.type =
      portTypeFromString(entry.value(QStringLiteral("type")).toString());
    if (entry.contains(QStringLiteral("persistent"))) {
      spec.persistent =
        entry.value(QStringLiteral("persistent")).toBool(false);
      spec.persistentSpecified = true;
    }
    if (!spec.name.isEmpty() && spec.type != PortType::None) {
      m_outputs.append(spec);
    }
  }

  // parameters: same shape and coercion semantics as schema-v1, so the
  // shared PythonNodeUtils helpers handle int/double/enum without
  // Qt6's QJsonValue→QVariant<double> collapse breaking type-sensitive
  // operators.
  for (const auto& v : obj.value(QStringLiteral("parameters")).toArray()) {
    QJsonObject param = v.toObject();
    QString name = param.value(QStringLiteral("name")).toString();
    QString type = param.value(QStringLiteral("type")).toString();
    QJsonValue defaultVal = param.value(QStringLiteral("default"));
    if (name.isEmpty()) {
      continue;
    }
    m_parameterTypes[name] = type;
    m_parameterSpecs[name] = param;

    if (type == QLatin1String("enumeration")) {
      QJsonArray options = param.value(QStringLiteral("options")).toArray();
      m_enumOptions[name] = options;
      QVariant resolved =
        PythonNodeUtils::resolveEnumDefault(defaultVal, options);
      if (resolved.isValid()) {
        m_parameters[name] = resolved;
        continue;
      }
    }

    QVariant value =
      PythonNodeUtils::coerceJsonByDeclaredType(defaultVal, type);
    if (!value.isValid()) {
      // Complex / unknown declared type with no explicit default: let
      // the user's Python default win. With an explicit default fall
      // back to QJsonValue's own conversion (preserves string / list).
      if (defaultVal.isUndefined() || defaultVal.isNull()) {
        continue;
      }
      value = defaultVal.toVariant();
    }
    m_parameters[name] = value;
  }
}

void PythonNodeBackend::applyDescription(AddInputFn addInput,
                                         AddOutputFn addOutput)
{
  if (addInput) {
    for (const auto& spec : m_inputs) {
      addInput(spec.name, spec.type);
    }
  }
  if (addOutput) {
    for (const auto& spec : m_outputs) {
      OutputPort* port = addOutput(spec.name, spec.type);
      if (port && spec.persistentSpecified) {
        // Honor an explicit "persistent" flag from the operator JSON.
        // When omitted, leave whatever default the host node-class
        // installed (SourceNode → InMemory persistent; TransformNode →
        // OnDisk persistent during the temporary rollout).
        port->setPersistent(spec.persistent);
      }
    }
  }
}

QJsonObject PythonNodeBackend::serializeInto(QJsonObject base) const
{
  base[QStringLiteral("description")] = m_jsonDescription;
  base[QStringLiteral("script")] = m_script;
  const auto params = parameters();
  if (!params.isEmpty()) {
    base[QStringLiteral("arguments")] = QJsonObject::fromVariantMap(params);
  }
  return base;
}

void PythonNodeBackend::applySerializedFields(const QJsonObject& json,
                                              AddInputFn addInput,
                                              AddOutputFn addOutput)
{
  if (json.contains(QStringLiteral("description"))) {
    setJSONDescription(json.value(QStringLiteral("description")).toString());
  }
  if (json.contains(QStringLiteral("script"))) {
    setScript(json.value(QStringLiteral("script")).toString());
  }
  applyDescription(std::move(addInput), std::move(addOutput));

  auto args = json.value(QStringLiteral("arguments")).toObject();
  QMutexLocker locker(&m_parametersMutex);
  for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
    const QString& key = it.key();
    const QString declType = m_parameterTypes.value(key);
    QVariant qv;
    if (declType == QLatin1String("enumeration")) {
      qv = PythonNodeUtils::resolveEnumValue(
        it.value(), m_enumOptions.value(key));
    }
    if (!qv.isValid()) {
      qv = PythonNodeUtils::coerceJsonByDeclaredType(it.value(), declType);
    }
    if (!qv.isValid()) {
      qv = it.value().toVariant();
    }
    m_parameters[key] = qv;
  }
}

QMap<QString, PortData> PythonNodeBackend::runTransform(
  Node* host, const QMap<QString, PortData>& inputs)
{
  return runImpl(host, inputs, /*isSource=*/false);
}

QMap<QString, PortData> PythonNodeBackend::runSource(Node* host)
{
  return runImpl(host, {}, /*isSource=*/true);
}

bool PythonNodeBackend::runShouldAutoExecute(Node* host)
{
  if (!host) {
    return false;
  }

  if (!Py_IsInitialized()) {
    py::initialize_interpreter();
  }

  const bool isSource = !isTransformShape();

  // Snapshot what the GUI thread can rewrite mid-poll (a description
  // or script apply), so the whole poll sees one consistent view.
  QString operatorName;
  QString script;
  QMap<QString, QJsonObject> parameterSpecs;
  QMap<QString, QVariant> params;
  {
    QMutexLocker locker(&m_parametersMutex);
    operatorName = m_operatorName;
    script = m_script;
    parameterSpecs = m_parameterSpecs;
    params = m_parameters;
  }

  try {
    py::gil_scoped_acquire gil;

    py::module_::import("tomviz.utils");
    py::module_ nodesMod = py::module_::import("tomviz.nodes");
    py::object baseClass =
      nodesMod.attr(isSource ? kSourceBaseAttr : kTransformBaseAttr);

    py::object scriptModule =
      PythonNodeUtils::loadScriptAsModule(operatorName, script);
    py::object userClass =
      PythonNodeUtils::findNodeClass(scriptModule, baseClass);
    if (userClass.is_none()) {
      qWarning("PythonNodeBackend: no %s subclass found in script",
               isSource ? "SourceNode" : "TransformNode");
      return false;
    }

    // Same instantiation as runImpl so __init__ and the hook can use
    // self.progress / self.canceled / self.completed / self.state.
    py::object wrapper = createNodeWrapper(host, primaryOutputName());
    py::object instance = userClass.attr("__new__")(userClass);
    instance.attr("_operator_wrapper") = wrapper;
    userClass.attr("__init__")(instance);
    instance.attr("state") =
      PythonNodeUtils::variantMapToPyDict(host->userState());
    injectParameterApi(instance, parameterSpecs, params);

    // Scripts written against a tomviz.nodes that predates the hook
    // simply never auto-execute.
    if (!py::hasattr(instance, "should_auto_execute")) {
      return false;
    }

    py::dict kwargs;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
      kwargs[py::str(it.key().toStdString())] =
        PythonNodeUtils::qvariantToPython(it.value());
    }

    py::object pyResult;
    try {
      pyResult = instance.attr("should_auto_execute")(**kwargs);
    } catch (...) {
      // Like self.state, parameter write-backs made before the hook
      // raised still count (the Python runtime harvests in a finally).
      harvestUserState(host, instance);
      harvestParameterUpdates(host, instance);
      throw;
    }

    // The hook may legitimately update self.state — and its parameters
    // — even when it answers "no" (e.g. remembering the timestamp it
    // just inspected). The answer alone decides whether a run happens.
    harvestUserState(host, instance);
    harvestParameterUpdates(host, instance);

    return PyObject_IsTrue(pyResult.ptr()) == 1;
  } catch (const py::error_already_set& e) {
    qWarning("PythonNodeBackend should_auto_execute Python error: %s",
             e.what());
  } catch (const std::exception& e) {
    qWarning("PythonNodeBackend should_auto_execute error: %s", e.what());
  }
  return false;
}

QMap<QString, PortData> PythonNodeBackend::runImpl(
  Node* host, const QMap<QString, PortData>& inputs, bool isSource)
{
  QMap<QString, PortData> result;
  if (!host) {
    return result;
  }

  // Ensure the embedded interpreter is alive. Never finalize — C
  // extension modules like numpy can't be re-loaded after
  // finalize_interpreter().
  if (!Py_IsInitialized()) {
    py::initialize_interpreter();
  }

  // Snapshot what the GUI thread can rewrite mid-run: kwargs, the
  // kernel's `self.parameter()` view, and the loaded script must agree.
  QString operatorName;
  QString script;
  QMap<QString, QJsonObject> parameterSpecs;
  QMap<QString, QVariant> params;
  {
    QMutexLocker locker(&m_parametersMutex);
    operatorName = m_operatorName;
    script = m_script;
    parameterSpecs = m_parameterSpecs;
    params = m_parameters;
  }

  try {
    py::gil_scoped_acquire gil;

    // Some user scripts may reach into tomviz.utils.* without
    // importing it (legacy carry-over).
    py::module_::import("tomviz.utils");

    py::module_ boundary = py::module_::import("tomviz._boundary");

    py::module_ nodesMod = py::module_::import("tomviz.nodes");
    py::object baseClass =
      nodesMod.attr(isSource ? kSourceBaseAttr : kTransformBaseAttr);

    py::object scriptModule =
      PythonNodeUtils::loadScriptAsModule(operatorName, script);

    py::object userClass =
      PythonNodeUtils::findNodeClass(scriptModule, baseClass);
    if (userClass.is_none()) {
      qWarning("PythonNodeBackend: no %s subclass found in script",
               isSource ? "SourceNode" : "TransformNode");
      return result;
    }

    // Instantiate following the operator-class pattern: __new__ → set
    // _operator_wrapper → __init__. Lets the user's __init__ access
    // self.progress / self.canceled / self.completed.
    py::object wrapper =
      createNodeWrapper(host, primaryOutputName());
    py::object instance = userClass.attr("__new__")(userClass);
    instance.attr("_operator_wrapper") = wrapper;
    userClass.attr("__init__")(instance);

    // Hand the node's user-state bag to the instance as `self.state`.
    // The instance is rebuilt every run, so this is how state survives
    // between executions; it is harvested back after the user method
    // returns.
    instance.attr("state") =
      PythonNodeUtils::variantMapToPyDict(host->userState());
    injectParameterApi(instance, parameterSpecs, params);

    // Build kwargs from current parameters.
    py::dict kwargs;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
      kwargs[py::str(it.key().toStdString())] =
        PythonNodeUtils::qvariantToPython(it.value());
    }

    py::object pyResult;
    try {
      if (isSource) {
        pyResult = instance.attr("produce")(**kwargs);
      } else {
        // Convert each input PortData to the right Python
        // representation. Volume-shaped inputs get a numpy Dataset over
        // a deep copy so user mutation doesn't leak upstream.
        py::dict inputsDict;
        for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
          inputsDict[py::str(it.key().toStdString())] =
            portDataToPython(it.value(), boundary);
        }
        pyResult = instance.attr("transform")(inputsDict, **kwargs);
      }
    } catch (...) {
      // A raising user method still keeps the state and parameter
      // write-backs it made before failing — parity with the Python
      // runtime, which harvests in a finally.
      harvestUserState(host, instance);
      harvestParameterUpdates(host, instance);
      throw;
    }

    // Harvest state mutations before any early return below: a
    // canceled or output-less run still keeps what the user method
    // recorded.
    harvestUserState(host, instance);
    harvestParameterUpdates(host, instance);

    if (host->isCanceled()) {
      return result;
    }

    if (pyResult.is_none() || !py::isinstance<py::dict>(pyResult)) {
      // None is the documented return for "cancellation or error" per
      // tomviz.nodes — the user's transform/produce returned without
      // producing outputs. Any other non-dict return is treated the
      // same way. The caller's port-empty check transitions the node
      // to Failed.
      return result;
    }

    // Pull each declared output by name. Unknown / missing keys are
    // ignored (the caller will detect missing outputs via the
    // node-level check that PortData was set on every output port).
    py::dict outDict = pyResult.cast<py::dict>();
    for (const auto& spec : m_outputs) {
      OutputPort* port = host->outputPort(spec.name);
      if (!port) {
        continue;
      }
      std::string key = spec.name.toStdString();
      if (!outDict.contains(key)) {
        continue;
      }
      py::object payload = outDict[py::str(key)];
      PortData pd =
        PythonNodeUtils::pythonValueToPortData(payload, port);
      if (pd.isValid()) {
        result[spec.name] = pd;
      }
    }
  } catch (const py::error_already_set& e) {
    qWarning("PythonNodeBackend Python error: %s", e.what());
  } catch (const std::exception& e) {
    qWarning("PythonNodeBackend error: %s", e.what());
  }

  return result;
}

} // namespace pipeline
} // namespace tomviz
