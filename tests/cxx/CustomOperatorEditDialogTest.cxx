/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "CustomOperatorEditDialog.h"
#include "Utilities.h"
#include "pipeline/NodeDefinitionWidget.h"
#include "pipeline/NodeEditDialog.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PythonNodeEditorWidget.h"
#include "pipeline/PythonScriptEdit.h"
#include "pipeline/transforms/LegacyPythonTransform.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>

#include <memory>

#include "TomvizTest.h"

using tomviz::CustomOperatorDraft;
using tomviz::CustomOperatorEditDialog;
using tomviz::markDescriptionAsCopy;
using tomviz::sanitizedOperatorStem;
using tomviz::uniqueOperatorStem;
using tomviz::withDescriptionIdentity;
using tomviz::pipeline::NodeDefinitionWidget;
using tomviz::pipeline::PythonScriptEdit;

namespace {

const char* kScript = "def transform(self, volume, sigma=1.0):\n"
                      "    return volume\n";

const char* kTransform = R"({
  "schemaVersion": 2,
  "name": "Blur",
  "label": "Blur",
  "inputs": [{"name": "volume", "type": "ImageData"}],
  "outputs": [{"name": "volume", "type": "ImageData"}],
  "parameters": [{"name": "sigma", "type": "double", "default": 1.0}]
})";

// Source-shaped: no inputs. A live transform node could never adopt it.
const char* kSource = R"({
  "schemaVersion": 2,
  "name": "Noise",
  "outputs": [{"name": "volume", "type": "ImageData"}],
  "parameters": []
})";

bool writeFile(const QString& path, const QString& text)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(text.toUtf8());
  return true;
}

QString readFile(const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

QJsonObject parse(const QString& json)
{
  return QJsonDocument::fromJson(json.toUtf8()).object();
}

// Restores an environment variable to whatever the process had.
struct ScopedEnv
{
  explicit ScopedEnv(const char* variable)
    : name(variable), had(qEnvironmentVariableIsSet(variable)),
      old(qgetenv(variable))
  {
  }
  ~ScopedEnv()
  {
    if (had) {
      qputenv(name, old);
    } else {
      qunsetenv(name);
    }
  }
  const char* name;
  bool had;
  QByteArray old;
};

class CustomOperatorEditDialogTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    tomviz_test::ensureQApp();
    ASSERT_TRUE(dir.isValid());
    scriptPath = dir.path() + "/Blur.py";
    jsonPath = dir.path() + "/Blur.json";
    ASSERT_TRUE(writeFile(scriptPath, kScript));
    ASSERT_TRUE(writeFile(jsonPath, kTransform));
  }

  std::unique_ptr<CustomOperatorEditDialog> open(const QString& description)
  {
    auto dialog = std::make_unique<CustomOperatorEditDialog>(
      "Blur", scriptPath, description);
    EXPECT_TRUE(dialog->loadError().isEmpty())
      << dialog->loadError().toStdString();
    watch(dialog.get());
    dialog->show();
    return dialog;
  }

  std::unique_ptr<CustomOperatorEditDialog> openDraft(
    const QString& stem, const QString& script, const QString& description)
  {
    CustomOperatorDraft draft;
    draft.directory = dir.path();
    draft.stem = stem;
    draft.script = script;
    draft.description = description;
    auto dialog = std::make_unique<CustomOperatorEditDialog>(draft);
    watch(dialog.get());
    dialog->show();
    return dialog;
  }

  void watch(CustomOperatorEditDialog* dialog)
  {
    QObject::connect(dialog, &CustomOperatorEditDialog::saved,
                     [this]() { ++saves; });
  }

  static QTextEdit* jsonEditor(QDialog* dialog)
  {
    return dialog->findChild<QTextEdit*>("definitionRawEditor");
  }

  static PythonScriptEdit* scriptEditor(QDialog* dialog)
  {
    return dialog->findChild<PythonScriptEdit*>();
  }

  static QLineEdit* stemEdit(QDialog* dialog)
  {
    return dialog->findChild<QLineEdit*>("operatorStemEdit");
  }

  static QLabel* stemIssue(QDialog* dialog)
  {
    return dialog->findChild<QLabel*>("operatorStemIssue");
  }

  static QPushButton* saveButton(QDialog* dialog)
  {
    return dialog->findChild<QDialogButtonBox*>()->button(
      QDialogButtonBox::Save);
  }

  // Validation is debounced; the host flushes it before deciding.
  static void setDefinition(QDialog* dialog, const QString& json)
  {
    jsonEditor(dialog)->setPlainText(json);
    dialog->findChild<NodeDefinitionWidget*>()->flushPendingValidation();
  }

  QStringList filesInDir() const
  {
    return QDir(dir.path()).entryList(QDir::Files, QDir::Name);
  }

  QTemporaryDir dir;
  QString scriptPath;
  QString jsonPath;
  int saves = 0;
};

} // namespace

