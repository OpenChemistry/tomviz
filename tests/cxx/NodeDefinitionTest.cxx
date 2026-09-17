/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "InputPort.h"
#include "NodeDefinitionEdits.h"
#include "NodeDefinitionValidator.h"
#include "NodeDefinitionWidget.h"
#include "OutputPort.h"
#include "EnumOptions.h"
#include "NodePropertiesWidget.h"
#include "Pipeline.h"
#include "PythonNodeEditorWidget.h"
#include "SourceNode.h"
#include "Utilities.h"
#include "transforms/LegacyPythonTransform.h"
#include "transforms/PythonTransform.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextEdit>

#include "TomvizTest.h"

using namespace tomviz::pipeline;

namespace {

const char* kV2Transform = R"({
  "schemaVersion": 2,
  "name": "InvertData",
  "label": "Invert Data",
  "inputs":  [{"name": "volume", "type": "ImageData"}],
  "outputs": [{"name": "volume", "type": "ImageData"}],
  "parameters": [
    {"name": "scale", "label": "Scale", "type": "double", "default": 1.0}
  ]
})";

const char* kV2Source = R"({
  "schemaVersion": 2,
  "name": "ConstantDataset",
  "outputs": [{"name": "volume", "type": "ImageData", "persistent": true}],
  "parameters": [
    {"name": "value", "type": "double", "default": 0.0}
  ]
})";

const char* kV1Transform = R"({
  "name": "AddConstant",
  "label": "Add Constant",
  "inputType": "ImageData",
  "results": [{"name": "stats", "type": "table"}],
  "parameters": [
    {"name": "constant", "type": "double", "default": 0.0}
  ]
})";

/// Replace the whole "parameters" array of @a json.
QString withParameters(const char* json, const QString& parameters)
{
  QJsonObject obj = QJsonDocument::fromJson(QByteArray(json)).object();
  obj["parameters"] =
    QJsonDocument::fromJson(parameters.toUtf8()).array();
  return QString::fromUtf8(QJsonDocument(obj).toJson());
}

/// Set (or, with an undefined value, remove) a single top-level key.
QString withKey(const char* json, const QString& key, const QJsonValue& value)
{
  QJsonObject obj = QJsonDocument::fromJson(QByteArray(json)).object();
  if (value.isUndefined()) {
    obj.remove(key);
  } else {
    obj[key] = value;
  }
  return QString::fromUtf8(QJsonDocument(obj).toJson());
}

DefinitionValidation checkV2Transform(const QString& candidate)
{
  return validateNodeDefinition(kV2Transform, candidate, NodeShape::Transform,
                                DefinitionSchema::V2);
}

} // namespace

TEST(NodeDefinitionTest, SchemaDetection)
{
  EXPECT_EQ(definitionSchema(kV2Transform), DefinitionSchema::V2);
  EXPECT_EQ(definitionSchema(kV1Transform), DefinitionSchema::V1);
  EXPECT_EQ(definitionSchema(QString()), DefinitionSchema::V1);
  EXPECT_EQ(definitionSchema("not json at all"), DefinitionSchema::V1);
}

TEST(NodeDefinitionTest, UnchangedDescriptionIsClean)
{
  auto result = checkV2Transform(kV2Transform);
  EXPECT_FALSE(result.hasErrors());
  EXPECT_TRUE(result.issues.isEmpty());
}

TEST(NodeDefinitionTest, ReformattingIsNotAChange)
{
  // Same content, different whitespace and key order.
  QString reformatted = QString::fromUtf8(
    QJsonDocument(QJsonDocument::fromJson(QByteArray(kV2Transform)).object())
      .toJson(QJsonDocument::Compact));
  auto result = checkV2Transform(reformatted);
  EXPECT_FALSE(result.hasErrors());
  EXPECT_TRUE(result.addedParameters.isEmpty());
  EXPECT_TRUE(result.removedParameters.isEmpty());
}

