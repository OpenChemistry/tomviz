/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizListEditorWidget_h
#define tomvizListEditorWidget_h

#include <QDialog>
#include <QListWidget>

namespace tomviz {

class ListEditorWidget : public QListWidget
{
  Q_OBJECT

public:
  ListEditorWidget(const QStringList& list, QWidget* parent = nullptr);

  // Get the new ordering for the names
  QList<int> currentOrder() const;

  // Get the names in the order that they appear in the widget
  QStringList currentNames() const;

private:
  QStringList m_originalList;
};

// A dialog containing only the ListEditorWidget
class ListEditorDialog : public QDialog
{
  Q_OBJECT

public:
  ListEditorDialog(const QStringList& list, QWidget* parent = nullptr);

  QList<int> currentOrder() const { return m_listEditor->currentOrder(); }
  QStringList currentNames() const { return m_listEditor->currentNames(); }

private:
  ListEditorWidget* m_listEditor;
};

} // namespace tomviz

#endif