TEST_F(CustomOperatorEditDialogTest, shows_definition_and_script_tabs)
{
  auto dialog = open(jsonPath);
  auto* tabs = dialog->findChild<QTabWidget*>();
  ASSERT_NE(tabs, nullptr);
  ASSERT_EQ(tabs->count(), 2);
  EXPECT_EQ(tabs->tabText(0), "Definition");
  EXPECT_EQ(tabs->tabText(1), "Script");

  EXPECT_EQ(jsonEditor(dialog.get())->toPlainText(), kTransform);
  EXPECT_EQ(scriptEditor(dialog.get())->toPlainText(), kScript);
  EXPECT_EQ(stemEdit(dialog.get())->text(), "Blur");
  EXPECT_TRUE(dialog->isBacked());
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());
}

TEST_F(CustomOperatorEditDialogTest, save_writes_both_files_and_accepts)
{
  auto dialog = open(jsonPath);

  // Non-ASCII on purpose: an older save path wrote Latin-1 and mangled it.
  // The literal is split so the "d" is not read as part of the escape.
  const QString newScript = QString::fromUtf8(
    "def transform(self, volume, sigma=2.0):\n"
    "    # \xC3\xA9" "dited\n"
    "    return volume * 2\n");
  scriptEditor(dialog.get())->setPlainText(newScript);

  QJsonObject obj = parse(kTransform);
  obj["label"] = "Blurred";
  const QString newJson = QString::fromUtf8(QJsonDocument(obj).toJson());
  setDefinition(dialog.get(), newJson);
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());

  saveButton(dialog.get())->click();

  EXPECT_EQ(dialog->result(), QDialog::Accepted);
  EXPECT_FALSE(dialog->isVisible());
  EXPECT_EQ(saves, 1);
  EXPECT_EQ(readFile(scriptPath), newScript);
  EXPECT_EQ(readFile(jsonPath), newJson);
}

TEST_F(CustomOperatorEditDialogTest, broken_definition_disables_save)
{
  auto dialog = open(jsonPath);
  setDefinition(dialog.get(), "{ not json");
  EXPECT_FALSE(saveButton(dialog.get())->isEnabled());

  // Fixing it re-enables Save; nothing was written meanwhile.
  setDefinition(dialog.get(), kTransform);
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());
  EXPECT_EQ(readFile(jsonPath), kTransform);
  EXPECT_EQ(saves, 0);
}

TEST_F(CustomOperatorEditDialogTest, file_edits_may_change_shape_and_ports)
{
  // A live transform node rejects a source-shaped description; a file
  // has no such identity, so this must be allowed and saved.
  auto dialog = open(jsonPath);
  setDefinition(dialog.get(), kSource);
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());

  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);
  EXPECT_EQ(readFile(jsonPath), kSource);
}

TEST_F(CustomOperatorEditDialogTest, script_only_operator_gains_a_description)
{
  ASSERT_TRUE(QFile::remove(jsonPath));

  // No description file: the path is derived, the tab starts empty, and
  // saving leaves the operator script-only.
  auto dialog = open(QString());
  EXPECT_EQ(dialog->descriptionPath(), jsonPath);
  EXPECT_TRUE(jsonEditor(dialog.get())->toPlainText().isEmpty());
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());

  scriptEditor(dialog.get())->setPlainText("def transform(self, v):\n"
                                           "    return v\n");
  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);
  EXPECT_FALSE(QFile::exists(jsonPath));

  // Typing a description creates the file beside the script.
  auto second = open(QString());
  setDefinition(second.get(), kSource);
  ASSERT_TRUE(saveButton(second.get())->isEnabled());
  saveButton(second.get())->click();
  EXPECT_EQ(second->result(), QDialog::Accepted);
  EXPECT_EQ(readFile(jsonPath), kSource);
  EXPECT_EQ(saves, 2);
}

