/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeDefinitionValidator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

namespace tomviz {
namespace pipeline {

namespace {

// Parameter types ParameterInterfaceBuilder knows how to render. A
// parameter declared as anything else still reaches the script, but has
// no control in the Parameters tab, so it is worth flagging.
const QStringList& renderableParameterTypes()
{
  static const QStringList types = { QStringLiteral("bool"),
                                     QStringLiteral("int"),
                                     QStringLiteral("double"),
                                     QStringLiteral("enumeration"),
                                     QStringLiteral("xyz_header"),
                                     QStringLiteral("file"),
                                     QStringLiteral("save_file"),
                                     QStringLiteral("directory"),
                                     QStringLiteral("string"),
                                     QStringLiteral("select_scalars") };
  return types;
}

QJsonObject parseObject(const QString& json, QString* parseError)
{
  if (json.trimmed().isEmpty()) {
    return {};
  }
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError) {
    if (parseError) {
      *parseError = QStringLiteral("%1 (offset %2)")
                      .arg(error.errorString())
                      .arg(error.offset);
    }
    return {};
  }
  if (!doc.isObject()) {
    if (parseError) {
      *parseError = QStringLiteral("the description must be a JSON object");
    }
    return {};
  }
  return doc.object();
}

void addError(DefinitionValidation& result, const QString& message)
{
  result.issues.append({ DefinitionIssue::Severity::Error, message });
}

void addWarning(DefinitionValidation& result, const QString& message)
{
  result.issues.append({ DefinitionIssue::Severity::Warning, message });
}

/// Compare a key that a live node cannot re-apply. Arrays and objects
/// are compared structurally; an absent key reads as an empty array so
/// `"inputs": []` and no `inputs` at all are the same thing.
void checkFrozenKey(DefinitionValidation& result, const QJsonObject& current,
                    const QJsonObject& candidate, const QString& key,
                    const QString& reason)
{
  QJsonValue before = current.value(key);
  QJsonValue after = candidate.value(key);
  if (before.isUndefined() && after.isArray() && after.toArray().isEmpty()) {
    return;
  }
  if (after.isUndefined() && before.isArray() && before.toArray().isEmpty()) {
    return;
  }
  if (before == after) {
    return;
  }
  addError(result, QStringLiteral("\"%1\" cannot be changed on an existing "
                                  "node: %2")
                     .arg(key, reason));
}

/// The v1 schema turns `dataset`-typed parameters into input ports, so
/// their declaration is as frozen as `results` is.
QStringList datasetParameterNames(const QJsonObject& obj)
{
  QStringList names;
  for (const auto& value : obj.value(QStringLiteral("parameters")).toArray()) {
    QJsonObject param = value.toObject();
    if (param.value(QStringLiteral("type")).toString() ==
        QLatin1String("dataset")) {
      names.append(param.value(QStringLiteral("name")).toString());
    }
  }
  return names;
}

void validateParameters(DefinitionValidation& result,
                        const QJsonObject& candidate)
{
  QSet<QString> seen;
  for (const auto& value :
       candidate.value(QStringLiteral("parameters")).toArray()) {
    if (!value.isObject()) {
      addError(result,
               QStringLiteral("every entry in \"parameters\" must be an "
                              "object"));
      continue;
    }
    QJsonObject param = value.toObject();
    QString name = param.value(QStringLiteral("name")).toString();
    QString type = param.value(QStringLiteral("type")).toString();

    if (type.isEmpty()) {
      addError(result, QStringLiteral("parameter \"%1\" declares no \"type\"")
                         .arg(name.isEmpty() ? QStringLiteral("<unnamed>")
                                             : name));
      continue;
    }

    // A nameless entry is a layout marker (xyz_header), not a value the
    // script receives. Anything else without a name is a mistake.
    if (name.isEmpty()) {
      if (type != QLatin1String("xyz_header")) {
        addError(result,
                 QStringLiteral("a \"%1\" parameter must declare a \"name\"")
                   .arg(type));
      }
      continue;
    }

    if (seen.contains(name)) {
      addError(result,
               QStringLiteral("parameter \"%1\" is declared more than once")
                 .arg(name));
      continue;
    }
    seen.insert(name);

    if (type == QLatin1String("enumeration")) {
      QJsonArray options = param.value(QStringLiteral("options")).toArray();
      if (options.isEmpty()) {
        addError(result,
                 QStringLiteral("enumeration parameter \"%1\" declares no "
                                "\"options\"")
                   .arg(name));
      }
      for (const auto& option : options) {
        // The parameter form renders each option by taking the object's
        // single key as the label and its value as what the script gets,
        // so anything else can't be displayed.
        if (!option.isObject() || option.toObject().size() != 1) {
          addError(result,
                   QStringLiteral("each option of \"%1\" must be a single "
                                  "{\"Label\": value} pair")
                     .arg(name));
          break;
        }
      }
    }

    if (!renderableParameterTypes().contains(type) &&
        type != QLatin1String("dataset")) {
      addWarning(result,
                 QStringLiteral("parameter \"%1\" has type \"%2\", which has "
                                "no control in the Parameters tab — the "
                                "script still receives its default")
                   .arg(name, type));
    }
  }
}

void diffParameters(DefinitionValidation& result, const QString& currentJson,
                    const QString& candidateJson)
{
  auto before = parameterDeclaredTypes(currentJson);
  auto after = parameterDeclaredTypes(candidateJson);

  for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
    auto prev = before.constFind(it.key());
    if (prev == before.constEnd()) {
      result.addedParameters.append(it.key());
    } else if (prev.value() != it.value()) {
      result.retypedParameters.append(it.key());
    }
  }
  for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
    if (!after.contains(it.key())) {
      result.removedParameters.append(it.key());
    }
  }

