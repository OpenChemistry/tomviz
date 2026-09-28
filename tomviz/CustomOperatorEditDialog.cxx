/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */
#include "CustomOperatorEditDialog.h"

#include "Utilities.h"
#include "pipeline/NodeDefinitionValidator.h"
#include "pipeline/NodeDefinitionWidget.h"
#include "pipeline/PythonNodeEditorWidget.h"
#include "pipeline/PythonScriptEdit.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>
#include <QVBoxLayout>

namespace tomviz {

namespace {

/// Read @a path as UTF-8 into @a text. On failure @a error says why and
/// @a text is left alone.
bool readTextFile(const QString& path, QString* text, QString* error)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    *error = QObject::tr("Could not read \"%1\":\n%2")
               .arg(QDir::toNativeSeparators(path), file.errorString());
    return false;
  }
  *text = QString::fromUtf8(file.readAll());
  return true;
}

bool isStemCharacter(QChar c)
{
  return (c.unicode() < 128 && c.isLetterOrNumber()) ||
         c == QLatin1Char('_') || c == QLatin1Char('-');
}

} // namespace

QString sanitizedOperatorStem(const QString& text)
{
  QString stem;
  for (QChar c : text) {
    if (isStemCharacter(c)) {
      stem.append(c);
    }
  }
  return stem;
}

QString uniqueOperatorStem(const QString& directory, const QString& base,
                           int* number)
{
  QString stem = sanitizedOperatorStem(base);
  if (stem.isEmpty()) {
    stem = QStringLiteral("CustomTransform");
  }
  const QDir dir(directory);
  auto taken = [&dir](const QString& candidate) {
    return QFile::exists(dir.filePath(candidate + QStringLiteral(".py"))) ||
           QFile::exists(dir.filePath(candidate + QStringLiteral(".json")));
  };
  int n = 1;
  QString candidate = stem;
  while (taken(candidate)) {
    ++n;
    candidate = stem + QString::number(n);
  }
  if (number) {
    *number = n;
  }
  return candidate;
}

QString withDescriptionIdentity(const QString& description,
                                const QString& name, const QString& label)
{
  const QJsonDocument doc = QJsonDocument::fromJson(description.toUtf8());
  if (!doc.isObject()) {
    return description;
  }
  QJsonObject obj = doc.object();
  if (!name.isEmpty() && obj.contains(QStringLiteral("name"))) {
    obj[QStringLiteral("name")] = name;
  }
  if (!label.isEmpty() && obj.contains(QStringLiteral("label"))) {
    obj[QStringLiteral("label")] = label;
  }
  return QString::fromUtf8(QJsonDocument(obj).toJson());
}

QString markDescriptionAsCopy(const QString& description, int number)
{
  const QJsonDocument doc = QJsonDocument::fromJson(description.toUtf8());
  if (!doc.isObject()) {
    return description;
  }
  const QJsonObject obj = doc.object();
  QString name = obj.value(QStringLiteral("name")).toString();
  if (!name.isEmpty()) {
    name += QStringLiteral("Copy");
    if (number > 1) {
      name += QString::number(number);
    }
  }
  QString label = obj.value(QStringLiteral("label")).toString();
  if (!label.isEmpty()) {
    label += number > 1 ? QStringLiteral(" (copy %1)").arg(number)
                        : QStringLiteral(" (copy)");
  }
  return withDescriptionIdentity(description, name, label);
}

using pipeline::DefinitionTarget;
using pipeline::NodeDefinitionWidget;
using pipeline::PythonScriptEdit;

CustomOperatorEditDialog::CustomOperatorEditDialog(
  const QString& label, const QString& scriptPath,
  const QString& descriptionPath, QWidget* parent)
  : QDialog(parent), m_backed(true), m_scriptPath(scriptPath),
    m_descriptionPath(
      descriptionPath.isEmpty()
        ? pipeline::PythonNodeEditorWidget::descriptionPathFor(scriptPath)
        : descriptionPath)
{
  setWindowTitle(tr("Edit Custom Node - %1").arg(label));

  QString script;
  if (!readTextFile(m_scriptPath, &script, &m_loadError)) {
    return;
  }
  // No description file is a valid state (a script-only operator); an
  // unreadable one is not, since saving would replace it.
  QString description;
  if (QFile::exists(m_descriptionPath) &&
      !readTextFile(m_descriptionPath, &description, &m_loadError)) {
    return;
  }
  build(script, description);
}