// --- identity: what a live node can never become ---------------------

TEST(NodeDefinitionTest, SchemaVersionIsFrozen)
{
  auto result = checkV2Transform(withKey(kV2Transform, "schemaVersion", 1));
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, TransformCannotDropItsInputs)
{
  auto result = checkV2Transform(withKey(kV2Transform, "inputs", QJsonArray()));
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, SourceCannotGrowInputs)
{
  QJsonArray inputs{ QJsonObject{ { "name", "volume" },
                                  { "type", "ImageData" } } };
  auto result = validateNodeDefinition(
    kV2Source, withKey(kV2Source, "inputs", inputs), NodeShape::Source,
    DefinitionSchema::V2);
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, CustomWidgetIsFrozen)
{
  auto result = checkV2Transform(withKey(kV2Transform, "widget", "SomeWidget"));
  EXPECT_TRUE(result.hasErrors());
}

// --- port topology ---------------------------------------------------

TEST(NodeDefinitionTest, AddingAnOutputIsRejected)
{
  QJsonArray outputs{
    QJsonObject{ { "name", "volume" }, { "type", "ImageData" } },
    QJsonObject{ { "name", "stats" }, { "type", "Table" } }
  };
  auto result = checkV2Transform(withKey(kV2Transform, "outputs", outputs));
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, RenamingAnInputIsRejected)
{
  QJsonArray inputs{ QJsonObject{ { "name", "source" },
                                  { "type", "ImageData" } } };
  auto result = checkV2Transform(withKey(kV2Transform, "inputs", inputs));
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, RetypingAPortIsRejected)
{
  QJsonArray outputs{ QJsonObject{ { "name", "volume" },
                                   { "type", "TiltSeries" } } };
  auto result = checkV2Transform(withKey(kV2Transform, "outputs", outputs));
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, AbsentAndEmptyOutputsAreTheSameThing)
{
  // The source has no "inputs" key at all; an explicit empty array must
  // not read as a change.
  auto result = validateNodeDefinition(
    kV2Source, withKey(kV2Source, "inputs", QJsonArray()), NodeShape::Source,
    DefinitionSchema::V2);
  EXPECT_FALSE(result.hasErrors());
}

TEST(NodeDefinitionTest, LegacyResultsAreFrozen)
{
  auto result = validateNodeDefinition(
    kV1Transform, withKey(kV1Transform, "results", QJsonArray()),
    NodeShape::Transform, DefinitionSchema::V1);
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, LegacyPortTypeOverridesAreFrozen)
{
  auto result = validateNodeDefinition(
    kV1Transform, withKey(kV1Transform, "inputType", "TiltSeries"),
    NodeShape::Transform, DefinitionSchema::V1);
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, LegacyDatasetParametersAreFrozen)
{
  // A "dataset" parameter is an input port in the v1 schema, so adding
  // one has to be rejected even though it lives in "parameters".
  QString candidate = withParameters(
    kV1Transform,
    R"([{"name": "constant", "type": "double", "default": 0.0},
        {"name": "other", "type": "dataset"}])");
  auto result = validateNodeDefinition(kV1Transform, candidate,
                                       NodeShape::Transform,
                                       DefinitionSchema::V1);
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, LegacyParametersAreOtherwiseEditable)
{
  QString candidate = withParameters(
    kV1Transform,
    R"([{"name": "constant", "type": "double", "default": 0.0},
        {"name": "clamp", "type": "bool", "default": true}])");
  auto result = validateNodeDefinition(kV1Transform, candidate,
                                       NodeShape::Transform,
                                       DefinitionSchema::V1);
  EXPECT_FALSE(result.hasErrors());
  EXPECT_EQ(result.addedParameters, QStringList{ "clamp" });
}

// --- parameter wellformedness ---------------------------------------