TEST_F(CustomOperatorEditDialogTest, description_path_keeps_a_dotted_stem)
{
  // "my.operator.py" pairs with "my.operator.json", not "my.json".
  EXPECT_EQ(tomviz::pipeline::PythonNodeEditorWidget::descriptionPathFor(
              dir.path() + "/my.operator.py"),
            dir.path() + "/my.operator.json");
}

TEST_F(CustomOperatorEditDialogTest, unreadable_script_is_reported)
{
  CustomOperatorEditDialog dialog("Missing", dir.path() + "/Missing.py",
                                  QString());
  EXPECT_FALSE(dialog.loadError().isEmpty());
}

TEST_F(CustomOperatorEditDialogTest, renaming_moves_both_files_on_save)
{
  auto dialog = open(jsonPath);
  stemEdit(dialog.get())->setText("Sharpen");
  EXPECT_FALSE(stemIssue(dialog.get())->isVisible());
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());

  const QString newScript = "def transform(self, volume):\n"
                            "    return volume\n";
  scriptEditor(dialog.get())->setPlainText(newScript);
  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);

  const QString movedScript = dir.path() + "/Sharpen.py";
  const QString movedJson = dir.path() + "/Sharpen.json";
  EXPECT_EQ(readFile(movedScript), newScript);
  EXPECT_EQ(readFile(movedJson), kTransform);
  EXPECT_FALSE(QFile::exists(scriptPath));
  EXPECT_FALSE(QFile::exists(jsonPath));
  EXPECT_EQ(dialog->scriptPath(), movedScript);
  EXPECT_EQ(dialog->descriptionPath(), movedJson);
  EXPECT_EQ(saves, 1);
}

TEST_F(CustomOperatorEditDialogTest, renaming_a_script_only_operator)
{
  ASSERT_TRUE(QFile::remove(jsonPath));
  auto dialog = open(QString());
  stemEdit(dialog.get())->setText("Sharpen");
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());
  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);

  EXPECT_EQ(filesInDir(), QStringList{ "Sharpen.py" });
  EXPECT_EQ(dialog->descriptionPath(), dir.path() + "/Sharpen.json");
}

TEST_F(CustomOperatorEditDialogTest, case_only_rename_is_allowed)
{
  auto dialog = open(jsonPath);
  stemEdit(dialog.get())->setText("blur");
  EXPECT_FALSE(stemIssue(dialog.get())->isVisible());
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());
  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);

  EXPECT_EQ(filesInDir(), (QStringList{ "blur.json", "blur.py" }));
  EXPECT_EQ(readFile(dir.path() + "/blur.py"), kScript);
}

TEST_F(CustomOperatorEditDialogTest, name_of_another_operator_blocks_save)
{
  ASSERT_TRUE(writeFile(dir.path() + "/Other.py", "pass\n"));
  auto dialog = open(jsonPath);

  stemEdit(dialog.get())->setText("Other");
  EXPECT_FALSE(saveButton(dialog.get())->isEnabled());
  EXPECT_TRUE(stemIssue(dialog.get())->isVisible());
  EXPECT_TRUE(stemIssue(dialog.get())->text().contains("Other"));

  // A stray description with that name counts too.
  ASSERT_TRUE(writeFile(dir.path() + "/Stray.json", "{}"));
  stemEdit(dialog.get())->setText("Stray");
  EXPECT_FALSE(saveButton(dialog.get())->isEnabled());

  stemEdit(dialog.get())->setText("Blur");
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());
  EXPECT_FALSE(stemIssue(dialog.get())->isVisible());
  EXPECT_EQ(saves, 0);
}

TEST_F(CustomOperatorEditDialogTest, file_name_is_sanitised)
{
  auto dialog = open(jsonPath);
  auto* edit = stemEdit(dialog.get());

  // Typed characters outside the allowed set are refused outright.
  edit->clear();
  QTest::keyClicks(edit, "my op.v2!");
  EXPECT_EQ(edit->text(), "myopv2");
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());

  // Text that arrives some other way still has to pass.
  edit->setText("bad name");
  EXPECT_FALSE(saveButton(dialog.get())->isEnabled());
  EXPECT_TRUE(stemIssue(dialog.get())->isVisible());

  edit->clear();
  EXPECT_FALSE(saveButton(dialog.get())->isEnabled());

  edit->setText("fine_name-2");
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());
}