CustomOperatorEditDialog::CustomOperatorEditDialog(
  const CustomOperatorDraft& draft, QWidget* parent)
  : QDialog(parent), m_backed(false),
    m_scriptPath(
      QDir(draft.directory).filePath(draft.stem + QStringLiteral(".py"))),
    m_descriptionPath(
      QDir(draft.directory).filePath(draft.stem + QStringLiteral(".json")))
{
  setWindowTitle(tr("New Custom Node"));
  build(draft.script, draft.description);
}

void CustomOperatorEditDialog::build(const QString& script,
                                     const QString& description)
{
  // Like the node editor: float above the main window rather than slip
  // behind it while the user consults the pipeline.
  floatAboveMainWindow(this);

  auto* layout = new QVBoxLayout(this);

  // File name: one stem for both files, renamed (or created) together.
  const QString directory =
    QDir::toNativeSeparators(QFileInfo(m_scriptPath).absolutePath());
  auto* nameRow = new QHBoxLayout;
  nameRow->addWidget(new QLabel(tr("File name"), this));
  m_stemEdit = new QLineEdit(currentStem(), this);
  m_stemEdit->setObjectName(QStringLiteral("operatorStemEdit"));
  // Letters, digits, underscore and hyphen: a name no filesystem, shell
  // or menu scan trips over. Typing anything else is simply refused.
  m_stemEdit->setValidator(new QRegularExpressionValidator(
    QRegularExpression(QStringLiteral("[A-Za-z0-9_-]+")), m_stemEdit));
  m_stemEdit->setToolTip(
    m_backed ? tr("Both files are renamed on save. They live in %1.")
                 .arg(directory)
             : tr("Both files are created on save, in %1.").arg(directory));
  nameRow->addWidget(m_stemEdit, 1);
  auto* suffixes = new QLabel(tr(".py and .json"), this);
  suffixes->setStyleSheet("QLabel { color: palette(mid); }");
  nameRow->addWidget(suffixes);
  layout->addLayout(nameRow);

  m_stemIssue = new QLabel(this);
  m_stemIssue->setObjectName(QStringLiteral("operatorStemIssue"));
  m_stemIssue->setWordWrap(true);
  m_stemIssue->hide();
  layout->addWidget(m_stemIssue);

  m_tabs = new QTabWidget(this);
  // Shape and schema only seed the form's initial rendering; as a file
  // the description is free to change either.
  m_definition = new NodeDefinitionWidget(
    description, pipeline::definitionShape(description),
    pipeline::definitionSchema(description), DefinitionTarget::File,
    m_tabs);
  m_tabs->addTab(m_definition, tr("Definition"));
  m_script = new PythonScriptEdit(m_tabs);
  m_script->setPlainText(script);
  m_tabs->addTab(m_script, tr("Script"));
  layout->addWidget(m_tabs, 1);

  m_buttons = new QDialogButtonBox(
    QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  layout->addWidget(m_buttons);
  connect(m_buttons, &QDialogButtonBox::accepted, this,
          &CustomOperatorEditDialog::save);
  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_definition, &NodeDefinitionWidget::validityChanged, this,
          &CustomOperatorEditDialog::updateSaveEnabled);
  connect(m_stemEdit, &QLineEdit::textChanged, this,
          &CustomOperatorEditDialog::updateSaveEnabled);
  updateSaveEnabled();

  resize(760, 600);
}

QString CustomOperatorEditDialog::currentStem() const
{
  // completeBaseName() so "my.operator.py" keeps its dots, the same way
  // descriptionPathFor() pairs the files.
  return QFileInfo(m_scriptPath).completeBaseName();
}