TEST(NodeDefinitionTest, MalformedJsonIsAnError)
{
  auto result = checkV2Transform("{ \"name\": ");
  EXPECT_TRUE(result.hasErrors());
}

TEST(NodeDefinitionTest, EmptyingTheDescriptionIsAnError)
{
  auto result = checkV2Transform("   ");
  EXPECT_TRUE(result.hasErrors());

  // Clearing the raw editor and switching back to the form spells the
  // same loss as "{}", because the form cannot hold unparseable text.
  EXPECT_TRUE(checkV2Transform("{}").hasErrors());

  // A node that never had a description is not being emptied.
  EXPECT_FALSE(
    validateNodeDefinition("", "{}", NodeShape::Transform,
                           DefinitionSchema::V1)
      .hasErrors());
}

TEST(NodeDefinitionTest, DuplicateParameterNamesAreAnError)
{
  QString candidate = withParameters(
    kV2Transform,
    R"([{"name": "scale", "type": "double", "default": 1.0},
        {"name": "scale", "type": "int", "default": 2}])");
  EXPECT_TRUE(checkV2Transform(candidate).hasErrors());
}

TEST(NodeDefinitionTest, EnumerationWithoutOptionsIsAnError)
{
  QString candidate = withParameters(
    kV2Transform, R"([{"name": "mode", "type": "enumeration", "default": 0}])");
  EXPECT_TRUE(checkV2Transform(candidate).hasErrors());
}

TEST(NodeDefinitionTest, ParameterWithoutTypeIsAnError)
{
  QString candidate =
    withParameters(kV2Transform, R"([{"name": "scale", "default": 1.0}])");
  EXPECT_TRUE(checkV2Transform(candidate).hasErrors());
}

TEST(NodeDefinitionTest, NamelessLayoutMarkerIsAllowed)
{
  QString candidate = withParameters(
    kV2Transform, R"([{"type": "xyz_header"},
                      {"name": "scale", "type": "double", "default": 1.0}])");
  EXPECT_FALSE(checkV2Transform(candidate).hasErrors());
}

TEST(NodeDefinitionTest, NamelessValueParameterIsAnError)
{
  QString candidate =
    withParameters(kV2Transform, R"([{"type": "double", "default": 1.0}])");
  EXPECT_TRUE(checkV2Transform(candidate).hasErrors());
}

TEST(NodeDefinitionTest, UnrenderableParameterTypeIsOnlyAWarning)
{
  QString candidate = withParameters(
    kV2Transform, R"([{"name": "map", "type": "label_map"}])");
  auto result = checkV2Transform(candidate);
  EXPECT_FALSE(result.hasErrors());
  EXPECT_FALSE(result.messages(DefinitionIssue::Severity::Warning).isEmpty());
}

// --- parameter diff --------------------------------------------------

TEST(NodeDefinitionTest, ParameterDiffReportsAddedRemovedAndRetyped)
{
  QString candidate = withParameters(
    kV2Transform,
    R"([{"name": "scale", "type": "int", "default": 1},
        {"name": "offset", "type": "double", "default": 0.0}])");
  auto result = checkV2Transform(candidate);
  EXPECT_FALSE(result.hasErrors());
  EXPECT_EQ(result.addedParameters, QStringList{ "offset" });
  EXPECT_EQ(result.retypedParameters, QStringList{ "scale" });
  EXPECT_TRUE(result.removedParameters.isEmpty());

  QString dropped = withParameters(kV2Transform, "[]");
  auto droppedResult = checkV2Transform(dropped);
  EXPECT_FALSE(droppedResult.hasErrors());
  EXPECT_EQ(droppedResult.removedParameters, QStringList{ "scale" });
}

