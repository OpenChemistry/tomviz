/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSelectItemsDialog_h
#define tomvizSelectItemsDialog_h

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;

namespace tomviz {

class SelectItemsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SelectItemsDialog(QStringList items, QWidget* parent);

  QStringList selectedItems() const;

  QList<bool> selections() const;
  void setSelections(QList<bool> selections);

private:
  QStringList m_items;
  QList<QCheckBox*> m_checkboxes;
};
} // namespace tomviz

#endif
