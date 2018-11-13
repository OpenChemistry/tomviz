/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RegexGroupsWidget.h"

#include "ui_RegexGroupsWidget.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"
#include "RegexGroupDialog.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <QListWidget>
#include <QMenu>

#include <algorithm>

namespace tomviz {

RegexGroupsWidget::RegexGroupsWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::RegexGroupsWidget)
{
  m_ui->setupUi(this);

  readSettings();

  // New
  connect(m_ui->newRegexGroupButton, &QPushButton::clicked, [this]() {
    RegexGroupDialog dialog;
    auto r = dialog.exec();
    if (r != QDialog::Accepted || dialog.name().isEmpty()) {
      return;
    }

    if (m_ui->regexGroupsWidget->findItems(dialog.name(), Qt::MatchExactly)
          .isEmpty()) {
      m_ui->regexGroupsWidget->addItem(dialog.name());
    }

    writeSettings();
    emit groupsChanged();
  });

  // Delete
  connect(m_ui->regexGroupsWidget, &QWidget::customContextMenuRequested,
          [this](const QPoint& pos) {
            auto globalPos = m_ui->regexGroupsWidget->mapToGlobal(pos);
            QMenu contextMenu;
            contextMenu.addAction("Delete", this, [this, &pos]() {
              auto item = m_ui->regexGroupsWidget->itemAt(pos);
              delete item;
              writeSettings();
              emit groupsChanged();
            });

            // Show context menu at handling position
            contextMenu.exec(globalPos);
          });
}

RegexGroupsWidget::~RegexGroupsWidget() = default;

void RegexGroupsWidget::readSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");
  auto groups = settings->value("regexGroupNames").toStringList();
  // Add the default
  if (!settings->contains("regexGroupNames")) {
    groups.append("angle");
  }
  foreach (const QString& group, groups) {
    m_ui->regexGroupsWidget->addItem(group);
  }
  settings->endGroup();
}

void RegexGroupsWidget::writeSettings()
{
  auto settings = pqApplicationCore::instance()->settings();
  settings->beginGroup("acquisition");

  QStringList groups;
  for (int i = 0; i < m_ui->regexGroupsWidget->count(); i++) {
    QListWidgetItem* group = m_ui->regexGroupsWidget->item(i);
    groups.append(group->text());
  }
  settings->setValue("regexGroupNames", groups);
  settings->endGroup();
}

QStringList RegexGroupsWidget::regexGroups()
{
  QStringList groups;
  for (int i = 0; i < m_ui->regexGroupsWidget->count(); i++) {
    QListWidgetItem* group = m_ui->regexGroupsWidget->item(i);
    groups.append(group->text());
  }

  return groups;
}
} // namespace tomviz
