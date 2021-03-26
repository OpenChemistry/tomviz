/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizComboTextEditor_h
#define tomvizComboTextEditor_h

#include <QComboBox>

namespace tomviz {

class ComboTextEditor : public QComboBox
{
  /***
  * The ComboTextEditor is a QComboBox tweaked to be used specifically
  * as a text editor for a list of texts. A regular QComboBox is not
  * very friendly for this sort of thing. For instance, if a user enters
  * the name of another item on the list, the default behavior for
  * QComboBox is to change the index to that item. We'd rather not
  * change the index, but just revert back to the original name if the
  * new name is invalid.
  *
  * When an item is finished editing, the "itemEdited" signal
  * gets emitted.
  ***/
  Q_OBJECT

public:
  ComboTextEditor(QWidget* parent = nullptr);

  QStringList items() const;

  bool eventFilter(QObject* watched, QEvent* event) override;

signals:
  void itemEdited(int index, const QString& text);

private:
  void onEditingFinished();
};

} // namespace tomviz

#endif // tomvizComboTextEditor_h