  result.addedParameters.sort();
  result.removedParameters.sort();
  result.retypedParameters.sort();

  if (!result.removedParameters.isEmpty()) {
    addWarning(result,
               QStringLiteral("dropping %1 — any value you set is lost")
                 .arg(result.removedParameters.join(QStringLiteral(", "))));
  }
  if (!result.retypedParameters.isEmpty()) {
    addWarning(result,
               QStringLiteral("%1 changed type and will revert to the "
                              "declared default")
                 .arg(result.retypedParameters.join(QStringLiteral(", "))));
  }
  if (!result.addedParameters.isEmpty()) {
    addWarning(result,
               QStringLiteral("adding %1 — the script must accept it as a "
                              "keyword argument")
                 .arg(result.addedParameters.join(QStringLiteral(", "))));
  }
}

} // namespace

bool DefinitionValidation::hasErrors() const
{
  for (const auto& issue : issues) {
    if (issue.severity == DefinitionIssue::Severity::Error) {
      return true;
    }
  }
  return false;
}

QStringList DefinitionValidation::messages(
  DefinitionIssue::Severity severity) const
{
  QStringList out;
  for (const auto& issue : issues) {
    if (issue.severity == severity) {
      out.append(issue.message);
    }
  }
  return out;
}

DefinitionSchema definitionSchema(const QString& json)
{
  QJsonObject obj = parseObject(json, nullptr);
  return obj.value(QStringLiteral("schemaVersion")).toInt(1) == 2
           ? DefinitionSchema::V2
           : DefinitionSchema::V1;
}

NodeShape definitionShape(const QString& json)
{
  QJsonObject obj = parseObject(json, nullptr);
  const bool v2 = obj.value(QStringLiteral("schemaVersion")).toInt(1) == 2;
  if (v2 && obj.value(QStringLiteral("inputs")).toArray().isEmpty()) {
    return NodeShape::Source;
  }
  return NodeShape::Transform;
}

QMap<QString, QString> parameterDeclaredTypes(const QString& json,
                                              bool includeDatasets)
{
  QMap<QString, QString> types;
  QJsonObject obj = parseObject(json, nullptr);
  for (const auto& value : obj.value(QStringLiteral("parameters")).toArray()) {
    QJsonObject param = value.toObject();
    QString name = param.value(QStringLiteral("name")).toString();
    QString type = param.value(QStringLiteral("type")).toString();
    if (name.isEmpty()) {
      continue;
    }
    if (!includeDatasets && type == QLatin1String("dataset")) {
      continue;
    }
    types.insert(name, type);
  }
  return types;
}