// --- drafts: create new, clone, save as ---------------------------------

TEST_F(CustomOperatorEditDialogTest, draft_touches_nothing_until_saved)
{
  auto dialog = openDraft("Sharpen", kScript, kSource);
  EXPECT_FALSE(dialog->isBacked());
  EXPECT_EQ(stemEdit(dialog.get())->text(), "Sharpen");
  EXPECT_EQ(dialog->scriptPath(), dir.path() + "/Sharpen.py");
  EXPECT_EQ(scriptEditor(dialog.get())->toPlainText(), kScript);
  EXPECT_EQ(jsonEditor(dialog.get())->toPlainText(), kSource);
  EXPECT_EQ(filesInDir(), (QStringList{ "Blur.json", "Blur.py" }));
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());

  // The name can still change before anything exists.
  stemEdit(dialog.get())->setText("Sharpen2");
  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);
  EXPECT_TRUE(dialog->isBacked());
  EXPECT_EQ(saves, 1);
  EXPECT_EQ(filesInDir(), (QStringList{ "Blur.json", "Blur.py",
                                        "Sharpen2.json", "Sharpen2.py" }));
  EXPECT_EQ(readFile(dir.path() + "/Sharpen2.py"), kScript);
  EXPECT_EQ(readFile(dir.path() + "/Sharpen2.json"), kSource);
  EXPECT_EQ(dialog->scriptPath(), dir.path() + "/Sharpen2.py");
}

TEST_F(CustomOperatorEditDialogTest, draft_may_not_take_an_existing_name)
{
  auto dialog = openDraft("Blur", kScript, kSource);
  EXPECT_FALSE(saveButton(dialog.get())->isEnabled());
  EXPECT_TRUE(stemIssue(dialog.get())->isVisible());

  stemEdit(dialog.get())->setText("Fresh");
  EXPECT_TRUE(saveButton(dialog.get())->isEnabled());
  EXPECT_EQ(saves, 0);
}

TEST_F(CustomOperatorEditDialogTest, script_only_draft_writes_no_description)
{
  auto dialog = openDraft("Bare", kScript, QString());
  ASSERT_TRUE(saveButton(dialog.get())->isEnabled());
  saveButton(dialog.get())->click();
  EXPECT_EQ(dialog->result(), QDialog::Accepted);
  EXPECT_EQ(filesInDir(), (QStringList{ "Bare.py", "Blur.json", "Blur.py" }));
}

TEST_F(CustomOperatorEditDialogTest, unique_stem_skips_taken_names)
{
  int number = 0;
  EXPECT_EQ(uniqueOperatorStem(dir.path(), "Fresh", &number), "Fresh");
  EXPECT_EQ(number, 1);

  // Blur.py and Blur.json exist; a lone json counts as taken too.
  EXPECT_EQ(uniqueOperatorStem(dir.path(), "Blur", &number), "Blur2");
  EXPECT_EQ(number, 2);
  ASSERT_TRUE(writeFile(dir.path() + "/Blur2.json", "{}"));
  EXPECT_EQ(uniqueOperatorStem(dir.path(), "Blur", &number), "Blur3");
  EXPECT_EQ(number, 3);
}

TEST_F(CustomOperatorEditDialogTest, stems_are_sanitised_and_never_empty)
{
  EXPECT_EQ(sanitizedOperatorStem("my op.v2!"), "myopv2");
  EXPECT_EQ(sanitizedOperatorStem("Fine_name-2"), "Fine_name-2");
  EXPECT_EQ(sanitizedOperatorStem(QString::fromUtf8("caf\xC3\xA9 au lait")),
            "cafaulait");
  EXPECT_EQ(uniqueOperatorStem(dir.path(), "my op.v2!"), "myopv2");
  EXPECT_EQ(uniqueOperatorStem(dir.path(), "!!!"), "CustomTransform");
}

