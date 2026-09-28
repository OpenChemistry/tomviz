/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorSearchDialog.h"

#include "Utilities.h"

#include <QAction>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace tomviz {

OperatorSearchDialog::OperatorSearchDialog(QWidget* parent)
  : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
  setWindowTitle("Search Operators");
  setMinimumSize(450, 550);

  auto* layout = new QVBoxLayout(this);

  m_searchBox = new QLineEdit(this);
  m_searchBox->setPlaceholderText("Type to search");
  m_searchBox->setClearButtonEnabled(true);
  layout->addWidget(m_searchBox);

  m_createButton = new QPushButton(this);
  m_createButton->setText("(no selection)");
  m_createButton->setToolTip("Create the selected operator");
  layout->addWidget(m_createButton);

  m_availableList = new QListWidget(this);
  m_availableList->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Expanding);
  layout->addWidget(m_availableList);

  m_descriptionLabel = new QLabel(this);
  m_descriptionLabel->setWordWrap(true);
  m_descriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_descriptionLabel->setMinimumHeight(40);
  layout->addWidget(m_descriptionLabel);

  m_unavailableLabel = new QLabel("Unavailable:", this);
  m_unavailableLabel->setToolTip(
    "Select one to see its description. These operators require "
    "an active data source to be enabled.");
  layout->addWidget(m_unavailableLabel);

  m_unavailableList = new QListWidget(this);
  m_unavailableList->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Maximum);
  m_unavailableList->setMaximumHeight(150);
  layout->addWidget(m_unavailableList);

  m_instructionsLabel = new QLabel(
    "Press Enter to create.\nEsc to cancel.", this);
  layout->addWidget(m_instructionsLabel);

  connect(m_searchBox, &QLineEdit::textChanged, this,
          &OperatorSearchDialog::filterOperators);
  connect(m_createButton, &QPushButton::clicked, this,
          &OperatorSearchDialog::activateCurrentProxy);
  connect(m_availableList, &QListWidget::currentItemChanged, this,
          &OperatorSearchDialog::availableItemChanged);
  connect(m_unavailableList, &QListWidget::currentItemChanged, this,
          &OperatorSearchDialog::unavailableItemChanged);
  connect(m_availableList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem*) { activateCurrentProxy(); });
}

void OperatorSearchDialog::addOperatorAction(QAction* action,
                                             const QString& category,
                                             const QString& description)
{
  OperatorEntry entry;
  entry.action = action;
  entry.name = action->text().remove('&');
  entry.category = category;
  entry.description = description;
  m_operators.append(entry);
}

void OperatorSearchDialog::collectActionsFromMenu(QMenu* menu,
                                                  const QString& prefix)
{
  for (auto* action : menu->actions()) {
    if (action->menu()) {
      QString submenuTitle = action->menu()->title().remove('&');
      QString newPrefix = prefix.isEmpty()
                            ? submenuTitle
                            : prefix + " > " + submenuTitle;
      collectActionsFromMenu(action->menu(), newPrefix);
    } else if (!action->isSeparator()) {
      // Prefer statusTip (set from JSON description) over toolTip
      QString desc = action->statusTip();
      if (desc.isEmpty()) {
        QString tip = action->toolTip();
        // Qt often auto-sets toolTip to the action text; skip those
        if (tip != action->text().remove('&')) {
          desc = tip;
        }
      }
      addOperatorAction(action, prefix, desc);
    }
  }
}

void OperatorSearchDialog::removeCategory(const QString& category)
{
  m_operators.erase(std::remove_if(m_operators.begin(), m_operators.end(),
                                   [&category](const OperatorEntry& entry) {
                                     return entry.category == category;
                                   }),
                    m_operators.end());
}

void OperatorSearchDialog::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  m_searchBox->clear();
  m_searchBox->setFocus();
  rebuildLists();
}