DefinitionValidation validateNodeDefinition(const QString& currentJson,
                                            const QString& candidateJson,
                                            NodeShape shape,
                                            DefinitionSchema schema,
                                            DefinitionTarget target)
{
  DefinitionValidation result;

  QString parseError;
  QJsonObject candidate = parseObject(candidateJson, &parseError);
  if (!parseError.isEmpty()) {
    addError(result, parseError);
    return result;
  }
  QJsonObject current = parseObject(currentJson, nullptr);

  // Blank text and a document cleared down to a bare "{}" are the same
  // loss: the node's identity and every parameter go with it. The form
  // cannot show text that doesn't parse, so clearing the raw editor and
  // switching back produces the "{}" spelling rather than the blank one.
  if (candidate.isEmpty() && !current.isEmpty()) {
    addError(result, QStringLiteral("the description cannot be emptied"));
    return result;
  }
  if (candidate.isEmpty()) {
    // Node never had a description and still doesn't, nothing to check.
    return result;
  }

  if (target == DefinitionTarget::File) {
    // A file is bound to no node class: schema, shape, ports and widget
    // are all free to change. The parameter diff describes what happens
    // to a live node's values, which a file doesn't have.
    validateParameters(result, candidate);
    return result;
  }

  // --- identity: fixed by the C++ class the node was built as --------
  DefinitionSchema candidateSchema =
    candidate.value(QStringLiteral("schemaVersion")).toInt(1) == 2
      ? DefinitionSchema::V2
      : DefinitionSchema::V1;
  if (candidateSchema != schema) {
    addError(result,
             QStringLiteral("\"schemaVersion\" cannot be changed on an "
                            "existing node — the schema picks the node "
                            "class at creation time. Add a new node from "
                            "the menu instead."));
    // Every remaining check assumes the node's own schema, so stop here.
    return result;
  }

  if (schema == DefinitionSchema::V2) {
    bool hasInputs =
      !candidate.value(QStringLiteral("inputs")).toArray().isEmpty();
    if (shape == NodeShape::Source && hasInputs) {
      addError(result,
               QStringLiteral("this is a source node: it cannot declare "
                              "\"inputs\". Add a transform from the menu "
                              "instead."));
    } else if (shape == NodeShape::Transform && !hasInputs) {
      addError(result,
               QStringLiteral("this is a transform node: it must declare at "
                              "least one entry in \"inputs\". Add a source "
                              "from the menu instead."));
    }
  }

  // --- port topology: frozen -----------------------------------------
  // Nothing in the pipeline can remove a port, or the links hanging off
  // one, so a live node's ports are fixed for now.
  const QString portReason =
    QStringLiteral("a node's ports are fixed once it exists");
  if (schema == DefinitionSchema::V2) {
    checkFrozenKey(result, current, candidate, QStringLiteral("inputs"),
                   portReason);
    checkFrozenKey(result, current, candidate, QStringLiteral("outputs"),
                   portReason);
  } else {
    checkFrozenKey(result, current, candidate, QStringLiteral("results"),
                   portReason);
    checkFrozenKey(result, current, candidate, QStringLiteral("children"),
                   portReason);
    checkFrozenKey(result, current, candidate, QStringLiteral("inputType"),
                   portReason);
    checkFrozenKey(result, current, candidate, QStringLiteral("outputType"),
                   portReason);
    if (datasetParameterNames(current) != datasetParameterNames(candidate)) {
      addError(result,
               QStringLiteral("\"dataset\" parameters cannot be added or "
                              "removed: each one is an input port, and %1")
                 .arg(portReason));
    }
  }

  // The custom widget replaces the whole Parameters tab and decides
  // whether the node needs live input data to be edited at all, so it
  // is bound at editor-construction time.
  checkFrozenKey(result, current, candidate, QStringLiteral("widget"),
                 QStringLiteral("reopen the editor to pick up a different "
                                "custom widget"));

  validateParameters(result, candidate);
  if (!result.hasErrors()) {
    diffParameters(result, currentJson, candidateJson);
  }

  return result;
}

QMap<QString, QVariant> mergeParameterValues(
  const QMap<QString, QVariant>& previousValues,
  const QMap<QString, QString>& previousTypes,
  const QMap<QString, QVariant>& newDefaults,
  const QMap<QString, QString>& newTypes,
  const QMap<QString, QJsonArray>& newEnumOptions, QStringList* resetNames)
{
  QMap<QString, QVariant> merged = newDefaults;

  for (auto it = newTypes.constBegin(); it != newTypes.constEnd(); ++it) {
    const QString& name = it.key();
    const QString& type = it.value();

    auto previous = previousValues.constFind(name);
    if (previous == previousValues.constEnd()) {
      continue; // Brand new parameter: the declared default stands.
    }
    if (previousTypes.value(name) != type) {
      if (resetNames) {
        resetNames->append(name);
      }
      continue;
    }
    if (type == QLatin1String("enumeration")) {
      // Options can be rewritten without the type changing; a value
      // that is no longer offered has to go back to the default.
      bool stillOffered = false;
      for (const auto& option : newEnumOptions.value(name)) {
        QJsonObject entry = option.toObject();
        if (entry.isEmpty()) {
          continue;
        }
        if (entry.constBegin().value().toVariant() == previous.value()) {
          stillOffered = true;
          break;
        }
      }
      if (!stillOffered) {
        if (resetNames) {
          resetNames->append(name);
        }
        continue;
      }
    }
    merged.insert(name, previous.value());
  }

  return merged;
}

} // namespace pipeline
} // namespace tomviz