TEST_F(CustomOperatorEditDialogTest, copies_are_marked_in_name_and_label)
{
  QJsonObject first = parse(markDescriptionAsCopy(kTransform, 1));
  EXPECT_EQ(first.value("name").toString(), "BlurCopy");
  EXPECT_EQ(first.value("label").toString(), "Blur (copy)");
  // Everything else survives.
  EXPECT_EQ(first.value("parameters").toArray().size(), 1);

  QJsonObject third = parse(markDescriptionAsCopy(kTransform, 3));
  EXPECT_EQ(third.value("name").toString(), "BlurCopy3");
  EXPECT_EQ(third.value("label").toString(), "Blur (copy 3)");

  // A description without a label is told apart by its file name; no
  // label is invented for it.
  QJsonObject unlabeled = parse(markDescriptionAsCopy(kSource, 1));
  EXPECT_EQ(unlabeled.value("name").toString(), "NoiseCopy");
  EXPECT_FALSE(unlabeled.contains("label"));

  // Not JSON (a script-only operator has none): untouched.
  EXPECT_TRUE(markDescriptionAsCopy(QString(), 1).isEmpty());
  EXPECT_EQ(markDescriptionAsCopy("{ nope", 1), "{ nope");
}

TEST_F(CustomOperatorEditDialogTest, identity_helper_only_sets_present_keys)
{
  QJsonObject renamed =
    parse(withDescriptionIdentity(kTransform, "Fresh", "Fresh Label"));
  EXPECT_EQ(renamed.value("name").toString(), "Fresh");
  EXPECT_EQ(renamed.value("label").toString(), "Fresh Label");

  QJsonObject partial = parse(withDescriptionIdentity(kSource, "Fresh", "L"));
  EXPECT_EQ(partial.value("name").toString(), "Fresh");
  EXPECT_FALSE(partial.contains("label"));

  EXPECT_EQ(withDescriptionIdentity("not json", "a", "b"), "not json");
}

TEST_F(CustomOperatorEditDialogTest,
       node_editor_offers_save_as_custom_transform)
{
  // The button lives on the node editor's action bar and hands the
  // editor's current script and description to a draft in the user
  // directory; nothing is written until that draft is saved.
  ScopedEnv guard("TOMVIZ_USER_DIRECTORY");
  QTemporaryDir userDir;
  ASSERT_TRUE(userDir.isValid());
  qputenv("TOMVIZ_USER_DIRECTORY", userDir.path().toLocal8Bit());

  const QString original = tomviz::readInJSONDescription("GaussianFilter");
  ASSERT_FALSE(original.isEmpty());
  const QString expectedStem =
    sanitizedOperatorStem(parse(original).value("name").toString());
  ASSERT_FALSE(expectedStem.isEmpty());

  tomviz::pipeline::Pipeline pipeline;
  auto* node = new tomviz::pipeline::LegacyPythonTransform();
  node->setJSONDescription(original);
  const QString script = "def transform(dataset):\n    pass\n";
  node->setScript(script);
  pipeline.addNode(node);

  tomviz::pipeline::NodeEditDialog editor(node, &pipeline);
  editor.show();
  auto* button = editor.findChild<QPushButton*>("saveAsCustomOperatorButton");
  ASSERT_NE(button, nullptr);
  button->click();

  // The node editor is done once the draft exists, as if cancelled.
  EXPECT_FALSE(editor.isVisible());
  EXPECT_EQ(editor.result(), QDialog::Rejected);

  CustomOperatorEditDialog* draft = nullptr;
  for (QWidget* widget : QApplication::topLevelWidgets()) {
    if (auto* candidate = qobject_cast<CustomOperatorEditDialog*>(widget)) {
      draft = candidate;
      break;
    }
  }
  ASSERT_NE(draft, nullptr);
  EXPECT_FALSE(draft->isBacked());
  EXPECT_EQ(stemEdit(draft)->text(), expectedStem);
  EXPECT_EQ(scriptEditor(draft)->toPlainText(), script);
  // A text edit stores line breaks as "\n" whatever the file used (a
  // Windows checkout ships the description with "\r\n").
  EXPECT_EQ(jsonEditor(draft)->toPlainText(),
            QString(original).replace("\r\n", "\n"));
  EXPECT_TRUE(QDir(userDir.path()).entryList(QDir::Files).isEmpty());

  ASSERT_TRUE(saveButton(draft)->isEnabled());
  saveButton(draft)->click();
  EXPECT_EQ(QDir(userDir.path()).entryList(QDir::Files, QDir::Name),
            (QStringList{ expectedStem + ".json", expectedStem + ".py" }));
  EXPECT_EQ(readFile(userDir.path() + "/" + expectedStem + ".py"), script);
  // The draft deletes itself on close; let that happen before the
  // temporary directory goes away.
  QTest::qWait(10);
}