TEST(NodeDefinitionTest, DeclaredTypesSkipLayoutAndDatasetEntries)
{
  auto types = parameterDeclaredTypes(
    R"({"parameters": [{"type": "xyz_header"},
                       {"name": "other", "type": "dataset"},
                       {"name": "scale", "type": "double"}]})");
  EXPECT_EQ(types.size(), 1);
  EXPECT_EQ(types.value("scale"), QString("double"));

  auto withDatasets = parameterDeclaredTypes(
    R"({"parameters": [{"name": "other", "type": "dataset"}]})", true);
  EXPECT_EQ(withDatasets.value("other"), QString("dataset"));
}

// --- value carry-over ------------------------------------------------

TEST(NodeDefinitionTest, MergeKeepsValuesOfUnchangedParameters)
{
  QMap<QString, QVariant> previousValues{ { "scale", 7.5 },
                                          { "gone", 3 } };
  QMap<QString, QString> previousTypes{ { "scale", "double" },
                                        { "gone", "int" } };
  QMap<QString, QVariant> newDefaults{ { "scale", 1.0 }, { "fresh", 2 } };
  QMap<QString, QString> newTypes{ { "scale", "double" }, { "fresh", "int" } };

  QStringList reset;
  auto merged = mergeParameterValues(previousValues, previousTypes,
                                     newDefaults, newTypes, {}, &reset);

  EXPECT_EQ(merged.value("scale"), QVariant(7.5));
  EXPECT_EQ(merged.value("fresh"), QVariant(2));
  EXPECT_FALSE(merged.contains("gone"));
  EXPECT_TRUE(reset.isEmpty());
}

TEST(NodeDefinitionTest, MergeResetsRetypedParameters)
{
  QMap<QString, QVariant> previousValues{ { "scale", 7.5 } };
  QMap<QString, QString> previousTypes{ { "scale", "double" } };
  QMap<QString, QVariant> newDefaults{ { "scale", 1 } };
  QMap<QString, QString> newTypes{ { "scale", "int" } };

  QStringList reset;
  auto merged = mergeParameterValues(previousValues, previousTypes,
                                     newDefaults, newTypes, {}, &reset);

  EXPECT_EQ(merged.value("scale"), QVariant(1));
  EXPECT_EQ(reset, QStringList{ "scale" });
}

TEST(NodeDefinitionTest, MergeResetsEnumValueNoLongerOffered)
{
  QJsonArray options{ QJsonObject{ { "Bicubic", "bicubic" } },
                      QJsonObject{ { "Nearest", "nearest" } } };
  QMap<QString, QJsonArray> enumOptions{ { "mode", options } };
  QMap<QString, QString> types{ { "mode", "enumeration" } };

  QStringList reset;
  auto kept = mergeParameterValues({ { "mode", "nearest" } }, types,
                                   { { "mode", "bicubic" } }, types,
                                   enumOptions, &reset);
  EXPECT_EQ(kept.value("mode"), QVariant("nearest"));
  EXPECT_TRUE(reset.isEmpty());

  reset.clear();
  auto dropped = mergeParameterValues({ { "mode", "lanczos" } }, types,
                                      { { "mode", "bicubic" } }, types,
                                      enumOptions, &reset);
  EXPECT_EQ(dropped.value("mode"), QVariant("bicubic"));
  EXPECT_EQ(reset, QStringList{ "mode" });
}

// --- form-mode JSON mutators -----------------------------------------

TEST(NodeDefinitionTest, JsonValuesRoundTrip)
{
  auto roundTrip = [](const char* text) {
    return formatJsonValue(parseJsonValue(QString::fromUtf8(text)));
  };
  EXPECT_EQ(roundTrip("1.5"), QString("1.5"));
  EXPECT_EQ(roundTrip("42"), QString("42"));
  EXPECT_EQ(roundTrip("true"), QString("true"));
  EXPECT_EQ(roundTrip("\"nearest\""), QString("\"nearest\""));
  EXPECT_EQ(roundTrip("[128,128,128]"), QString("[128,128,128]"));

  // Blank and unparseable input both mean "no value", which callers
  // turn into "remove the key".
  EXPECT_TRUE(parseJsonValue("   ").isUndefined());
  EXPECT_TRUE(parseJsonValue("not json").isUndefined());
  EXPECT_TRUE(formatJsonValue(QJsonValue(QJsonValue::Undefined)).isEmpty());
}

