/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonNodeUtils.h"

#include "OutputPort.h"
#include "PortType.h"
#include "data/LabelMapData.h"
#include "data/VolumeData.h"

#pragma push_macro("slots")
#undef slots
#include <pybind11/eval.h>
#pragma pop_macro("slots")

#include <vtkImageData.h>
#include <vtkMolecule.h>
#include <vtkPythonUtil.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>

#include <QJsonObject>
#include <QVariantList>

namespace py = pybind11;

namespace tomviz {
namespace pipeline {
namespace PythonNodeUtils {

QVariant coerceJsonByDeclaredType(const QJsonValue& value,
                                  const QString& type)
{
  if (value.isArray()) {
    QVariantList list;
    for (const auto& item : value.toArray()) {
      if (type == "int" || type == "integer") {
        list.append(item.toInt());
      } else if (type == "double") {
        list.append(item.toDouble());
      } else {
        list.append(item.toVariant());
      }
    }
    return list;
  }
  if (type == "int" || type == "integer") {
    return QVariant(value.toInt());
  }
  if (type == "enumeration") {
    // Enumeration option values can be either int or string; preserve
    // the JSON-native type so saved values round-trip losslessly.
    if (value.isString()) {
      return QVariant(value.toString());
    }
    return QVariant(value.toInt());
  }
  if (type == "double") {
    return QVariant(value.toDouble());
  }
  if (type == "bool" || type == "boolean") {
    return QVariant(value.toBool());
  }
  if (type == "string" || type == "file" || type == "save_file" ||
      type == "directory") {
    return QVariant(value.toString());
  }
  return QVariant();
}


py::object qvariantToPython(const QVariant& value)
{
  if (!value.isValid()) {
    return py::none();
  }
  switch (value.typeId()) {
    case QMetaType::Double:
      return py::float_(value.toDouble());
    case QMetaType::Int:
      return py::int_(value.toInt());
    case QMetaType::LongLong:
      return py::int_(value.toLongLong());
    case QMetaType::Bool:
      return py::bool_(value.toBool());
    case QMetaType::QString:
      return py::str(value.toString().toStdString());
    case QMetaType::QVariantList: {
      py::list pyList;
      for (const auto& item : value.toList()) {
        pyList.append(qvariantToPython(item));
      }
      return pyList;
    }
    case QMetaType::QVariantMap:
      return variantMapToPyDict(value.toMap());
    default:
      // Defensive fallback: cast to float. Operator parameters are
      // always one of the listed types in practice (loaded from JSON +
      // coerced via the operator description's declared type), so this
      // arm is essentially unreachable; keeping the legacy behavior
      // here for parity.
      return py::float_(value.toDouble());
  }
}

QVariant pythonToQVariant(py::handle value, bool* ok)
{
  if (ok) {
    *ok = true;
  }
  if (value.is_none()) {
    return QVariant();
  }
  // bool first: a Python bool is also an int.
  if (py::isinstance<py::bool_>(value)) {
    return QVariant(value.cast<bool>());
  }
  if (py::isinstance<py::int_>(value)) {
    return QVariant(value.cast<qlonglong>());
  }
  if (py::isinstance<py::float_>(value)) {
    return QVariant(value.cast<double>());
  }
  if (py::isinstance<py::str>(value)) {
    return QVariant(QString::fromStdString(value.cast<std::string>()));
  }
  if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value)) {
    QVariantList list;
    for (auto item : value.cast<py::sequence>()) {
      bool itemOk = true;
      QVariant v = pythonToQVariant(item, &itemOk);
      if (!itemOk) {
        if (ok) {
          *ok = false;
        }
        return QVariant();
      }
      list.append(v);
    }
    return list;
  }
  if (py::isinstance<py::dict>(value)) {
    QVariantMap map;
    for (auto item : value.cast<py::dict>()) {
      if (!py::isinstance<py::str>(item.first)) {
        if (ok) {
          *ok = false;
        }
        return QVariant();
      }
      bool itemOk = true;
      QVariant v = pythonToQVariant(item.second, &itemOk);
      if (!itemOk) {
        if (ok) {
          *ok = false;
        }
        return QVariant();
      }
      map[QString::fromStdString(item.first.cast<std::string>())] = v;
    }
    return map;
  }
  if (ok) {
    *ok = false;
  }
  return QVariant();
}

QVariantMap pyDictToVariantMap(py::dict dict)
{
  QVariantMap map;
  for (auto item : dict) {
    if (!py::isinstance<py::str>(item.first)) {
      qWarning("PythonNodeUtils: dropping state entry with non-string key");
      continue;
    }
    QString key = QString::fromStdString(item.first.cast<std::string>());
    bool ok = true;
    QVariant v = pythonToQVariant(item.second, &ok);
    if (!ok) {
      qWarning("PythonNodeUtils: state value for key '%s' is not "
               "JSON-serializable; dropping it",
               qPrintable(key));
      continue;
    }
    map[key] = v;
  }
  return map;
}

py::dict variantMapToPyDict(const QVariantMap& map)
{
  py::dict dict;
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    dict[py::str(it.key().toStdString())] = qvariantToPython(it.value());
  }
  return dict;
}

