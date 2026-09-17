/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePythonNodeUtils_h
#define tomvizPipelinePythonNodeUtils_h

// Qt's `slots` macro conflicts with Python's object.h. Includers must
// already deal with this; we mirror the dance here so they can include
// the header in any order.
#pragma push_macro("slots")
#undef slots
#include <pybind11/pybind11.h>
#pragma pop_macro("slots")

#include "EnumOptions.h"
#include "PortData.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QVariant>

namespace tomviz {
namespace pipeline {

class OutputPort;

/// Helpers shared by every C++ class that runs a tomviz Python operator
/// (LegacyPythonTransform today; the schema-v2 PythonSource /
/// PythonTransform pair next). All entry points assume the caller
/// already holds the GIL.
namespace PythonNodeUtils {

/// Coerce @a value into a QVariant whose Qt type matches the operator
/// description's declared @a type for a parameter. Returns a valid
/// QVariant for the scalar types operators care about (``int``,
/// ``double``, ``bool``, ``string``-family, ``enumeration``) and for
/// arrays of int/double; returns an invalid QVariant for any other
/// declared type so the caller can decide on a site-appropriate
/// fallback. Needed because Qt6's QJsonValue::toVariant() collapses
/// every JSON number into a QVariant<double> regardless of whether the
/// source was an int — which lets ``axis: 2`` reach Python operators
/// as ``2.0`` and break tuple/list indexing.
QVariant coerceJsonByDeclaredType(const QJsonValue& value,
                                  const QString& type);


/// Convert a QVariant to a Python object. Preserves int/double/bool
/// distinctions that QJsonValue::toVariant collapses (a JSON ``2``
/// loaded into a QVariant<double> still reaches Python as a float —
/// callers that need integer round-tripping must coerce upstream;
/// see LegacyPythonTransform's coerceJsonByDeclaredType). Lists recurse
/// element-wise; unknown scalar types fall back to a string conversion
/// matching the legacy behavior.
pybind11::object qvariantToPython(const QVariant& value);

/// Convert a Python value into a QVariant restricted to the
/// JSON-compatible types the node user-state bag supports (None, bool,
/// int, float, str, plus lists/tuples and string-keyed dicts thereof).
/// When the value — or anything nested in it — has no mapping, @a ok
/// (if given) is set to false and an invalid QVariant is returned.
QVariant pythonToQVariant(pybind11::handle value, bool* ok = nullptr);

/// Convert a Python dict into a QVariantMap via pythonToQVariant.
/// Entries with non-string keys or non-convertible values are dropped
/// with a warning naming the key.
QVariantMap pyDictToVariantMap(pybind11::dict dict);

/// Convert a QVariantMap into a Python dict via qvariantToPython.
/// Invalid QVariants map to None.
pybind11::dict variantMapToPyDict(const QVariantMap& map);

/// Load Python source code as a fresh ModuleType and exec the source
/// inside the module's namespace. @a name is stamped onto the module's
/// ``__name__`` so tracebacks point at it; the module is *not*
/// registered in sys.modules. Raises py::error_already_set on syntax /
/// runtime errors during exec.
pybind11::object loadScriptAsModule(const QString& name,
                                    const QString& script);

/// Locate a single subclass of @a baseClass in @a module. Returns
/// py::none() if no subclass is found. Raises a py::value_error if more
/// than one subclass is present (mirrors the operator-discovery
/// contract: one node per script). The base class itself is not
/// reported as a match.
pybind11::object findNodeClass(pybind11::object module,
                               pybind11::object baseClass);

/// Convert a Python value (typically a tomviz Dataset wrapper, or a
/// raw vtkObject) into a typed PortData suitable for @a port. The
/// conversion follows @a port's effective type: volume-shaped ports
/// expect a vtkImageData, Table ports a vtkTable, Molecule ports a
/// vtkMolecule. Returns an invalid PortData when no conversion
/// applies.
PortData pythonValueToPortData(pybind11::object pyValue, OutputPort* port);

} // namespace PythonNodeUtils

} // namespace pipeline
} // namespace tomviz

#endif
