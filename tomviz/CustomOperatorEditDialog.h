/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */
#ifndef tomvizCustomOperatorEditDialog_h
#define tomvizCustomOperatorEditDialog_h

#include <QDialog>
#include <QString>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTabWidget;

namespace tomviz {

namespace pipeline {
class NodeDefinitionWidget;
class PythonScriptEdit;
} // namespace pipeline

/// A custom operator that exists only in memory until it is saved as
/// <directory>/<stem>.py and <stem>.json: what "Create New", "Clone" and
/// "Save as Custom Transform" hand to the dialog.
struct CustomOperatorDraft
{
  QString directory;
  QString stem;
  QString script;
  QString description;
};

/// @a text reduced to the characters an operator file stem may contain:
/// letters, digits, "_" and "-".
QString sanitizedOperatorStem(const QString& text);

/// A stem no operator in @a directory uses yet: @a base (sanitized, or
/// "CustomTransform" if nothing is left of it), else base2, base3, ...
/// The number picked (1 for the bare base) is written to @a number when
/// given, so a caller can mark the name and label the same way.
QString uniqueOperatorStem(const QString& directory, const QString& base,
                           int* number = nullptr);

/// @a description with "name" and "label" replaced, when present in the
/// text and non-empty themselves. Text that is not a JSON object comes
/// back untouched.
QString withDescriptionIdentity(const QString& description,
                                const QString& name, const QString& label);

/// @a description marked as copy number @a number of its original: the
/// name gains "Copy" (then "Copy2", ...), the label " (copy)" (then
/// " (copy 2)", ...). Keys the description lacks are left out, so an
/// operator labelled by its file name is told apart by the stem alone.
QString markDescriptionAsCopy(const QString& description, int number);

/// Edits the two files that define a custom operator in the user's
/// directory, in place: a "Definition" tab for the JSON description and
/// a "Script" tab for the Python source. Both tabs are the same widgets
/// the Python node editor uses, with the definition edited as a file
/// rather than as a live node, so schema, shape and ports may change.
///
/// A file name field above the tabs names both files. The name is
/// restricted to letters, digits, "_" and "-", and may not collide with
/// another operator in the directory; Save stays disabled while it does.
/// For an existing operator a changed name renames the files on save.
///
/// Save writes the script, and the description unless it is empty (a
/// script-only operator stays that way until a description is typed).
/// Nothing touches the disk before Save; Cancel discards the edit. A
/// dialog opened on a CustomOperatorDraft has no files at all until then.
class CustomOperatorEditDialog : public QDialog
{
  Q_OBJECT

public:
  /// Edit an operator that is on disk. @a descriptionPath may be empty
  /// when the operator has no JSON file yet; the conventional path
  /// beside the script is used then. The files are read here: when that
  /// fails, loadError() says why and the dialog is not usable.
  CustomOperatorEditDialog(const QString& label, const QString& scriptPath,
                           const QString& descriptionPath,
                           QWidget* parent = nullptr);

  /// Start from @a draft; the files come into being on the first Save.
  explicit CustomOperatorEditDialog(const CustomOperatorDraft& draft,
                                    QWidget* parent = nullptr);

  /// Empty when both files were read (or the description is absent).
  QString loadError() const { return m_loadError; }

  /// False for a draft that has not been saved yet.
  bool isBacked() const { return m_backed; }

  /// Current locations: where a draft will land, or where the files are.
  /// They move when a save renames the files.
  QString scriptPath() const { return m_scriptPath; }
  QString descriptionPath() const { return m_descriptionPath; }

signals:
  /// Emitted once both files are on disk, just before the dialog accepts.
  void saved();

private:
  Q_DISABLE_COPY(CustomOperatorEditDialog)

  void build(const QString& script, const QString& description);
  void save();
  void updateSaveEnabled();
  /// The stem the paths currently carry.
  QString currentStem() const;
  /// Why the typed file name can't be used, or empty when it can.
  QString stemIssue() const;
  /// Move both files to @a stem, reporting failure to the user.
  bool renameTo(const QString& stem);

  bool m_backed = false;
  QString m_scriptPath;
  QString m_descriptionPath;
  QString m_loadError;

  QLineEdit* m_stemEdit = nullptr;
  QLabel* m_stemIssue = nullptr;
  QTabWidget* m_tabs = nullptr;
  pipeline::NodeDefinitionWidget* m_definition = nullptr;
  pipeline::PythonScriptEdit* m_script = nullptr;
  QDialogButtonBox* m_buttons = nullptr;
};

} // namespace tomviz

#endif