py::object loadScriptAsModule(const QString& name, const QString& script)
{
  py::module_ types = py::module_::import("types");
  py::object moduleType = types.attr("ModuleType");
  py::object module = moduleType(py::str(name.toStdString()));
  py::exec(py::str(script.toStdString()), module.attr("__dict__"));
  return module;
}

py::object findNodeClass(py::object module, py::object baseClass)
{
  py::module_ inspect = py::module_::import("inspect");
  py::object isclass = inspect.attr("isclass");
  py::object getmembers = inspect.attr("getmembers");

  py::object found = py::none();
  py::object members = getmembers(module, isclass);
  for (auto item : members) {
    py::tuple pair = py::reinterpret_borrow<py::tuple>(item);
    py::object cls = pair[1];
    // Skip the base class itself.
    if (cls.is(baseClass)) {
      continue;
    }
    if (!PyObject_IsSubclass(cls.ptr(), baseClass.ptr())) {
      // PyObject_IsSubclass returns -1 on error; py::error_already_set
      // would have been thrown by pybind11's exception translation, so
      // any non-truthy non-throwing return means "not a subclass".
      continue;
    }
    if (!found.is_none()) {
      throw py::value_error(
        "Multiple node classes defined in module — only one node class "
        "can be defined per script.");
    }
    found = cls;
  }
  return found;
}

PortData pythonValueToPortData(py::object pyValue, OutputPort* port)
{
  if (!port || pyValue.is_none()) {
    return PortData();
  }
  // Library payloads (tomviz_pipeline Dataset / Table / Molecule) are
  // converted to their VTK counterparts here — the single boundary for
  // declared outputs and live-preview (progress.data) payloads alike.
  // VTK objects pass through payload_to_vtk untouched.
  py::object dataObj =
    py::module_::import("tomviz._boundary").attr("payload_to_vtk")(pyValue);
  if (dataObj.is_none()) {
    return PortData();
  }
  void* raw =
    vtkPythonUtil::GetPointerFromObject(dataObj.ptr(), "vtkObjectBase");
  if (!raw) {
    return PortData();
  }
  PortType type = port->type();
  if (isVolumeType(type)) {
    if (auto* image =
          vtkImageData::SafeDownCast(static_cast<vtkObjectBase*>(raw))) {
      auto vol = makeVolumeData(image, type);
      return PortData(std::any(vol), type);
    }
  } else if (type == PortType::Table) {
    if (auto* table =
          vtkTable::SafeDownCast(static_cast<vtkObjectBase*>(raw))) {
      vtkSmartPointer<vtkTable> sp(table);
      return PortData(std::any(sp), type);
    }
  } else if (type == PortType::Molecule) {
    if (auto* mol =
          vtkMolecule::SafeDownCast(static_cast<vtkObjectBase*>(raw))) {
      vtkSmartPointer<vtkMolecule> sp(mol);
      return PortData(std::any(sp), type);
    }
  }
  return PortData();
}

} // namespace PythonNodeUtils
} // namespace pipeline
} // namespace tomviz
