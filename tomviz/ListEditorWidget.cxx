/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ListEditorWidget.h"

#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace tomviz {

ListEditorWidget::ListEditorWidget(const QStringList& list, QWidget* parent)
  : QListWidget(parent), m_originalList(list)
{
  // Allow the rows to be moved internally for reordering
  setDragDropMode(QAbstractItemView::InternalMove);

  // Allow the rows to be edited
  for (int i = 0; i < list.size(); ++i) {
    const auto& text = list[i];
    auto* item = new QListWidgetItem(text, this);
    // Store the original index in the data
    item->setData(Qt::UserRole, i);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    addItem(item);
  }
}

QList<int> ListEditorWidget::currentOrder() const
{
  QList<int> ret;
  for (int i = 0; i < count(); ++i) {
    ret.append(item(i)->data(Qt::UserRole).toInt());
  }
  return ret;
}

QStringList ListEditorWidget::currentNames() const
{
  QStringList ret;
  for (int i = 0; i < count(); ++i) {
    ret.append(item(i)->text());
  }
  return ret;
}

ListEditorDialog::ListEditorDialog(const QStringList& list, QWidget* parent)
  : QDialog(parent), m_listEditor(new ListEditorWidget(list, this))
{
  // Resize based upon the list editor widget contents
  auto* editor = m_listEditor;
  auto width = editor->sizeHintForColumn(0) + 2 * editor->frameWidth() + 30;
  auto height = (editor->sizeHintForRow(0) + 8) * editor->count() +
                2 * editor->frameWidth();

  width = std::min(width, 600);
  height = std::min(height, 600);
  resize(QSize(width, height));

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(editor);

  auto buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
  auto* buttonBox = new QDialogButtonBox(buttons, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttonBox);

  setLayout(layout);
}

} // namespace tomviz