QString CustomOperatorEditDialog::stemIssue() const
{
  const QString stem = m_stemEdit->text();
  if (stem.isEmpty() || !m_stemEdit->hasAcceptableInput()) {
    return tr("The file name may contain letters, digits, \"_\" and \"-\" "
              "only.");
  }
  if (m_backed && stem == currentStem()) {
    return QString();
  }

  // Anything already at the target is another operator, except this
  // operator's own file under another spelling of its case.
  const QDir dir = QFileInfo(m_scriptPath).dir();
  auto taken = [this, &dir, &stem](const QString& suffix,
                                   const QString& own) {
    const QString target = dir.filePath(stem + suffix);
    return QFile::exists(target) &&
           (!m_backed || target.compare(own, Qt::CaseInsensitive) != 0);
  };
  if (taken(QStringLiteral(".py"), m_scriptPath) ||
      taken(QStringLiteral(".json"), m_descriptionPath)) {
    return tr("An operator named \"%1\" already exists in this directory.")
      .arg(stem);
  }
  return QString();
}

void CustomOperatorEditDialog::updateSaveEnabled()
{
  const QString issue = stemIssue();
  m_stemIssue->setText(
    QStringLiteral("<b>%1</b>").arg(issue.toHtmlEscaped()));
  m_stemIssue->setVisible(!issue.isEmpty());
  m_buttons->button(QDialogButtonBox::Save)
    ->setEnabled(m_definition->isValid() && issue.isEmpty());
}

bool CustomOperatorEditDialog::renameTo(const QString& stem)
{
  const QDir dir = QFileInfo(m_scriptPath).dir();
  const QString newScript = dir.filePath(stem + QStringLiteral(".py"));
  const QString newDescription = dir.filePath(stem + QStringLiteral(".json"));

  // QFile::rename never overwrites, and knows how to change only the
  // case of a name on a case-insensitive filesystem.
  auto move = [this](const QString& from, const QString& to) {
    if (QFile::rename(from, to)) {
      return true;
    }
    QMessageBox::warning(this, tr("Rename Custom Node"),
                         tr("Could not rename \"%1\" to \"%2\".")
                           .arg(QDir::toNativeSeparators(from),
                                QDir::toNativeSeparators(to)));
    return false;
  };
  if (!move(m_scriptPath, newScript)) {
    return false;
  }
  if (QFile::exists(m_descriptionPath) &&
      !move(m_descriptionPath, newDescription)) {
    // Keep the pair together: put the script back where it was.
    QFile::rename(newScript, m_scriptPath);
    return false;
  }
  m_scriptPath = newScript;
  m_descriptionPath = newDescription;
  return true;
}

void CustomOperatorEditDialog::save()
{
  // Flush so a keystroke typed a moment ago can't slip past validation.
  m_definition->flushPendingValidation();
  if (!m_definition->isValid()) {
    QMessageBox::warning(
      this, tr("Invalid node definition"),
      tr("The description in the Definition tab has errors, so it can't be "
         "saved. Fix the problems listed there, or revert the edit."));
    return;
  }
  // The live check keeps Save disabled on a bad name; this is the guard
  // against a file that appeared since.
  const QString issue = stemIssue();
  if (!issue.isEmpty()) {
    QMessageBox::warning(this, tr("Save Custom Node"), issue);
    return;
  }

  const QString stem = m_stemEdit->text();
  if (m_backed) {
    // Rename before writing, so a case-only change on a case-insensitive
    // filesystem never writes and then deletes the same file.
    if (stem != currentStem() && !renameTo(stem)) {
      return;
    }
  } else {
    const QDir dir = QFileInfo(m_scriptPath).dir();
    m_scriptPath = dir.filePath(stem + QStringLiteral(".py"));
    m_descriptionPath = dir.filePath(stem + QStringLiteral(".json"));
  }

  if (!writeTextFile(this, m_scriptPath, m_script->toPlainText())) {
    return;
  }
  // From here the operator is on disk, so a retry after a failure below
  // is a plain re-save rather than a draft colliding with its own file.
  m_backed = true;
  // The validator refuses to blank out a description that exists, so an
  // empty one here means the operator never had a file and stays without.
  const QString description = m_definition->definitionText();
  if (!description.trimmed().isEmpty() &&
      !writeTextFile(this, m_descriptionPath, description)) {
    return;
  }

  emit saved();
  accept();
}

} // namespace tomviz
