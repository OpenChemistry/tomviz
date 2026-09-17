/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineEnumOptions_h
#define tomvizPipelineEnumOptions_h

#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

namespace tomviz {
namespace pipeline {
namespace PythonNodeUtils {

// How an enumeration parameter's ``options`` (a list of single
// {"Label": value} objects) relate to what is stored for it. Kept apart
// from PythonNodeUtils.h so code and tests that never touch Python can
// use them without pybind11.

/// Resolve an enumeration parameter's stored form to the option value
/// the operator actually receives. The state-file convention is to
/// persist the option value (so a saved file is self-describing), so a
/// number or string that matches an option's value is that value; a
/// number matching nothing is read as an index, so older files that
/// persisted the index also load. Returns an invalid QVariant when no
/// resolution is possible — the caller falls back to its usual
/// coercion path.
QVariant resolveEnumValue(const QJsonValue& value,
                          const QJsonArray& options);

/// Resolve an enumeration's declared ``default``, which by convention
/// is an index into ``options``; a string, or a number that is not a
/// valid index, is taken as an option value instead.
QVariant resolveEnumDefault(const QJsonValue& value,
                            const QJsonArray& options);

/// The index of the option whose value is @a value, or -1. Numbers
/// compare as numbers, so a stored 2 matches an option declared as
/// ``2`` or ``2.0``.
int enumOptionIndex(const QVariant& value, const QJsonArray& options);

} // namespace PythonNodeUtils
} // namespace pipeline
} // namespace tomviz

#endif