void OperatorSearchDialog::filterOperators(const QString& text)
{
  m_availableList->clear();
  m_unavailableList->clear();
  m_createButton->setText("(no selection)");
  m_createButton->setEnabled(false);
  m_descriptionLabel->clear();

  QString filter = text.trimmed();

  // Build name counts for duplicate detection
  QHash<QString, int> nameCounts;
  for (const auto& entry : m_operators) {
    if (!entry.action) {
      continue;
    }
    QString name = entry.action->text().remove('&');
    nameCounts[name]++;
  }

  for (int i = 0; i < m_operators.size(); ++i) {
    const auto& entry = m_operators[i];
    if (!entry.action) {
      continue;
    }
    QString currentName = entry.action->text().remove('&');
    if (!filter.isEmpty() &&
        !currentName.contains(filter, Qt::CaseInsensitive) &&
        !entry.category.contains(filter, Qt::CaseInsensitive)) {
      continue;
    }

    // Only disambiguate with category if there are duplicate names
    QString displayText = currentName;
    if (nameCounts.value(currentName) > 1 && !entry.category.isEmpty()) {
      displayText = currentName + "  (" + entry.category + ")";
    }

    auto* item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, i);

    if (entry.action->isEnabled()) {
      m_availableList->addItem(item);
    } else {
      item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
      m_unavailableList->addItem(item);
    }
  }

  // Select first available item, or first unavailable if none available
  if (m_availableList->count() > 0) {
    m_availableList->setCurrentRow(0);
  } else if (m_unavailableList->count() > 0) {
    m_unavailableList->setCurrentRow(0);
  }
}

void OperatorSearchDialog::availableItemChanged(QListWidgetItem* current,
                                                QListWidgetItem*)
{
  if (current) {
    // Clear selection in the other list
    m_unavailableList->clearSelection();
    m_unavailableList->setCurrentItem(nullptr);
    updateSelection(current);
    m_createButton->setEnabled(true);
  }
}

void OperatorSearchDialog::unavailableItemChanged(QListWidgetItem* current,
                                                  QListWidgetItem*)
{
  if (current) {
    // Clear selection in the other list
    m_availableList->clearSelection();
    m_availableList->setCurrentItem(nullptr);
    updateSelection(current);
    m_createButton->setEnabled(false);
  }
}

void OperatorSearchDialog::updateSelection(QListWidgetItem* item)
{
  if (!item) {
    return;
  }

  int index = item->data(Qt::UserRole).toInt();
  if (index < 0 || index >= m_operators.size()) {
    return;
  }

  const auto& entry = m_operators[index];
  if (!entry.action) {
    return;
  }
  QString currentName = entry.action->text().remove('&');
  m_createButton->setText(currentName);

  // Prefer live statusTip (set from JSON), fall back to stored description
  QString desc = entry.action->statusTip();
  if (desc.isEmpty()) {
    desc = entry.description;
  }
  m_descriptionLabel->setText(desc);
}

void OperatorSearchDialog::activateCurrentProxy()
{
  auto* item = m_availableList->currentItem();
  if (!item) {
    return;
  }

  int index = item->data(Qt::UserRole).toInt();
  if (index >= 0 && index < m_operators.size() &&
      m_operators[index].action && m_operators[index].action->isEnabled()) {
    accept();
    m_operators[index].action->trigger();
  }
}

QListWidget* OperatorSearchDialog::currentList()
{
  if (m_unavailableList->currentItem() &&
      !m_availableList->currentItem()) {
    return m_unavailableList;
  }
  return m_availableList;
}

void OperatorSearchDialog::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    activateCurrentProxy();
    return;
  } else if (event->key() == Qt::Key_Escape) {
    if (m_searchBox->text().isEmpty()) {
      reject();
    } else {
      m_searchBox->clear();
    }
    return;
  } else if (event->key() == Qt::Key_Down) {
    QListWidget* list = currentList();
    int row = list->currentRow();
    if (row < list->count() - 1) {
      list->setCurrentRow(row + 1);
    } else if (list == m_availableList && m_unavailableList->count() > 0) {
      // Move from available to unavailable list
      m_unavailableList->setCurrentRow(0);
    }
    return;
  } else if (event->key() == Qt::Key_Up) {
    QListWidget* list = currentList();
    int row = list->currentRow();
    if (row > 0) {
      list->setCurrentRow(row - 1);
    } else if (list == m_unavailableList && m_availableList->count() > 0) {
      // Move from unavailable back to available list
      m_availableList->setCurrentRow(m_availableList->count() - 1);
    }
    return;
  } else if (event->key() == Qt::Key_Tab) {
    // Toggle between lists
    QListWidget* list = currentList();
    if (list == m_availableList && m_unavailableList->count() > 0) {
      m_unavailableList->setCurrentRow(0);
    } else if (list == m_unavailableList && m_availableList->count() > 0) {
      m_availableList->setCurrentRow(0);
    }
    return;
  }

  // Forward unhandled text keys to the search box
  if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
    m_searchBox->setFocus();
    m_searchBox->setText(m_searchBox->text() + event->text());
    return;
  }

  QDialog::keyPressEvent(event);
}

void OperatorSearchDialog::rebuildLists()
{
  filterOperators(m_searchBox->text());
}

} // namespace tomviz