TEST(NodeDefinitionTest, SetOrClearRemovesRatherThanWritingBlanks)
{
  QJsonObject obj{ { "label", "Scale" }, { "supportsCancel", true } };

  setOrClear(obj, "label", QString());
  EXPECT_FALSE(obj.contains("label"));

  setOrClearBool(obj, "supportsCancel", false, false);
  EXPECT_FALSE(obj.contains("supportsCancel"));

  setOrClearBool(obj, "externalOnly", true, false);
  EXPECT_TRUE(obj.value("externalOnly").toBool());

  setOrClearValue(obj, "default", QJsonValue(QJsonValue::Undefined));
  EXPECT_FALSE(obj.contains("default"));
}

TEST(NodeDefinitionTest, ParameterEditPreservesKeysTheFormDoesNotRender)
{
  // Every one of these is read somewhere in the codebase but has no
  // control in the form, so a rebuild-from-fields editor would drop it.
  QJsonObject param{
    { "name", "scale" },      { "label", "Scale" },
    { "type", "double" },     { "default", 1.0 },
    { "filter", "advanced" }, { "bindToSink", "sink.volume" },
    { "input", "volume" },    { "apply_to_each_array", true },
    { "show_apply_all", true }
  };

  auto fields = readParameterFields(param);
  fields.label = "Scale Factor";
  applyParameterFields(param, fields);

  EXPECT_EQ(param.value("label").toString(), QString("Scale Factor"));
  EXPECT_EQ(param.value("filter").toString(), QString("advanced"));
  EXPECT_EQ(param.value("bindToSink").toString(), QString("sink.volume"));
  EXPECT_EQ(param.value("input").toString(), QString("volume"));
  EXPECT_TRUE(param.value("apply_to_each_array").toBool());
  EXPECT_TRUE(param.value("show_apply_all").toBool());
}

