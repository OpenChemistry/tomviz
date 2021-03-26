/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ComboTextEditor.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>

namespace tomviz {

ComboTextEditor::ComboTextEditor(QWidget* p) : QComboBox(p)
{
  setEditable(true);

  // Turn off autocomplete for the QComboBox
  // As a text editor, it doesn't make sense for this to auto-complete
  // to other item names, since duplicates not allowed by default.
  setCompleter(nullptr);

  connect(lineEdit(), &QLineEdit::editingFinished, this,
          &ComboTextEditor::onEditingFinished);

  installEventFilter(this);
}

QStringList ComboTextEditor::items() const
{
  QStringList ret;
  for (int i = 0; i < count(); ++i) {
    ret.append(itemText(i));
  }
  return ret;
}

bool ComboTextEditor::eventFilter(QObject* watched, QEvent* event)
{
  if (watched != this) {
    return false;
  }

  static const QList<int> enterKeys = { Qt::Key_Enter, Qt::Key_Return };

  auto keyEvent = dynamic_cast<QKeyEvent*>(event);
  if (keyEvent && enterKeys.contains(keyEvent->key())) {
    // Clear focus when enter/return is pressed.
    lineEdit()->clearFocus();
    return true;
  }

  auto focusEvent = dynamic_cast<QFocusEvent*>(event);
  if (focusEvent && focusEvent->lostFocus()) {
    // This happens if the user presses enter, tabs out, or clicks on
    // something else.
    auto text = currentText();
    auto idx = currentIndex();

    if (items().contains(text) && itemText(idx) != text) {
      // Prevent the QComboBox from automatically changing
      // the index to be that of the other item in the list.
      // This is confusing behavior, and it's not what we
      // want here.
      setCurrentIndex(idx);
      // Let the widget lose focus
      return false;
    }
  }

  return false;
}

void ComboTextEditor::onEditingFinished()
{
  auto text = currentText();
  if (!duplicatesEnabled() && items().contains(text)) {
    // Revert the name back, as duplicate names are not allowed.
    setCurrentText(itemText(currentIndex()));
    return;
  }

  setItemText(currentIndex(), text);
  emit itemEdited(currentIndex(), text);
}

} // namespace tomviz