TEST(NodeDefinitionTest, ParameterEditRoundTripsEveryShippedDescriptor)
{
  QDir dir(TOMVIZ_PYTHON_DIR);
  const auto files = dir.entryList({ "*.json" }, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty()) << "no descriptors found in "
                                << TOMVIZ_PYTHON_DIR;

  int parametersChecked = 0;
  for (const QString& name : files) {
    QFile file(dir.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << name.toStdString();
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    ASSERT_TRUE(doc.isObject()) << name.toStdString();

    for (const auto& value :
         doc.object().value("parameters").toArray()) {
      const QJsonObject original = value.toObject();
      QJsonObject edited = original;
      applyParameterFields(edited, readParameterFields(original));
      ++parametersChecked;

      // Read-then-write must be the identity, except that an empty
      // string and an absent key mean the same thing and collapse.
      for (auto it = original.constBegin(); it != original.constEnd();
           ++it) {
        if (it.value().isString() && it.value().toString().isEmpty()) {
          continue;
        }
        EXPECT_EQ(edited.value(it.key()), it.value())
          << name.toStdString() << " parameter "
          << original.value("name").toString().toStdString() << " key "
          << it.key().toStdString();
      }
      for (auto it = edited.constBegin(); it != edited.constEnd(); ++it) {
        EXPECT_TRUE(original.contains(it.key()))
          << name.toStdString() << " gained key "
          << it.key().toStdString();
      }
    }
  }
  EXPECT_GT(parametersChecked, 200);
}

TEST(NodeDefinitionTest, EnumerationValuesAndDefaultsResolveByValueAndIndex)
{
  // Values that are not their own indices, as Connected Components'
  // connectivity has them
  QJsonArray options{ QJsonObject{ { "Faces", 1 } },
                      QJsonObject{ { "Edges", 2 } },
                      QJsonObject{ { "Corners", 3 } } };
  using tomviz::pipeline::PythonNodeUtils::enumOptionIndex;
  using tomviz::pipeline::PythonNodeUtils::resolveEnumDefault;
  using tomviz::pipeline::PythonNodeUtils::resolveEnumValue;

  // A stored value is the value, whether it is written as int or double
  EXPECT_EQ(resolveEnumValue(QJsonValue(1), options), QVariant(1));
  EXPECT_EQ(resolveEnumValue(QJsonValue(3.0), options), QVariant(3));
  // A number matching no value is an index, from files that saved one
  EXPECT_EQ(resolveEnumValue(QJsonValue(0), options), QVariant(1));
  EXPECT_FALSE(resolveEnumValue(QJsonValue(7), options).isValid());
  // The declared default is an index first
  EXPECT_EQ(resolveEnumDefault(QJsonValue(0), options), QVariant(1));
  EXPECT_EQ(resolveEnumDefault(QJsonValue(2), options), QVariant(3));
  EXPECT_EQ(enumOptionIndex(QVariant(2.0), options), 1);
  EXPECT_EQ(enumOptionIndex(QVariant(9), options), -1);

  QJsonArray named{ QJsonObject{ { "Bicubic", "bicubic" } },
                    QJsonObject{ { "Nearest", "nearest" } } };
  EXPECT_EQ(resolveEnumValue(QJsonValue("nearest"), named),
            QVariant("nearest"));
  EXPECT_EQ(resolveEnumDefault(QJsonValue(1), named), QVariant("nearest"));
  EXPECT_EQ(enumOptionIndex(QVariant("bicubic"), named), 0);

  // The generated form opens on the stored value's option and reads it
  // back as the value. No "label" on purpose: the builder used to lose a
  // parameter's name when the optional key was missing.
  tomviz_test::ensureQApp();
  QString json = withParameters(
    kV2Transform,
    R"([{"name": "connectivity", "type": "enumeration", "default": 0,
         "options": [{"Faces": 1}, {"Edges": 2}, {"Corners": 3}]}])");
  tomviz::pipeline::NodePropertiesWidget form(json, { { "connectivity", 2 } });
  EXPECT_EQ(form.values().value("connectivity"), QVariant(2));
  tomviz::pipeline::NodePropertiesWidget fresh(json, {});
  EXPECT_EQ(fresh.values().value("connectivity"), QVariant(1));
}

TEST(NodeDefinitionTest, MalformedEnumerationOptionsAreAnError)
{
  // An empty option object is what you get from writing an undefined
  // value into a QJsonObject; ParameterInterfaceBuilder used to read
  // keys()[0] off the end of the list and segfault on it.
  QString empty = withParameters(
    kV2Transform,
    R"([{"name": "mode", "type": "enumeration", "options": [{}]}])");
  EXPECT_TRUE(checkV2Transform(empty).hasErrors());

  QString bare = withParameters(
    kV2Transform,
    R"([{"name": "mode", "type": "enumeration", "options": [1, 2]}])");
  EXPECT_TRUE(checkV2Transform(bare).hasErrors());

  QString twoKeys = withParameters(
    kV2Transform,
    R"([{"name": "mode", "type": "enumeration",
         "options": [{"A": 0, "B": 1}]}])");
  EXPECT_TRUE(checkV2Transform(twoKeys).hasErrors());

  QString ok = withParameters(
    kV2Transform,
    R"([{"name": "mode", "type": "enumeration",
         "options": [{"Box": 0}, {"Ball": 1}]}])");
  EXPECT_FALSE(checkV2Transform(ok).hasErrors());
}

TEST(NodeDefinitionTest, ShippedEnumerationOptionsAllValidate)
{
  // The new single-pair rule must not reject anything already on disk.
  QDir dir(TOMVIZ_PYTHON_DIR);
  const auto files = dir.entryList({ "*.json" }, QDir::Files, QDir::Name);
  ASSERT_FALSE(files.isEmpty());

  int enumerations = 0;
  for (const QString& name : files) {
    QFile file(dir.filePath(name));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QString json = QString::fromUtf8(doc.toJson());
    NodeShape shape =
      doc.object().value("inputs").toArray().isEmpty() &&
          doc.object().value("schemaVersion").toInt(1) == 2
        ? NodeShape::Source
        : NodeShape::Transform;
    auto result =
      validateNodeDefinition(json, json, shape, definitionSchema(json));
    for (const auto& p : doc.object().value("parameters").toArray()) {
      if (p.toObject().value("type").toString() == "enumeration") {
        ++enumerations;
      }
    }
    EXPECT_FALSE(result.hasErrors())
      << name.toStdString() << ": "
      << result.messages(DefinitionIssue::Severity::Error)
           .join("; ")
           .toStdString();
  }
  EXPECT_GT(enumerations, 30);
}

// --- type inference must survive a definition edit --------------------

TEST(NodeDefinitionTest, ReconfigureKeepsInferredOutputType)
{
  // A v1 transform declaring ImageData in and out infers its output type
  // from whatever it is connected to. Re-applying the description must
  // not knock that back down to the declared base type.
  const char* description = R"({
    "name": "AddConstant",
    "label": "Add Constant",
    "inputType": "ImageData",
    "outputType": "ImageData",
    "parameters": [{"name": "constant", "type": "double", "default": 0.0}]
  })";

  Pipeline pipeline;
  auto* source = new SourceNode;
  source->setLabel("Source");
  auto* sourceOut = source->addOutput("volume", PortType::Volume);
  pipeline.addNode(source);

  auto* transform = new LegacyPythonTransform;
  transform->setJSONDescription(QString::fromUtf8(description));
  pipeline.addNode(transform);

  ASSERT_TRUE(pipeline.createLink(sourceOut, transform->inputPort("volume")));
  auto* out = transform->outputPort("volume");
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->declaredType(), PortType::ImageData);
  EXPECT_EQ(out->type(), PortType::Volume) << "inference did not run";

  // Exactly what the Definition tab does on Apply.
  transform->reconfigureDescription(QString::fromUtf8(description));

  EXPECT_EQ(out->type(), PortType::Volume)
    << "re-applying the description reset the inferred effective type";
}

TEST(NodeDefinitionTest, SchemaV2ReconfigureKeepsInferredOutputType)
{
  // Same guarantee for the v2 shells. Their reconfigure() re-parses
  // without touching ports, but assert it rather than trusting it.
  const char* description = R"({
    "schemaVersion": 2,
    "name": "InvertData",
    "inputs":  [{"name": "volume", "type": "ImageData"}],
    "outputs": [{"name": "volume", "type": "ImageData"}],
    "parameters": [{"name": "scale", "type": "double", "default": 1.0}]
  })";

  Pipeline pipeline;
  auto* source = new SourceNode;
  auto* sourceOut = source->addOutput("volume", PortType::Volume);
  pipeline.addNode(source);

  auto* transform = new PythonTransform;
  transform->setJSONDescription(QString::fromUtf8(description));
  pipeline.addNode(transform);

  ASSERT_TRUE(pipeline.createLink(sourceOut, transform->inputPort("volume")));
  auto* out = transform->outputPort("volume");
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->type(), PortType::Volume) << "inference did not run";

  transform->reconfigureDescription(QString::fromUtf8(description));
  EXPECT_EQ(out->type(), PortType::Volume);

  // A parameter edit is the common case — it must not disturb ports.
  QString edited = withParameters(
    description,
    R"([{"name": "scale", "type": "double", "default": 1.0},
        {"name": "offset", "type": "double", "default": 0.0}])");
  transform->reconfigureDescription(edited);
  EXPECT_EQ(out->type(), PortType::Volume);
  EXPECT_EQ(transform->outputPorts().size(), 1) << "ports were duplicated";
  EXPECT_EQ(transform->inputPorts().size(), 1) << "ports were duplicated";
}

// --- Save Script --------------------------------------------------------

TEST(NodeDefinitionTest, SaveScriptWritesTheEditedScriptAndDescription)
{
  tomviz_test::ensureQApp();

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());

  const QString original = tomviz::readInJSONDescription("GaussianFilter");
  ASSERT_FALSE(original.isEmpty());

  Pipeline pipeline;
  auto* node = new LegacyPythonTransform();
  node->setJSONDescription(original);
  node->setScript("def transform(dataset):\n    pass\n");
  pipeline.addNode(node);

  auto* editor = node->createPropertiesWidget(&pipeline, nullptr);
  ASSERT_NE(editor, nullptr);
  auto* python = qobject_cast<PythonNodeEditorWidget*>(editor);
  ASSERT_NE(python, nullptr);

  // Edit both tabs without applying: what lands on disk must be these
  // edits, not the descriptions the node is still running.
  auto* scriptEdit = editor->findChild<QTextEdit*>();
  ASSERT_NE(scriptEdit, nullptr);
  const QString editedScript =
    // Split so the "d" isn't swallowed into the \xA9 escape. Non-ASCII on
    // purpose: the old 2.x saveScript() wrote Latin-1 and mangled this.
    QString::fromUtf8("def transform(dataset):\n    # \xC3\xA9" "dited\n");
  scriptEdit->setPlainText(editedScript);

  auto* definition = editor->findChild<NodeDefinitionWidget*>();
  ASSERT_NE(definition, nullptr);
  auto* rawEditor = definition->findChild<QTextEdit*>();
  ASSERT_NE(rawEditor, nullptr);
  QJsonObject edited = QJsonDocument::fromJson(original.toUtf8()).object();
  edited["label"] = "Edited Label";
  const QString editedJson =
    QString::fromUtf8(QJsonDocument(edited).toJson());
  rawEditor->setPlainText(editedJson);
  definition->flushPendingValidation();
  ASSERT_TRUE(definition->isValid());

  const QString scriptPath = dir.filePath("my.operator.py");
  ASSERT_TRUE(python->saveScriptTo(scriptPath));

  // The description lands beside it, keeping the full stem.
  const QString jsonPath = dir.filePath("my.operator.json");
  EXPECT_EQ(PythonNodeEditorWidget::descriptionPathFor(scriptPath), jsonPath);
  ASSERT_TRUE(QFile::exists(scriptPath));
  ASSERT_TRUE(QFile::exists(jsonPath));

  QFile written(scriptPath);
  ASSERT_TRUE(written.open(QIODevice::ReadOnly));
  EXPECT_EQ(QString::fromUtf8(written.readAll()), editedScript)
    << "saved the node's script instead of the edited one";
  // Windows won't let us rewrite or remove a file we are still holding open.
  written.close();

  QFile writtenJson(jsonPath);
  ASSERT_TRUE(writtenJson.open(QIODevice::ReadOnly));
  const QJsonObject reloaded =
    QJsonDocument::fromJson(writtenJson.readAll()).object();
  writtenJson.close();
  EXPECT_EQ(reloaded.value("label").toString(), QString("Edited Label"))
    << "saved the node's description instead of the edited one";
  // The rest of the descriptor has to survive the round trip intact.
  EXPECT_EQ(reloaded.value("name").toString(),
            QJsonDocument::fromJson(original.toUtf8())
              .object()
              .value("name")
              .toString());

  // withDescription=false leaves any existing .json alone.
  ASSERT_TRUE(QFile::remove(jsonPath));
  ASSERT_TRUE(python->saveScriptTo(scriptPath, false));
  EXPECT_FALSE(QFile::exists(jsonPath));

  delete editor;
}
